#include "HttpPacketBuilder.hpp"
#include "Globals.hpp"
#include "Configuration.hpp"
#include "WebServerError.hpp"
#include <string>
#include <time.h>
#include <sys/stat.h>
#include <stdexcept>


std::string getPath(const std::string& uri) 
{
    const size_t query_pos = uri.find('?');
    const size_t fragment_pos = uri.find('#');

    const size_t cutoff = std::min(
        query_pos != std::string::npos ? query_pos : uri.size(),
        fragment_pos != std::string::npos ? fragment_pos : uri.size()
    );

    return uri.substr(0, cutoff);
}

bool getFileSuffix(const std::string& uri, std::string& suffix)
{
    if (uri.empty()) {
        return false;
    }

    const std::string path = getPath(uri);
    const size_t last_slash = path.rfind('/');
    const std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;

    if (filename.empty()) {
        return false;
    }

    const size_t dot_pos = filename.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }

    if (dot_pos == 0 || dot_pos == filename.size() - 1) {
        return false;
    }

    suffix = filename.substr(dot_pos);
    return true;
}


HttpPacketBuilder::HttpPacketBuilder(const HttpVersion http_version, const bool end_headers) : 
    end_headers_(end_headers)
{
    packet_.header().setHttpVersion(http_version);
}

void HttpPacketBuilder::reset()
{
    packet_.header().setHttpVersion(HttpVersion::UNSUPPORTED);
    end_headers_ = true;
    packet_.reset();
}

void HttpPacketBuilder::createCommonHeaders(const HttpStatusCode status_code, const bool status_code_page, const bool keep_alive)
{
    createCommonHeaders(nullptr, status_code, status_code_page, keep_alive);
}

void HttpPacketBuilder::createCommonHeaders(const Config::RParams* rparam, const HttpStatusCode status_code, const bool status_code_page, const bool keep_alive)
{
    HttpPacket::Header& pheader = packet_.header();

    if (pheader.httpVer() == HttpVersion::UNSUPPORTED) {
        throw WebServerError("Failed to create HTTP response (HTTP version not supported)");
    }

    // Pridavam jen pokud odesilam nejaky resource (pripadne i metoda HEAD)
    if (rparam) {
        packet_.body().addFile(rparam->resource_path, rparam->resource_size);
    }

    pheader.statusLine(pheader.httpVer(), status_code);
    pheader.server(Config::params().web_server_name);

    // Date
    time_t cas = time(nullptr);
    struct tm* gmt = gmtime(&cas);
    char datum[100];
    strftime(datum, sizeof(datum), HTTP_DATE_FORMAT, gmt);
    pheader.date(datum);

    // Pridavam jen pokud odesilam nejaky resource (pripadne i metoda HEAD)
    if (rparam)
    {
        // Content-Type
        std::string file_suffix;
        if (!getFileSuffix(rparam->resource_path, file_suffix)) {
            throw WebServerError("Failed to get file suffix");
        }
        const HttpContentTypeS* content_type = httpContentType(file_suffix);
        if (content_type == nullptr) {
            throw WebServerError("Invalid content type");
        }
        pheader.contentType(content_type->content_type_label);

        // Content-Length
        pheader.contentLength(packet_.body().data().content_length_);

        if (pheader.httpVer() == HttpVersion::HTTP_1_1) 
        { 
            if (keep_alive) { pheader.connection("keep-alive"); }
            else { pheader.connection("close"); }
        }

        // Pokud je to error page tak dalsi atributy generovat nebudu
        if (status_code_page) 
        {
            if (end_headers_) { pheader.end(); }
            return;
        }

        // Vary
        pheader.vary();

        // Last-Modified
        time_t mod_cas = rparam->last_modified;
        gmt = gmtime(&mod_cas);
        strftime(datum, sizeof(datum), HTTP_DATE_FORMAT, gmt);
        pheader.lastModified(datum);
        
        // Expires
        if (rparam->expires == -1) {
            pheader.expires("Thu, 01 Jan 1970 00:00:01 GMT");
        }
        else if (rparam->expires > 0) 
        {
            time_t exp = cas + rparam->expires;
            gmt = gmtime(&exp);
            strftime(datum, sizeof(datum), HTTP_DATE_FORMAT, gmt);
            pheader.expires(datum);
        }

        // Cache-Control
        pheader.cacheControl(rparam->cache_type, rparam->expires);

        // Header fields jen pro HTTP/1.1
        if (pheader.httpVer() == HttpVersion::HTTP_1_1)
        {
            // ETag
            pheader.etag(rparam->etag);

            // Accept-Ranges
            pheader.acceptRanges(rparam->accept_ranges.c_str());
        }
    }

    if (end_headers_) { pheader.end(); }
}


void HttpPacketBuilder::buildNoContent()
{
    createCommonHeaders(HttpStatusCode::NO_CONTENT, true);
}

void HttpPacketBuilder::buildNoContent(const Config::RParams* rparam)
{
    if (!rparam) {
        throw WebServerError("Failed to build packet (null resource parameters)");
    }

    HttpPacket::Header& pheader = packet_.header();
    setEndHeaders(false);
    createCommonHeaders(HttpStatusCode::NO_CONTENT, true);
    pheader.contentLocation(rparam->resource_path);

    // Last-Modified
    char datum[100];
    time_t mod_cas = rparam->last_modified;
    struct tm* gmt = gmtime(&mod_cas);
    strftime(datum, sizeof(datum), HTTP_DATE_FORMAT, gmt);
    pheader.lastModified(datum);

    if (pheader.httpVer() == HttpVersion::HTTP_1_1) {
        pheader.etag(rparam->etag);
    }
    pheader.end();
}

void HttpPacketBuilder::buildCreated(const Config::RParams* rparam)
{
    if (!rparam) {
        throw WebServerError("Failed to build packet (null resource parameters)");
    }

    HttpPacket::Header& pheader = packet_.header();
    setEndHeaders(false);
    createCommonHeaders(HttpStatusCode::CREATED, true);
    pheader.location("/" + rparam->resource_path);

    // Last-Modified
    char datum[100];
    time_t mod_cas = rparam->last_modified;
    struct tm* gmt = gmtime(&mod_cas);
    strftime(datum, sizeof(datum), HTTP_DATE_FORMAT, gmt);
    pheader.lastModified(datum);

    if (pheader.httpVer() == HttpVersion::HTTP_1_1) {
        pheader.etag(rparam->etag);
    }
    pheader.end();
}

void HttpPacketBuilder::buildBadRequest()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(BAD_REQUEST_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::BAD_REQUEST, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildHttpVersionNotSupported()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(HTTP_VERSION_NOT_SUPPORTED_ERROR_WEB_PAGE);
        createCommonHeaders(rparam, 
            HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED, true, false);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildNotFound()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(NOT_FOUND_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::NOT_FOUND, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildMethodNotAllowed(const Config::RParams* rparam)
{
    if (!rparam) {
        throw WebServerError("Failed to build packet (null resource parameters)");
    }

    Config::RParams* rparam_sp;
    try
    {
        rparam_sp = getStatPageRParams(METHOD_NOT_ALLOWED_WEB_PAGE);
        setEndHeaders(false);
        createCommonHeaders(rparam_sp, HttpStatusCode::METHOD_NOT_ALLOWED, true);

        std::string methods_allowed;
        methods_allowed.reserve(50);
        const std::vector<std::string>& resource_methods_allowed = rparam->methods_allowed;

        for (const std::string& m : resource_methods_allowed) 
        {
            methods_allowed.insert(methods_allowed.size(), m);
            methods_allowed.insert(methods_allowed.size(), ", ");
        }

        methods_allowed.erase(methods_allowed.size() - 2);
        packet_.header().allow(methods_allowed);
        packet_.header().end();
        rparam_sp->unlock();
    }
    catch (const WebServerError& exc) {
        rparam_sp->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam_sp->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildNotAcceptable()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(NOT_ACCEPTABLE_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::NOT_ACCEPTABLE, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildForbidden()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(FORBIDDEN_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::FORBIDDEN, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildLengthRequired()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(LENGTH_REQUIRED_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::LENGTH_REQUIRED, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildContentTooLarge()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(CONTENT_TOO_LARGE_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::CONTENT_TOO_LARGE, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildInternalServerError()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(INTERNAL_SERVER_ERROR_WEB_PAGE);
        createCommonHeaders(rparam, 
            HttpStatusCode::INTERNAL_SERVER_ERROR, true, false);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildNotImplemented()
{
    createCommonHeaders(HttpStatusCode::NOT_IMPLEMENTED, true);
}

void HttpPacketBuilder::buildServiceUnavailable()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(SERVICE_UNAVAILABLE_WEB_PAGE);
        createCommonHeaders(rparam, 
            HttpStatusCode::SERVICE_UNAVAILABLE, true, false);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildNotModified(const Config::RParams* rparam)
{
    if (!rparam) {
        throw WebServerError("Failed to build packet (null resource parameters)");
    }

    // Zde nastaveno status_page jako false, protoze pro Not Modified se odesilaji i ostatni polozky
    createCommonHeaders(rparam, HttpStatusCode::NOT_MODIFIED, false);
}

void HttpPacketBuilder::buildUnsupportedMediaType()
{
    Config::RParams* rparam;
    try
    {
        rparam = getStatPageRParams(UNSUPPORTED_MEDIA_TYPE_WEB_PAGE);
        createCommonHeaders(rparam, HttpStatusCode::UNSUPPORTED_MEDIA_TYPE, true);
        rparam->unlock();
    }
    catch (const WebServerError& exc) {
        rparam->unlock();
        throw WebServerError(exc.what());
    }
    catch (const std::exception& exc) {
        rparam->unlock();
        throw std::runtime_error(exc.what());
    }
}

void HttpPacketBuilder::buildPreconditionFailed()
{
    createCommonHeaders(HttpStatusCode::PRECONDITION_FAILED, true);
}

void HttpPacketBuilder::buildRangeNotSatisfiable(const Config::RParams* rparam)
{
    if (!rparam) {
        throw WebServerError("Failed to build packet (null resource parameters)");
    }

    setEndHeaders(false);
    createCommonHeaders(HttpStatusCode::RANGE_NOT_SATISFIABLE, true);
    packet_.header().contentRange("*", rparam->resource_size);
    packet_.header().end();
}


void HttpPacketBuilder::buildServerOptions()
{
    HttpPacket::Header& pheader = packet_.header();
    pheader.statusLine(pheader.httpVer(), HttpStatusCode::NO_CONTENT);
    pheader.server(Config::params().web_server_name);
    pheader.allow(HTTP_ALL_SUPPORTED_METHODS);
    pheader.end();
}

void HttpPacketBuilder::buildContinue()
{
    HttpPacket::Header& pheader = packet_.header();
    pheader.statusLine(pheader.httpVer(), HttpStatusCode::CONTINUE);
    pheader.server(Config::params().web_server_name);
    pheader.end();
}

void HttpPacketBuilder::buildExpectationFailed()
{
    createCommonHeaders(HttpStatusCode::EXPECTATION_FAILED, true);
}
