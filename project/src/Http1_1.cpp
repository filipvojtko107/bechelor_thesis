#include "Http1_1.hpp"
#include "Configuration.hpp"
#include "Logger.hpp"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>


Http1_1::Http1_1(const std::shared_ptr<TcpServer>& tcp_server, std::shared_ptr<TcpServer::Connection>& connection) :
    Http1_0(tcp_server, connection)
{
    this->http_version_ = HttpVersion::HTTP_1_1;
    this->packet_builder_.setHttpVersion(HttpVersion::HTTP_1_1);
    this->packet_builder_sp_.setHttpVersion(HttpVersion::HTTP_1_1);
}

void Http1_1::reset()
{
    Http1_0::reset();
    this->packet_builder_.setHttpVersion(HttpVersion::HTTP_1_1);
    this->packet_builder_sp_.setHttpVersion(HttpVersion::HTTP_1_1);
    ranges_.clear();
}


int Http1_1::headersIfRange()
{
    if (getHeaderField("If-Range"))
    {
        // Kontrola datumu
        struct tm gmt = {0};
        if (strptime(header_field_->value.c_str(), HTTP_DATE_FORMAT, &gmt) != NULL)
        {
            if (rparam_->last_modified == mktime(&gmt)) {
                return 1;
            }
        }

        // Kontrola ETag
        if (header_field_->value == rparam_->etag) {
            return 1;
        }

        return -1;
    }
    
    return 0;
}

int Http1_1::headersRange(const bool parse_ranges)
{
    if (getHeaderField("Range"))
    {
        int ret = httpCheckRangeHeader(header_field_->value.c_str());
        if (ret == -1)
        {
            //LOG_DBG("Failed to compile regex (range header)");
            packet_builder_sp_.buildInternalServerError();
            status_page_ = true;
            return -1;
        }
        else if (ret == -2)
        {
            //LOG_DBG("Failed to match request line with regex (range header)");
            packet_builder_sp_.buildBadRequest();
            status_page_ = true;
            return -1;
        }

        if (parse_ranges)
        {
            // Ziskani rozsahu
            std::vector<std::string> ranges_str;
            const char* delimiters = ", ";
            const char* bytes_str = "bytes=";

            char ranges_cpy[header_field_->value.size() - strlen(bytes_str) + 1] = {0};
            strncpy(ranges_cpy, header_field_->value.c_str()+strlen(bytes_str), 
                header_field_->value.size()-strlen(bytes_str));

            char* saveptr = nullptr;
            char* token = strtok_r(ranges_cpy, delimiters, &saveptr);
            if (token == nullptr) {
                ranges_str.push_back(ranges_cpy);
            }
            else
            {
                do
                {
                    ranges_str.push_back(token);
                    token = strtok_r(nullptr, delimiters, &saveptr);
                } while (token != nullptr);
            }

            // Kontrola rozsahu
            ranges_.resize(ranges_str.size());
            for (size_t i = 0; i < ranges_str.size(); ++i)
            {
                const std::string& rstr = ranges_str[i];
                Range& range = ranges_[i];

                if (rstr.front() == '-')
                {
                    errno = 0;
                    uint64_t val = strtoull(rstr.substr(1).c_str(), NULL, 10);
                    if (errno == EINVAL || errno == ERANGE || 
                        val > rparam_->resource_size) 
                    {
                        goto range_not_satisfiable;
                    }

                    range.end = val;
                    range.type = RangeType::SUFFIX_LENGTH;
                }

                else if (rstr.back() == '-')
                {
                    errno = 0;
                    uint64_t val = strtoull(rstr.substr(0, rstr.size()-1).c_str(), NULL, 10);
                    if (errno == EINVAL || errno == ERANGE || 
                        val >= rparam_->resource_size)
                    {
                        goto range_not_satisfiable;
                    }

                    range.start = val;
                    range.type = RangeType::START_INF;
                }

                else
                {
                    const size_t range_ind = rstr.find('-');
                    if (range_ind == std::string::npos) {
                        goto range_not_satisfiable;
                    }

                    errno = 0;
                    uint64_t val = strtoull(rstr.substr(0, range_ind).c_str(), NULL, 10);
                    if (errno == EINVAL || errno == ERANGE || 
                        val >= rparam_->resource_size) 
                    {
                        goto range_not_satisfiable;
                    }
                    
                    uint64_t val2 = strtoull(rstr.substr(range_ind+1).c_str(), NULL, 10);
                    if (errno == EINVAL || errno == ERANGE || 
                        val2 >= rparam_->resource_size)
                    {
                        goto range_not_satisfiable;
                    }

                    if (val > val2) {
                        goto range_not_satisfiable;
                    }

                    range.start = val;
                    range.end = val2;
                    range.type = RangeType::START_END;
                }
            }
        }

        return 1;
    }

    return 0;

range_not_satisfiable:
    packet_builder_sp_.buildRangeNotSatisfiable(rparam_);
    status_page_ = true;
    return -1;
}

int Http1_1::headersIfMatch()
{
    if (getHeaderField("If-Match"))
    {
        std::vector<std::string> etags;

        static const char* delimiters = ", ";
        char hfield_etags_cpy[header_field_->value.size() + 1] = {0};
        strncpy(hfield_etags_cpy, header_field_->value.c_str(), header_field_->value.size());
        char* saveptr = nullptr;

        char* token = strtok_r(hfield_etags_cpy, delimiters, &saveptr);
        if (token == nullptr) {
            etags.push_back(hfield_etags_cpy);
        }
        else
        {
            do
            {
                etags.push_back(token);
                token = strtok_r(nullptr, delimiters, &saveptr);
            } while (token != nullptr);
        }

        const auto etag_it = std::find(etags.cbegin(), etags.cend(), rparam_->etag);
        if (etag_it == etags.cend())
        {
            packet_builder_sp_.buildPreconditionFailed();
            status_page_ = true;
            return -1;
        }

        return 1;
    }

    return 0;
}

int Http1_1::headersIfNoneMatch(const bool rsrc_exists_check)
{
    if (getHeaderField("If-None-Match"))
    {
        if (rsrc_exists_check)
        {
            if (header_field_->value == HTTP_ASTERISK)
            {
                if (access((std::string(RESOURCES_DIR) + request_uri_).c_str(), F_OK) == 0)
                {
                    packet_builder_sp_.buildPreconditionFailed();
                    status_page_ = true;
                    return -1;
                }
            }
        }

        std::vector<std::string> etags;
        static const char* delimiters = ", ";
        char hfield_etags_cpy[header_field_->value.size() + 1] = {0};
        strncpy(hfield_etags_cpy, header_field_->value.c_str(), header_field_->value.size());
        char* saveptr = nullptr;

        char* token = strtok_r(hfield_etags_cpy, delimiters, &saveptr);
        if (token == nullptr) {
            etags.push_back(hfield_etags_cpy);
        }
        else
        {
            do
            {
                etags.push_back(token);
                token = strtok_r(nullptr, delimiters, &saveptr);
            } while (token != nullptr);
        }

        const auto etag_it = std::find(etags.cbegin(), etags.cend(), rparam_->etag);
        if (etag_it != etags.cend())
        {
            packet_builder_sp_.buildNotModified(rparam_);
            status_page_ = true;
            return -1;
        }

        return 1;
    }

    return 0;
}


int Http1_1::headersIfUnmodifiedSince()
{
    if (getHeaderField("If-Unmodified-Since"))
    {
        // Kontrola formatu datumu
        struct tm gmt = {0};
        if (strptime(header_field_->value.c_str(), HTTP_DATE_FORMAT, &gmt) != NULL)
        {
            if (rparam_->last_modified != mktime(&gmt))
            {
                packet_builder_sp_.buildPreconditionFailed();
                status_page_ = true;
                return -1;
            }

            return 1;
        }

        return -1;
    }

    return 0;
}

int Http1_1::headersExpect()
{
    if (getHeaderField("Expect"))
    {
        if (header_field_->value != "100-continue") 
        {
            packet_builder_sp_.buildExpectationFailed();
            status_page_ = true;
            return -1;            
        }
        else
        {
            // OK
            packet_builder_sp_.buildContinue();
            status_page_ = true;
            return -1; 
        }

        return 1;
    }

    return 0;
}


bool Http1_1::requestGetMethod()
{
    /*
        Accept -> zvazit, zda implementovat
        Accept-Language -> Zvazit
        Accept-Encoding,  -> Musim hlavne v odpovedi uvest Content-Encoding !!!!!!!!!!! ()
        If-Modified-Since,

        Range (Accept-Ranges a Content-Range v odpovedi),
        If-Range,

        If-Match,
        If-None-Match,
        If-Unmodified-Since,

        Host
    */

    if (rparam_ && !rparam_->isSet()) {
        const_cast<Config::RParams*>(rparam_)->update();
    }

    packet_builder_.createCommonHeaders(const_cast<Config::RParams*>(rparam_), HttpStatusCode::OK, false, true);

    int ret = headersIfNoneMatch();
    if (ret == -1) { return false; }
    else if (ret == 0) {
        if (headersIfModifiedSince() == -1) { return false; }
    }

    if (headersIfNoneMatch() == -1) { return false; }
    if (headersIfMatch() == -1) { return false; }
    if (headersIfUnmodifiedSince() == -1) { return false; }

    ret = headersRange(true);
    if (ret == -1) { return false; }
    else if (ret == 1) 
    {
        ret = headersIfRange();
        if (ret == -1) { 
            return false; 
        }
        else if (ret == 1)
        {
            packet_builder_.reset();
            packet_builder_.setEndHeaders(false);
            packet_builder_.setHttpVersion(this->http_version_);
            packet_builder_.createCommonHeaders(const_cast<Config::RParams*>(rparam_), HttpStatusCode::PARTIAL_CONTENT, false, true);
        }
    }

    // Content negotioation (proactive content negotioation = server driven)
    // if (headersAccept() == -1) { return false; }
    if (headersAcceptEncoding() == -1) { return false; }

    return true;
}


bool Http1_1::requestHeadMethod()
{
    if (rparam_ && !rparam_->isSet()) {
        const_cast<Config::RParams*>(rparam_)->update();
    }

    packet_builder_.packet().header().setIsHeadMethod(true);
    packet_builder_.createCommonHeaders(const_cast<Config::RParams*>(rparam_), HttpStatusCode::OK, false, true);

    int ret = headersIfNoneMatch();
    if (ret == -1) { return false; }
    else if (ret == 0) {
        if (headersIfModifiedSince() == -1) { return false; }
    }

    if (headersIfMatch() == -1) { return false; }
    if (headersIfUnmodifiedSince() == -1) { return false; }

    return true;
}

bool Http1_1::requestPostMethod()
{ 
    if (headersContentType() == -1) { return false; }
    if (headersContentEncoding() == -1) { return false; }
    if (headersIfNoneMatch() == -1) { return false; }
    if (headersIfMatch() == -1) { return false; }
    if (headersIfUnmodifiedSince() == -1) { return false; }
    if (headersExpect() == -1) { return false; }
    return true;
}

bool Http1_1::requestPutMethod()
{
    if (headersContentType() == -1) { return false; }
    if (headersContentEncoding() == -1) { return false; }
    if (headersIfNoneMatch() == -1) { return false; }
    if (headersIfMatch() == -1) { return false; }
    if (headersIfUnmodifiedSince() == -1) { return false; }
    if (headersExpect() == -1) { return false; }
    return true;
}

bool Http1_1::requestDeleteMethod()
{
    if (headersIfMatch() == -1) { return false; }
    if (headersIfUnmodifiedSince() == -1) { return false; }
    return requestDeleteMethodFunc();
}

// Muze odeslat 200 OK nebo 204 No Content, ale je lepsi odesilat 200 OK primo s Content-Lenght a Content-Type
bool Http1_1::requestOptionsMethod()
{
    if (rparam_ && !rparam_->isSet()) {
        const_cast<Config::RParams*>(rparam_)->update();
    }
    
    packet_builder_.createCommonHeaders(const_cast<Config::RParams*>(rparam_), HttpStatusCode::OK, false, true);
    return true;
}


void Http1_1::handleConnection()
{
    //LOG_DBG("Http1_0::handleConnection");

    // Vytvorit adresar pro temp files
    if (!this->createTempDirectory()) 
    {
        packet_builder_sp_.buildInternalServerError();
        this->sendResponse(packet_builder_sp_.packet());
        return;
    }

    // Udrzovani spojeni
    while (isConnected())
    {
        try
        {
            bool end_conn = false;
            int ret;
            //LOG_DBG("Running Http1_1::handleConnection...");

            // Vygenerovat nazev pro temp file
            this->generateTempFilePath(0);
            temp_file_ = &temp_files_.at(0);

            // Prijmout request
            ret = receiveRequest();
            if (ret == 0) {
                //LOG_DBG("Http1_1: receiveRequest() --> end_connection");
                end_conn = true;
            }
            else if (ret == -1) {
                //LOG_DBG("Http1_1: receiveRequest() --> send_response");
                goto send_response;
            }

            // Sestaveni odpovedi na zaklade metody HTTP requestu
            switch (request_method_)
            {
            case HttpMethod::GET:
                this->requestGetMethod();
                break;
            case HttpMethod::HEAD:
                this->requestHeadMethod();
                break;
            case HttpMethod::DELETE:
                this->requestDeleteMethod();
                break;
            case HttpMethod::OPTIONS:
                this->requestOptionsMethod();
                break;
            }

    send_response:
            // Poslat odpoved
            if (status_page_)
            {
                this->sendResponse(packet_builder_sp_.packet());

                bool remove_orparam = true;
                if (rparam_)
                {
                    HttpStatusCode hsc = 
                        packet_builder_sp_.packet().header().statusCode();
                    switch (hsc)
                    {
                        case HttpStatusCode::CONTINUE:
                        case HttpStatusCode::OK:
                        case HttpStatusCode::CREATED:
                        case HttpStatusCode::NO_CONTENT:
                        case HttpStatusCode::PARTIAL_CONTENT:
                        case HttpStatusCode::NOT_MODIFIED:
                            remove_orparam = false;
                            break;
                    }
                    if (remove_orparam) 
                    {
                        Config::orparamsRemove(rparam_);
                        rparam_ = nullptr;
                    }
                }

                if (packet_builder_.packet().header().isConnectionClosed() 
                    || request_method_ == HttpMethod::POST
                    || request_method_ == HttpMethod::PUT) 
                {
                    if (remove_orparam) { 
                        break;
                    }
                }
            }
            
            else 
            {
                //LOG_DBG("send_response: ending packet...");
                // Prevence vuci tomu yda mam ukoncene header fields v HTTP response
                if (!packet_builder_.packet().header().hasEnd()) {
                    packet_builder_.packet().header().end();
                }
                //LOG_DBG("send_response: packet ended");

                // Odeslani odpovedi
                if (!ranges_.empty()) {
                    //LOG_DBG("Sending ranges in response...");
                    sendResponseRanges();
                }
                else {
                    //LOG_DBG("send_response: sending packet...");
                    this->sendResponse(packet_builder_.packet());
                    //LOG_DBG("send_response: packet sent");
                }

                // Prevence vuci ponechani resourcu v nekonzistentnim stavu
                //LOG_DBG("send_response: checking is rparam_ set...");
                if (rparam_ && !rparam_->isSet()) {
                    //LOG_DBG("send_response: rparam_ update()...");
                    const_cast<Config::RParams*>(rparam_)->update();
                    //LOG_DBG("send_response: rparam_ updated");
                }
            }

            if (rparam_) 
            { 
                //LOG_DBG("Unlocking rparam...");
                const_cast<Config::RParams*>(rparam_)->unlock();
                //LOG_DBG("Unlocked rparam");
                rparam_ = nullptr;
            }

            if (end_conn) {
                break;
            }
            //LOG_DBG("Http1_1::reset()...");
            this->reset();
            //LOG_DBG("Http1_1::reset done");
        }

        catch (const std::exception& exc)
        {
            LOG_ERR("Error: %s", exc.what());
            
            if (rparam_) 
            { 
                const_cast<Config::RParams*>(rparam_)->unlock();
                rparam_ = nullptr;
            }
            packet_builder_sp_.buildInternalServerError();
            this->sendResponse(packet_builder_sp_.packet());
            break;
        }
    }

    try
    {
        // Ukoncit spojeni
        //LOG_DBG("Http1_1::Peacfully ending connection...");
        if (rparam_) 
        { 
            //LOG_DBG("Unlocking rparam...");
            const_cast<Config::RParams*>(rparam_)->unlock();
            rparam_ = nullptr;
            //LOG_DBG("Unlocked rparam");
        }
        //LOG_DBG("Http1_1::Ending connection...");
        this->endConnection();
        //LOG_DBG("Http1_1::Ended connection");
    }
    catch (const std::exception& e) {
    }
}


bool Http1_1::sendResponseRanges()
{
    HttpPacket& packet = packet_builder_.packet();
    const int file_fd = open(packet.body().data().data_.c_str(), O_RDONLY | O_NOCTTY);
    if (file_fd == -1) 
    {
        LOG_ERR("Failed to open file to send (file: %s)", packet.body().data().data_.c_str());
        return false;
    }

    for (const Range& range : ranges_)
    {
        uint32_t count;
        uint64_t offset;
        switch (range.type)
        {
        case RangeType::START_END:
            count = range.end - range.start + 1;
            offset = range.start;
            break;
        case RangeType::START_INF:
            count = rparam_->resource_size - range.start;
            offset = range.start;
            break;
        case RangeType::SUFFIX_LENGTH:
            count = range.end;
            offset = rparam_->resource_size - range.end;
            break;
        }

        uint64_t sent_bytes = 0;
        while (sent_bytes < count)
        {
            const uint32_t chunk_size = 
                std::min(count - sent_bytes, static_cast<uint64_t>(Config::params().file_chunk_size));

            packet.header().removeEnd();
            packet.header().removeContentLength();
            packet.header().removeContentRange();
            
            if (content_encoding_ != HttpContentEncoding::NONE) {
                packet.header().transferEncoding(content_encoding_str_ + ", chunked");
            }
            else { 
                packet.header().contentLength(chunk_size);
            }
            packet.header().contentRange(offset, offset+chunk_size-1, rparam_->resource_size);
            packet.header().end();

            // Odeslani hlavicky packetu
            int send_ret;
            send_ret = tcp_server_->sendText(this->tcp_connection_, packet.header().data());
            if (send_ret == -1) {
                goto err;
            }
            else if (send_ret == 0) {
                goto end_send;
            }

            // Odeslani casti souboru
            send_ret = sendFileChunk(offset, file_fd, chunk_size);
            if (send_ret == -1) {
                goto err;
            }
            else if (send_ret == 0) {
                goto end_send;
            }

            // Zakonceni transfer encoding
            if (content_encoding_ != HttpContentEncoding::NONE)
            {
                send_ret = tcp_server_->sendText(this->tcp_connection_, "0\r\n\r\n");
                if (send_ret == -1) {
                    goto err;
                }
                else if (send_ret == 0) {
                    return true;
                }
            }

            sent_bytes += chunk_size;
        }
    }

end_send:
    close(file_fd);
    return true;

err:
    //LOG_DBG("Failed to send ranges data");
    close(file_fd);
    return false;
}

bool Http1_1::checkResourceConstraints()
{
    if (!Http1_0::checkResourceConstraints()) {
        return false;
    }

    const int headers_range_ret = headersRange();
    if (headers_range_ret == -1) {
        return false;
    }
    else if (headers_range_ret == 1)
    {
        if (rparam_->accept_ranges == "none") {
            return false;
        }
    }

    return true;
}
