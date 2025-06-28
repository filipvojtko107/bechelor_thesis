#ifndef __HTTP_HPP__
#define __HTTP_HPP__
#include "HttpGlobal.hpp"
#include "HttpPacketBase.hpp"
#include "TcpServer.hpp"
#include "Configuration.hpp"
#include <memory>
#include <unordered_map>


class Http
{
    public:
        using StreamId = uint32_t;  // Stream id je 31-bit cislo, 0x0 je rezervovano pro cele spojeni

        struct TempFile
        {
            uint32_t stream_id_ = 0;
            bool data_in_temp_file_ = false;  // Pokud byly zapsany data (telo HTTP paketu) do docasneho souboru
            std::string file_path_;  // Temporary file
        };

    public:
        Http(const std::shared_ptr<TcpServer>& tcp_server, 
            std::shared_ptr<TcpServer::Connection>& connection, const HttpVersion http_version);
        virtual ~Http() = default;

        virtual void reset();
        virtual void handleConnection() = 0;
        virtual bool sendResponse(HttpPacketBase& packetb) = 0;
        virtual int receiveRequest() = 0;
        bool endConnection();
        bool isConnected() const { return tcp_server_->isConnected(tcp_connection_); }
        HttpVersion httpVersion() const { return http_version_; }
        
    protected:
        virtual bool requestGetMethod() = 0;
        virtual bool requestHeadMethod() = 0;
        virtual bool requestPostMethod() = 0;
        virtual bool requestPutMethod() = 0;
        virtual bool requestDeleteMethod() = 0;

        bool getResourceParam();
        int checkResource(const std::string& uri);
        virtual bool checkResourceConstraints() = 0;
        int validateResource();
        bool storeResource(Config::RParams* rparam_, const Http::TempFile& temp_file, const std::string& rel_path, const char* resource_data = nullptr, const uint64_t resource_size = 0);
        bool deleteResource(const std::string& file_name);

        bool createTemporaryFile(int& temporary_file_fd, Http::TempFile& temp_file);
        void deleteTemporaryFile(const int temporary_file_fd, const Http::TempFile& temp_file);  // Nic nevracim, smazat temporary file potichu -> pokud se nesmaze jeji obsah se normalne prepise pri ulozeni dalsiho tela requestu
        bool createTempDirectory();
        void generateTempFilePath(const Http::StreamId stream_id);

    protected:
        std::shared_ptr<TcpServer> tcp_server_;
        std::shared_ptr<TcpServer::Connection> tcp_connection_;
        HttpVersion http_version_;
        HttpContentEncoding content_encoding_;
        std::string content_encoding_str_;
        HttpMethod request_method_;
        std::string request_uri_;
        bool status_page_;
        const Http::TempFile* temp_file_;
        const Config::RParams* rparam_;

        std::string temp_files_dir_;  // Cesta k adresari s temporary files
        std::unordered_map<Http::StreamId, Http::TempFile> temp_files_;  // .first = stream id (pro HTTP/1.x stream id nejsou, takze je zde vzdy jen jedna polozka (stream id = 0)) 
};


#endif