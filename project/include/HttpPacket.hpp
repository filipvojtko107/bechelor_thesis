#ifndef __HTTP_PACKET_HPP__
#define __HTTP_PACKET_HPP__
#include "HttpPacketBase.hpp"
#include "HttpGlobal.hpp"
#include <string>
#include <cstdint>


class HttpPacket : public HttpPacketBase
{
    public:
        class Header
        {
            public:
                void reset();
                const std::string& data() const { return data_; }
                bool isHeadMethod() const { return is_head_method_; }
                void setIsHeadMethod(const bool is_head_met) { is_head_method_ = is_head_met; }
                bool hasEnd() const { return has_end_; }
                bool isConnectionClosed() const { return is_connection_closed_; }
                HttpVersion httpVer() const { return http_version_; }
                void setHttpVersion(const HttpVersion ver) { http_version_ = ver; }
                HttpStatusCode statusCode() const { return status_code_; }
                void setStatusCode(const HttpStatusCode code) { status_code_ = code; }

                void statusLine(const HttpVersion version, const HttpStatusCode status_code);
                void server(const std::string& name);
                void contentType(const char* content_type);
                void contentType(const std::string& content_type) { contentType(content_type.c_str()); }
                void contentLength(const uint64_t len);
                void date(const char* dt);
                void date(const std::string& dt) { date(dt.c_str()); }
                void contentEncoding(const std::string& encoding);
                void vary();
                void allow(const std::string& methods_allowed);
                void expires(const char* exp);
                void expires(const std::string& exp) { expires(exp.c_str()); }
                void lastModified(const char* last_modified);
                void location(const std::string& rel_path);
                void end();

                // Od verze Http/1.1
                void connection(const std::string& conn);
                void acceptRanges(const char* ranges);
                void cacheControl(const std::string& control, const int32_t max_age);
                void etag(const ETag e); // ETag jako cas posledni modifikace v milisekundach
                void contentLocation(const std::string& rel_path);
                void contentRange(const std::string& range, const uint64_t size);
                void contentRange(const uint64_t start, const uint64_t end, const uint64_t size);
                void transferEncoding(const std::string& transfer_encoding);

                // Dalsi metody
                void removeContentLength();
                void removeContentRange();
                void removeEnd();

            private:
                std::string data_;
                bool is_head_method_ = false;
                bool is_connection_closed_ = false;
                bool has_end_ = false;
                HttpVersion http_version_ = HttpVersion::UNSUPPORTED;
                HttpStatusCode status_code_ = HttpStatusCode::UNSUPPORTED;
        };

        class Body
        {
            public:
                struct Data
                {
                    bool is_file_ = false;
                    std::string data_;  // Pokud je is_file_ = false, tak jsou zde ulozena ciste data, jinak je zde ulozen nazev souboru s daty
                    uint64_t content_length_ = 0;
                };

            public:
                void reset();
                bool addData(const std::string& data);
                bool addData(std::string&& data);
                bool addFile(const std::string& rel_path, const uint64_t file_size);

                const Data& data() const { return data_; }

            private:
                Data data_;
        };

        void reset();
        Header& header() { return header_; }
        Body& body() { return body_; }
        const Header& header() const { return header_; }
        const Body& body() const { return body_; }

    private:
        Header header_;
        Body body_;
};


#endif