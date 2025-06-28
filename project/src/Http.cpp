#include "Http.hpp"
#include "Logger.hpp"
#include "WebServerError.hpp"
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>


Http::Http(const std::shared_ptr<TcpServer>& tcp_server, std::shared_ptr<TcpServer::Connection>& connection, const HttpVersion http_version) :
    tcp_server_(tcp_server),
    tcp_connection_(connection),
    http_version_(http_version),
    content_encoding_(HttpContentEncoding::NONE),
    request_method_(HttpMethod::NOT_ALLOWED),
    status_page_(false),
    temp_file_(nullptr),
    rparam_(nullptr)
{
    
}

void Http::reset()
{
    content_encoding_ = HttpContentEncoding::NONE;
    content_encoding_str_.clear();
    request_method_ = HttpMethod::NOT_ALLOWED;
    request_uri_.clear();
    status_page_ = false;
    temp_file_ = nullptr;
    rparam_ = nullptr;
}


bool Http::endConnection()
{
    bool ret = true;

    if (!tcp_server_->endConnection(tcp_connection_)) {
        ret = false;
    }

    for (const auto& tmpf : temp_files_)
    {
        // Mazu docasny soubor jen pokud byl opravdu vytvoren pro ulozeni tela HTTP requestu
        if (tmpf.second.data_in_temp_file_)
        {
            if (remove(tmpf.second.file_path_.c_str()) == -1) {
                LOG_ERR("Failed to delete temporary file (file: %s)", tmpf.second.file_path_.c_str());
                ret = false;
            }
        }
    }

    if (!temp_files_dir_.empty() && remove(temp_files_dir_.c_str()) == -1) {
        LOG_ERR("Failed to delete temporary files directory (directory: %s)", temp_files_dir_.c_str());
        ret = false;
    }

    return ret;
}


bool Http::storeResource(Config::RParams* rparam_, const Http::TempFile& temp_file, const std::string& rel_path, const char* resource_data, const uint64_t resource_size)
{
    //LOG_DBG("storeResource()...");

    const std::string new_resource_file_path = std::string(RESOURCES_DIR) + rel_path;

    if (temp_file.data_in_temp_file_)
    {
        //LOG_DBG("Storing resource data from temp file...");

        if (!rparam_) {
            return false;
        }
        rparam_->releaseFileLock();

        if (rename(temp_file.file_path_.c_str(), new_resource_file_path.c_str()) == -1)
        {
            LOG_ERR("Failed to move temporary file to resource directory (file: %s)", temp_file.file_path_.c_str());
            this->deleteTemporaryFile(-1, temp_file);
            return false;
        }
        
        chmod(new_resource_file_path.c_str(), 0666);
    }
    else
    {
        //LOG_DBG("Storing resource data from buffer...");

        if (!resource_data || resource_size == 0) {
            return false;
        }

        const int resource_file_fd = open(new_resource_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOCTTY, 0666);
        if (resource_file_fd == -1) {   
            return false;
        }

        write(resource_file_fd, resource_data, resource_size);
        if (fsync(resource_file_fd) == -1) 
        {
            LOG_ERR("Failed to store resource (error: %s)", strerror(errno));
            close(resource_file_fd);
            return false;
        }

        fchmod(resource_file_fd, 0666);
        close(resource_file_fd);
    }

    //LOG_DBG("Resource stored");
    //LOG_DBG("Erasing temp file from temp files...");

    temp_files_.erase(temp_files_.find(temp_file.stream_id_));
    
    //LOG_DBG("Erased");

    return true;
}

bool Http::deleteResource(const std::string& file_name)
{
    const std::string file_path = std::string(RESOURCES_DIR) + file_name;
    if (remove(file_path.c_str()) == -1) {
        LOG_ERR("Failed to delete resource (resource: %s)", file_path.c_str());
        return false;
    }

    return true;
}

bool Http::getResourceParam()
{
    if (request_method_ == HttpMethod::POST) {
        return true;
    }
    const bool resource_lock_shared = !(httpIsStateChangingMethod(request_method_));
    
    try  
    {
        rparam_ = &Config::rparams(request_uri_, resource_lock_shared);
    }
    catch (const WebServerError& exc) 
    {
        LOG_ERR(exc.what());
        return false;
    }
    catch (const std::exception& exc) 
    {
        rparam_ = Config::orparams(request_uri_, resource_lock_shared);
        if (!rparam_) { 
            return false;
        }
    }

    return true;
}

int Http::checkResource(const std::string& uri)
{
    const std::string filepath = std::string(RESOURCES_DIR) + ((uri.at(0) != '/') ? "/" : "") + uri;
    struct stat file_info = {0};

    errno = 0;
    if (stat(filepath.c_str(), &file_info) == -1)
    {
        // Neexistuje
        if (errno == ENOENT) {
            return -1;
        }
        // Neni pristup
        else if (errno == EACCES) {
            return -2;
        }
    }
    else
    {
        // Check if the path is a directory
        if (S_ISDIR(file_info.st_mode)) {
            return -3;
        }
    }

    return 0;
}

int Http::validateResource()
{
    if (request_method_ != HttpMethod::GET &&
        request_method_ != HttpMethod::HEAD &&
        request_method_ != HttpMethod::DELETE &&
        request_method_ != HttpMethod::OPTIONS)
    {
        return true;
    }

    return checkResource(request_uri_);
}

bool Http::createTemporaryFile(int& temporary_file_fd, Http::TempFile& temp_file)
{
    temporary_file_fd = open(temp_file.file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOCTTY, 0600);
    if (temporary_file_fd == -1)
    {
        LOG_ERR("Failed to create temporary file");
        return false;
    }

    temp_file.data_in_temp_file_ = true;
    return true;
}

void Http::deleteTemporaryFile(const int temporary_file_fd, const Http::TempFile& temp_file)
{
    if (temporary_file_fd != -1) {
        close(temporary_file_fd);
    }

    if (temp_file.data_in_temp_file_)
    {
        if (remove(temp_file.file_path_.c_str()) == -1) {
            LOG_ERR("Failed to delete temporary file (file: %s)", temp_file.file_path_.c_str());
        }
    }

    temp_files_.erase(temp_files_.find(temp_file.stream_id_));
}

bool Http::createTempDirectory()
{
    char buffer[PATH_MAX];
	snprintf(buffer, sizeof(buffer), TEMPORARY_DIR_NAME_FORMAT, tcp_connection_->getSocket());
	temp_files_dir_ = buffer;

    // Zkontrolovat zda uz existuje adresar a pripadne vytvorit adresar pro docasne soubory daneho spojeni
    errno = 0;
    DIR* dir = opendir(temp_files_dir_.c_str());
    if (dir) {
        closedir(dir);
    }
    else if (!dir && errno == ENOENT)
    {
        if (mkdir(temp_files_dir_.c_str(), 0700) == -1) {
            LOG_ERR("Failed to create temporary files directory");
            return false;
        }
    }
    else {
        LOG_ERR("Failed to check if temporary files directory exists (error: %s)", strerror(errno));
        return false;
    }

    return true;
}

void Http::generateTempFilePath(const Http::StreamId stream_id)
{
	char buffer[PATH_MAX];
	snprintf(buffer, sizeof(buffer), TEMPORARY_FILE_NAME_FORMAT, tcp_connection_->getSocket(), stream_id);

    Http::TempFile tmp_file;
    tmp_file.stream_id_ = stream_id;
    tmp_file.data_in_temp_file_ = false;
    tmp_file.file_path_ = std::string(buffer);

    std::pair<Http::StreamId, Http::TempFile> item(stream_id, tmp_file);
	temp_files_.insert(item);
}
