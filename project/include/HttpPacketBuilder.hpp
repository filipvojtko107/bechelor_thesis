#ifndef __HTTP_PACKET_BUILDER_HPP__
#define __HTTP_PACKET_BUILDER_HPP__
#include "HttpPacketBuilderBase.hpp"
#include "HttpPacket.hpp"
#include "HttpGlobal.hpp"
#include "Configuration.hpp"


class HttpPacketBuilder : public HttpPacketBuilderBase
{
    public:
        HttpPacketBuilder(const HttpVersion http_version, const bool end_headers = true);
        void reset();

        void createCommonHeaders(const HttpStatusCode status_code, 
            const bool status_code_page = false, const bool keep_alive = true);  // Vytvorit headers bez pridani kontentu a souvisejicimi header fields
        void createCommonHeaders(const Config::RParams* rparam, 
            const HttpStatusCode status_code, const bool status_code_page = false, const bool keep_alive = true);

        void buildNoContent();
        void buildNoContent(const Config::RParams* rparam);
        void buildCreated(const Config::RParams* rparam);
        void buildBadRequest();
        void buildHttpVersionNotSupported();
        void buildNotFound();
        void buildMethodNotAllowed(const Config::RParams* rparam);
        void buildNotAcceptable();
        void buildForbidden();
        void buildLengthRequired();
        void buildContentTooLarge();
        void buildInternalServerError();
        void buildNotImplemented();
        void buildServiceUnavailable();
        void buildUnsupportedMediaType();
        void buildPreconditionFailed();
        void buildRangeNotSatisfiable(const Config::RParams* rparam);
        void buildContinue();
        void buildExpectationFailed();
        void buildNotModified(const Config::RParams* rparam);
        void buildServerOptions();

        void setEndHeaders(const bool end_headers) { end_headers_ = end_headers; }
        void setHttpVersion(const HttpVersion ver) { packet_.header().setHttpVersion(ver); }
        HttpPacket& packet() { return packet_; }
        const HttpPacket& packet() const { return packet_; }

    private:
        Config::RParams* getStatPageRParams(const std::string& status_page) { return &Config::rparams("/" + status_page, true); }

    private:
        bool end_headers_ = true;
        HttpPacket packet_;
};

#endif