#ifndef __HTTP_1_0_HPP__
#define __HTTP_1_0_HPP__
#include "Http.hpp"
#include "HttpPacketBuilder.hpp"
#include "Configuration.hpp"
#include "request.h"
#include <sys/stat.h>


class Http1_0 : public Http
{
    public:
        Http1_0(const std::shared_ptr<TcpServer>& tcp_server, std::shared_ptr<TcpServer::Connection>& connection);
        virtual ~Http1_0() = default;
        static void moveHttpRequest(httpparser::Request& req, const httpparser::Request& req2);
        static void resetHttpRequest(httpparser::Request& req);

        void reset() override;
        void handleConnection() override;
        bool sendResponse(HttpPacketBase& packetb) override;
        int receiveRequest() override;

    protected:
        int sendFileChunk(const uint64_t offset, const int file_fd, const uint32_t chunk_size);
        bool compressDataToSend(std::string& dec_data, const void* data, const size_t data_size);

        bool requestGetMethod() override;
        bool requestHeadMethod() override;

        bool requestPostMethod() override;
        bool requestPostMethodFunc();

        bool requestPutMethod() override;
        bool requestPutMethodFunc();

        bool requestDeleteMethod() override;
        bool requestDeleteMethodFunc();

        bool getHeaderField(const char* field_name);
        int headersAcceptEncoding();
        int headersIfModifiedSince();
        int headersContentLength(uint64_t* content_length = nullptr);
        int headersContentType();
        int headersContentEncoding();

        bool checkResourceConstraints() override;
        void removeRequestDataHeaders();
        int receiveRequestBody();
        int receiveRequestBodyPutMethod();
        int receiveRequestBodyPostMethod();
        int receiveRetCheck(const int ret, const int temporary_file_fd);
        bool checkRequest();
        bool prepareRequestUri();

    protected:
        std::string request_data_;  /// TODO: Mozna primo presunou do tridy Http
        httpparser::Request request_;
        HttpPacketBuilder packet_builder_;
        HttpPacketBuilder packet_builder_sp_;   // Status page packet builder
        std::vector<httpparser::Request::HeaderItem>::const_iterator header_field_;
        std::string boundary_;
        std::string file_name_;
};


#endif