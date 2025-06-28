#include "Configuration.hpp"
#include "WebServerError.hpp"
#include "Logger.hpp"
#include "toml.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>


#define MAX_OTHER_RPARAMS	100


// WebServer.config parameters
#define SERVER_NAME								"server_name"
#define IP_ADDRESS								"ip_address"
#define PORT									"port"
#define PORT_HTTPS								"port_https"
#define HTTPS_ENABLED							"https_enabled"
#define SSL_CERTIFICATE_RSA						"ssl_certificate_rsa"
#define PRIVATE_KEY_RSA							"private_key_rsa"
#define SSL_CERTIFICATE_ECDSA					"ssl_certificate_ecdsa"
#define PRIVATE_KEY_ECDSA						"private_key_ecdsa"
#define CIPHER_SUITES							"cipher_suites"
#define CLIENT_THREADS							"client_threads"
#define FILE_CHUNK_SIZE							"file_chunk_size"
#define MAX_HEADER_SIZE							"max_header_size"
#define CLIENT_BODY_BUFFER_SIZE					"client_body_buffer_size"
#define CLIENT_MAX_BODY_SIZE					"client_max_body_size"
#define PREFER_CONTENT_ENCODING					"prefer_content_encoding"


// resources.conf parameters
#define ALL_RESOURCES_NAME						"all_resources_name"
#define RESOURCE_REL_PATH						"resource_path"
#define METHODS_ALLOWED							"methods_allowed"
#define ACCEPT_RANGES							"accept_ranges"
#define EXPIRES									"expires"
#define CACHE_TYPE								"cache_type"


Config Config::obj_;


std::string buildErrMess(const char* mess, const char* param, const std::string* resource)
{
	std::string errmess;
	errmess.append(mess);
	errmess.append(" (param: ");
	errmess.append(param);
	if (resource) { 
		errmess.append(", resource: ");
		errmess.append(*resource);
	}
	errmess.append(")");
	return errmess;
}

template<typename T>
void getValue(T& obj, const char* param, const toml::value& input, const std::string* resource = nullptr)
{
	std::string errmess;

	try
	{
		if (resource) 
		{ 
			const auto input_par = toml::find(input, *resource);
			obj = toml::find<T>(input_par, param);
		}
		else { 
			obj = toml::find<T>(input, param);
		}
	}

	catch (const std::out_of_range& e) {
		throw WebServerError(buildErrMess("Missing parameter in configuration file", param, resource));
	}

	catch (const toml::type_error& e) {
		throw WebServerError(buildErrMess("Bad parameter data type in configuration file", param, resource));
	}
}

bool Config::buildParams()
{
	try
	{
		const toml::value input = toml::parse(CONFIG_FILE_FPATH);

		getValue(params_.web_server_name, SERVER_NAME, input);
		getValue(params_.ip_address, IP_ADDRESS, input);
		getValue(params_.port, PORT, input);
		getValue(params_.port_https, PORT_HTTPS, input);
		getValue(params_.https_enabled, HTTPS_ENABLED, input);
		getValue(params_.ssl_certificate_rsa, SSL_CERTIFICATE_RSA, input);
		getValue(params_.private_key_rsa, PRIVATE_KEY_RSA, input);
		getValue(params_.ssl_certificate_ecdsa, SSL_CERTIFICATE_ECDSA, input);
		getValue(params_.private_key_ecdsa, PRIVATE_KEY_ECDSA, input);
		getValue(params_.cipher_suites, CIPHER_SUITES, input);

		getValue(params_.client_threads, CLIENT_THREADS, input);
		getValue(params_.file_chunk_size, FILE_CHUNK_SIZE, input);
		getValue(params_.max_header_size, MAX_HEADER_SIZE, input);
		getValue(params_.client_body_buffer_size, CLIENT_BODY_BUFFER_SIZE, input);
		getValue(params_.client_max_body_size, CLIENT_MAX_BODY_SIZE, input);
		getValue(params_.prefer_content_encoding, PREFER_CONTENT_ENCODING, input);
	}

	catch (const toml::syntax_error& e) {
		LOG_ERR("Invalid syntax of configuration file");
		return false;
	}

	catch (const WebServerError& e) {
		LOG_ERR(e.what());
		return false;
	}

	catch (const std::exception& e) {
		LOG_ERR("Error while configuration file parsing (error: %s)", e.what());
		return false;
	}

	return true;
}

bool Config::buildRParams()
{
	try
	{
		const toml::value input = toml::parse(RESOURCES_CONFIG_FILE_FPATH);
		
		std::vector<std::string> all_resources_name;
		getValue(all_resources_name, ALL_RESOURCES_NAME, input);

		Config::RParams rparam;
		std::string rsrc_path;
		for (const std::string& resource_name : all_resources_name)
		{
			getValue(rparam.resource_path, RESOURCE_REL_PATH, input, &resource_name);
			getValue(rparam.methods_allowed, METHODS_ALLOWED, input, &resource_name);
			getValue(rparam.accept_ranges, ACCEPT_RANGES, input, &resource_name);
			getValue(rparam.expires, EXPIRES, input, &resource_name);
			getValue(rparam.cache_type, CACHE_TYPE, input, &resource_name);
			rsrc_path = rparam.resource_path;
			
			try {
				rparam.update();
			}
			catch (const std::exception& exc) {
				LOG_ERR("Failed to parse resources configuration file (error: %s)", exc.what());
				return false;
			}
		
			auto item = rparams_.emplace(std::move(rsrc_path), std::move(rparam));
			if (!item.second)
			{
				LOG_ERR("Failed to parse resources configuration file");
				return false;
			}
		}
	}

	catch (const toml::syntax_error& e) {
		LOG_ERR("Invalid syntax of resources configuration file");
		return false;
	}

	catch (const WebServerError& e) {
		LOG_ERR(e.what());
		return false;
	}

	catch (const std::exception& e) {
		LOG_ERR("Unknown error in resources configuration file parsing (error: %s)", e.what());
		return false;
	}
	
	return true;
}


bool Config::loadConfig()
{
	return obj_.buildParams();
}

bool Config::loadResourcesConfig()
{
	return obj_.buildRParams();
}

Config::RParams* Config::orparams(const std::string& resource, const bool resource_lock_shared)
{
	try
	{
		std::lock_guard<std::mutex> lock(obj_.orparams_mutex_);

		static const std::vector<std::string> default_allowed_methods = 
			{ "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS" };

		// Nejdrive zkusit najit resource
		//LOG_DBG("Finding other resource...");
		const std::string rsrc_path = ((!resource.empty() && resource.at(0) == '/') ? resource.substr(1) : resource);
		const auto rsrc_it = obj_.other_rparams_.find(rsrc_path);
		if (rsrc_it != obj_.other_rparams_.end()) 
		{
			//LOG_DBG("Other resource found");
			rsrc_it->second.lock(resource_lock_shared);
			return &rsrc_it->second;
		}

		//LOG_DBG("Creating other resource...");

		Config::RParams rparam;
		rparam.resource_path = rsrc_path;
		rparam.methods_allowed = default_allowed_methods;
		rparam.accept_ranges = "bytes";
		rparam.expires = 3600;  // 1 hodina v sekundach
		rparam.cache_type = "public";
		// Zatim nastavuji defaultni hodnoty, protoze resource nemusi byt jeste vytvoreny
		rparam.resource_size = 0;
		rparam.last_modified = -1;
		rparam.etag = "";
		rparam.last_resource_access = time(NULL);
		
		if (obj_.other_rparams_.size() == MAX_OTHER_RPARAMS)
		{
			const auto rparam_it = std::min_element(obj_.other_rparams_.cbegin(), obj_.other_rparams_.cend(),
			[](const std::pair<const std::string, Config::RParams>& r1, 
			   const std::pair<const std::string, Config::RParams>& r2) 
			{
				return r1.second.last_resource_access < r2.second.last_resource_access;
			});

			if (rsrc_path != rparam_it->first) {
				//LOG_DBG("Erasing other resource...");
				obj_.other_rparams_.erase(rparam_it);
				//LOG_DBG("Erased other resource");
			}
		}

		//LOG_DBG("Adding other resource param...");
		obj_.other_rparams_[rsrc_path] = std::move(rparam);
		//LOG_DBG("Added other resource param");

		//LOG_DBG("Locking other resource param...");
		Config::RParams* rp = &obj_.other_rparams_[rsrc_path];
		rp->lock(resource_lock_shared);
		//LOG_DBG("Locked other resource param");
		return rp;
	}

	catch (const std::exception& exc)
	{	
	}

	return nullptr;
}

void Config::orparamsRemove(const Config::RParams* rparam)
{
	try
	{
		if (rparam) 
		{
			std::lock_guard<std::mutex> lock(obj_.orparams_mutex_);
			auto rparam_it = obj_.other_rparams_.find(rparam->resource_path);
			if (rparam_it == obj_.other_rparams_.end()) {
				return;
			}
			rparam_it->second.unlock();
			obj_.other_rparams_.erase(rparam_it);
		}
	}

	catch (const std::exception& exc) {
		//LOG_DBG("Failed to remove other resource param");
	}
}


Config::RParams& Config::rparams(const std::string& resource, const bool resource_lock_shared)
{ 
	Config::RParams& rparam = obj_.rparams_.at((!resource.empty() && resource.at(0) == '/') ? resource.substr(1) : resource);
	rparam.lock(resource_lock_shared);
	return rparam;
}

void Config::reset()
{
	obj_.params_.reset();
	obj_.rparams_.clear();
	obj_.other_rparams_.clear();
}


void Config::Params::reset()
{
	web_server_name.clear();
	ip_address.clear();
	port = 0;
	port_https = 0;
	ssl_certificate_rsa.clear();
	private_key_rsa.clear();
	ssl_certificate_ecdsa.clear();
	private_key_ecdsa.clear();
	cipher_suites.clear();

	client_threads = 0;
	file_chunk_size = 0;
	max_header_size = 0;
	client_body_buffer_size = 0;
	client_max_body_size = 0;
	prefer_content_encoding = false;
}


Config::RParams::RParams() :
	expires(0),
	resource_size(0),
	last_modified(-1),
	access_lock(PTHREAD_RWLOCK_INITIALIZER),
	access_counter(0),
	resource_fd(-1),
	last_resource_access(-1)
{
}

Config::RParams::~RParams()
{
	this->unlock();
	if (resource_fd != -1) {
		close(resource_fd);
	}
}

Config::RParams::RParams(Config::RParams&& obj) :
	resource_path(std::move(obj.resource_path)),
	methods_allowed(std::move(obj.methods_allowed)),
	accept_ranges(std::move(obj.accept_ranges)),
	expires(obj.expires),
	cache_type(std::move(obj.cache_type)),
	resource_size(obj.resource_size),
	last_modified(obj.last_modified),
	etag(std::move(obj.etag)),
	access_lock(PTHREAD_RWLOCK_INITIALIZER),
	access_counter((uint32_t)obj.access_counter),
	resource_fd(obj.resource_fd),
	last_resource_access(obj.last_resource_access)
{
	obj.expires = 0;
	obj.resource_size = 0;
	obj.last_modified = -1;
	obj.access_counter = 0;
	obj.resource_fd = -1;
	obj.last_resource_access = -1;
}

Config::RParams& Config::RParams::operator=(Config::RParams&& obj)
{
	if (this != &obj)
	{
		resource_path = std::move(obj.resource_path);
		methods_allowed = std::move(obj.methods_allowed);
		accept_ranges = std::move(obj.accept_ranges);
		expires = obj.expires;
		cache_type = std::move(obj.cache_type);
		resource_size = obj.resource_size;
		last_modified = obj.last_modified;
		etag = std::move(obj.etag);
		access_counter = (uint32_t)obj.access_counter;
		resource_fd = obj.resource_fd;
		last_resource_access = obj.last_resource_access;

		obj.expires = 0;
		obj.resource_size = 0;
		obj.last_modified = -1;
		obj.access_counter = 0;
		obj.resource_fd = -1;
		obj.last_resource_access = -1;
	}

	return *this;
}


void Config::RParams::lock(const bool resource_lock_shared)
{
	if (resource_lock_shared) 
	{
		//LOG_DBG("\nLocking shared...");
		//LOG_DBG("Locking access..");
		if (pthread_rwlock_rdlock(&access_lock) != 0) {
			goto err;
		}
		//LOG_DBG("Locked access");

		if (resource_fd != -1)
		{
			//LOG_DBG("Locking file...");
			if (flock(resource_fd, LOCK_SH) == -1) {
				goto err;
			}
			//LOG_DBG("Locked file");
		}
		//LOG_DBG("Locked shared\n");
	}
	else 
	{
		//LOG_DBG("\nLocking exclusive...");
		//LOG_DBG("Locking access..");
		if (pthread_rwlock_wrlock(&access_lock) != 0) {
			goto err;
		}
		//LOG_DBG("Locked access");

		if (resource_fd != -1)
		{
			//LOG_DBG("Locking file...");
			if (flock(resource_fd, LOCK_EX) == -1) {
				goto err;
			}
			//LOG_DBG("Locked file");
		}
		//LOG_DBG("Locked exclusive\n");
	}

	access_counter++;
	return;

err:
	throw WebServerError(
		std::string("Failed to lock resource (err: ") + strerror(errno) + ")");
}

void Config::RParams::unlock()
{
	if (access_counter == 1)
	{
		if (resource_fd != -1) {
			//LOG_DBG("Unlocking file lock...");
			flock(resource_fd, LOCK_UN);
		}
		//LOG_DBG("Unlocking access...");
		pthread_rwlock_unlock(&access_lock);
	}
	if (access_counter > 0) {
		//LOG_DBG("Decrementing access counter...");
		access_counter--;
	}
}

void Config::RParams::releaseFileLock()
{
	if (resource_fd != -1) 
	{
		//LOG_DBG("\nReleasing file lock...\n");
		flock(resource_fd, LOCK_UN);
		close(resource_fd);
		resource_fd = -1;
	}
}

void Config::RParams::update()
{
	std::unique_lock<std::mutex> lock(update_lock, std::try_to_lock);

	// Jiny klient provadi update --> 
	// dva klienti aktualizuji stejny resource pouze kdyz je resource uzamcen ve sdilenem rezimu, 
	// v exkluzivnim to sem nemuze dojit, protoze ceka na ziskani zamku od klienta, ktery si jej vzal na dany resource pred nim
	if (!lock.owns_lock()) {
		return;
	}

	const std::string file_path = std::string(RESOURCES_DIR) + "/" + resource_path;
	if (resource_fd == -1)
	{
		resource_fd = open(file_path.c_str(), O_RDWR | O_NOCTTY);
		if (resource_fd == -1) {
			throw WebServerError("Failed to open resource (file: " + file_path + ")");
		}
	}

	struct stat st = {0};
	if (stat(file_path.c_str(), &st) < 0) {
		throw WebServerError("Failed to get resource info (file: " + file_path + ")");
	}

	if (last_modified != st.st_mtime)
	{
		resource_size = st.st_size;
		last_modified = st.st_mtime;
		etag = generateETag(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
	}

	updateLastAccess();
}

void Config::RParams::updateLastAccess()
{
	last_resource_access = time(NULL);
}

std::string Config::RParams::generateETag(const int64_t sec, const int64_t nsec)
{
	const int64_t e = sec*1000 + nsec/1000000;  // ETag = cas modifikace souboru v milisekundach
	return (std::string("\"") + std::to_string(e) + "\"");
}
