#include "Http1_0.hpp"
#include "HttpPacketBuilder.hpp"
#include "Logger.hpp"
#include "Configuration.hpp"
#include "Globals.hpp"
#include "HttpGlobal.hpp"
#include "Codec.hpp"
#include "WebServerError.hpp"
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <algorithm>


Http1_0::Http1_0(const std::shared_ptr<TcpServer>& tcp_server, std::shared_ptr<TcpServer::Connection>& connection) :
    Http(tcp_server, connection, HttpVersion::HTTP_1_0),
    packet_builder_(http_version_, false),
    packet_builder_sp_(http_version_)
{
}

void Http1_0::reset()
{
    Http::reset();
    packet_builder_sp_.reset();
    packet_builder_sp_.setHttpVersion(this->http_version_);
    packet_builder_.setEndHeaders(true);

    packet_builder_.reset();
    packet_builder_.setHttpVersion(this->http_version_);
    packet_builder_.setEndHeaders(false);

    boundary_.clear();
    file_name_.clear();
    request_data_.clear();
    resetHttpRequest(request_);
}

/*
    Response:
        Allow,
        Content-Language -> zvazit
        Content-Type,
        Content-Encoding (podmineny Accept-Encoding v requestu),
        Content-Length,
        Date,
        Expires (podmineno nastavenim v konfiguraku pro jednotlive rousources),
        Last-Modified, (jen pro verzi 1.0, pro 1.1 se dava ETag)
        Server,
*/

bool Http1_0::getHeaderField(const char* field_name)
{
    header_field_ = std::find_if(request_.headers.cbegin(), request_.headers.cend(), 
    [field_name](const httpparser::Request::HeaderItem& header_item)
    {
        return (strcasecmp(field_name, header_item.name.c_str()) == 0);
    });

    return (header_field_ != request_.headers.cend());
}

int Http1_0::headersAcceptEncoding()
{
    if (getHeaderField("Accept-Encoding"))
    {
        const auto* content_enc = httpContentEncoding(header_field_->value, true);
        if (!content_enc)
        {
            packet_builder_sp_.buildNotAcceptable();
            status_page_ = true;
            return -1;
        }

        if (Config::params().prefer_content_encoding) 
        {
            packet_builder_.packet().header().contentEncoding(content_enc->first);
            content_encoding_str_ = content_enc->first;
            content_encoding_ = content_enc->second;
        }

        return 1;
    }

    return 0;
}

int Http1_0::headersIfModifiedSince()
{
    if (getHeaderField("If-Modified-Since"))
    {
        // Kontrola formatu datumu
        struct tm gmt = {0};
        if (strptime(header_field_->value.c_str(), HTTP_DATE_FORMAT, &gmt) != NULL)
        {
            // Kontrola zda byl modifikovan
            if (rparam_->last_modified == mktime(&gmt))
            {
                packet_builder_sp_.buildNotModified(rparam_);
                status_page_ = true;
                return -1;
            }
            else {
                // Pokud nebyl zdroj modifikovan, odesila se cely zdroj (normalne jako metoda GET)
                return 1;
            }
        }
        else {
            // Invalidni format casu - odesila se cely zdroj (normalne jako metoda GET)
            return 1;
        }
    }

    return 0;
}


int Http1_0::headersContentLength(uint64_t* content_length)
{
    if (getHeaderField("Content-Length")) 
    {
        errno = 0;
        const uint64_t content_len = strtoull(header_field_->value.c_str(), NULL, 10);
        if (errno == EINVAL || errno == ERANGE)  // Poskytnuta hodnota Content-Length neni validni
        {
            packet_builder_sp_.buildBadRequest();
            status_page_ = true;
            return -1;
        }

        if (content_length) {
            *content_length = content_len;
        }

        return 1;
    }

    packet_builder_sp_.buildLengthRequired();
    status_page_ = true;
    return -1;
}


int Http1_0::headersContentType()
{
    // TODO:
    // Dle specifikace https://www.w3.org/Protocols/rfc2616/rfc2616-sec7.html#sec7.2.1 neni user-agent (klient) nutny posilat Content-Type
    // v PUT, POST requestu. Server si muze odvodit typ sam dle pripony souboru v URI. Pokud ani to neni tak je soubor ve formatu "application/octet-stream".
    // Tohle asi nebudu podporovat, proste tam musi byt --> nove teda podporuju odvozeni Content-Type z pripony resource uvedenem v URI

    if (getHeaderField("Content-Type")) 
    {
        const std::string& content_type = header_field_->value;
        const size_t semicolon_ind = content_type.find(';');
        const size_t charset_ind = content_type.find("charset=");
        const std::string boundary_str = "boundary=";
        const size_t boundary_ind = content_type.find(boundary_str);
        std::string ct;

        // charset
        if (charset_ind != std::string::npos)
        {
            if (charset_ind > 0) { 
                ct = content_type.substr(0, semicolon_ind);
            }
        }
        // boundary
        else if (boundary_ind != std::string::npos)
        {
            if (boundary_ind > 0) 
            { 
                ct = content_type.substr(0, semicolon_ind);
                boundary_ = content_type.substr(boundary_ind+boundary_str.size());
            }
        }
        else 
        {
            ct = content_type;
        }

        if (!httpContentTypeSuffix(ct))
        {
            packet_builder_sp_.buildUnsupportedMediaType();
            status_page_ = true;
            return -1;
        }

        return 1;
    }
    else
    {
        const size_t file_suffix_ind = request_uri_.rfind('.');
        if (file_suffix_ind == std::string::npos)
        {
            packet_builder_sp_.buildUnsupportedMediaType();
            status_page_ = true;
            return -1;
        }

        const std::string file_suffix = request_uri_.substr(file_suffix_ind);
        if (!httpContentType(file_suffix))
        {
            packet_builder_sp_.buildUnsupportedMediaType();
            status_page_ = true;
            return -1;
        }

        return 1;
    }

    return 0;
}


int Http1_0::headersContentEncoding()
{
    if (getHeaderField("Content-Encoding"))
    {
        const auto* content_enc = httpContentEncoding(header_field_->value, false);
        if (!content_enc)
        {
            packet_builder_sp_.buildUnsupportedMediaType();
            status_page_ = true;
            return -1;
        }

        content_encoding_str_ = content_enc->first;
        content_encoding_ = content_enc->second;
        return 1;
    }

    return 0;
}


bool Http1_0::requestGetMethod()
{
    if (rparam_ && !rparam_->isSet()) {
        const_cast<Config::RParams*>(rparam_)->update();
    }
    
    packet_builder_.createCommonHeaders(const_cast<Config::RParams*>(rparam_), HttpStatusCode::OK, false /*, false -> pro keep_alive, u Http/1.0 ale neresim*/);

    if (headersAcceptEncoding() == -1) { return false; }
    if (headersIfModifiedSince() == -1) { return false; }
    return true;
}


bool Http1_0::requestHeadMethod()
{
    if (rparam_ && !rparam_->isSet()) {
        const_cast<Config::RParams*>(rparam_)->update();
    }

    packet_builder_.packet().header().setIsHeadMethod(true);
    packet_builder_.createCommonHeaders(const_cast<Config::RParams*>(rparam_), HttpStatusCode::OK, false /*, false -> pro keep_alive, u Http/1.0 ale neresim*/);
    if (headersIfModifiedSince() == -1) { return false; }
    return true;
}


bool Http1_0::requestPostMethod()
{
    /*
        Content-Encoding,
        Content-Length,
        Content-Type,

        *** Pro Http/1.1 ***
        Host
        Pripadne dodelat Authorization
    */
    
    if (headersContentType() == -1) { return false; }
    if (headersContentEncoding() == -1) { return false; }
    return true;
}

bool Http1_0::requestPostMethodFunc()
{
    /*Pokud nebyly ulozeny data tela HTTP requestu do dosacneho souboru, 
    tak jsou v request_data_ a pro dalsi zpracovani je musim odstranit*/
    if (!temp_file_->data_in_temp_file_) {
        removeRequestDataHeaders();
    }

    if (!this->storeResource(nullptr, *temp_file_, 
        file_name_, request_data_.data(), request_data_.size())) 
    {
        packet_builder_sp_.buildInternalServerError();
        status_page_ = true;
        return false;
    }

    packet_builder_.createCommonHeaders(HttpStatusCode::OK, false /*, false -> pro keep_alive, Http/1.0 ale nepodporuje*/);

    return true;
}


bool Http1_0::requestPutMethod()
{
    /*
        Content-Encoding,
        Content-Length,
        Content-Type,

        *** Pro Http/1.1 ***
        Host
        Pripadne dodelat Authorization
    */

    if (headersContentType() == -1) { return false; }
    if (headersContentEncoding() == -1) { return false; }

    //LOG_DBG("PUT::packet_builder_ done");
    return true;
}

bool Http1_0::requestPutMethodFunc()
{
    // Ukladam resource jen pro metodu POST a PUT
    const bool file_existed = 
        (access((std::string(RESOURCES_DIR) + request_uri_).c_str(), F_OK) == 0);

    if (!this->storeResource(const_cast<Config::RParams*>(rparam_), *temp_file_, 
        request_uri_, request_data_.data(), request_data_.size())) 
    {
        packet_builder_sp_.buildInternalServerError();
        status_page_ = true;
        return false;
    }

    const_cast<Config::RParams*>(rparam_)->update();

    if (file_existed) {
        packet_builder_sp_.buildNoContent(rparam_);
    }
    else {
        packet_builder_sp_.buildCreated(rparam_);
    }
    status_page_ = true;

    return true;
}


bool Http1_0::requestDeleteMethod()
{
    /*
        *** Pro Http/1.1 ***
        Host
        Pripadne dodelat Authorization
    */

    return requestDeleteMethodFunc();
}

bool Http1_0::requestDeleteMethodFunc()
{
    if (!this->deleteResource(request_uri_)) 
    {
        packet_builder_sp_.buildInternalServerError();
        status_page_ = true;
        return false;
    }

    // Vse probehlo spravne --> vracim 204 No Content
    packet_builder_sp_.buildNoContent();
    status_page_ = true;
    Config::orparamsRemove(rparam_);
    rparam_ = nullptr;
    return true;
}


void Http1_0::handleConnection()
{
    try
    {
        //LOG_DBG("Http1_0::handleConnection");

        if (isConnected())
        {
            int ret;
            //LOG_DBG("Running Http1_0::handleConnection...");

            // Vytvorit adresar pro temp files
            if (!this->createTempDirectory()) 
            {
                packet_builder_sp_.buildInternalServerError();
                status_page_ = true;
                goto send_response;
            }

            // Vygenerovat nazev pro temp file
            this->generateTempFilePath(0);
            temp_file_ = &temp_files_.at(0);

            // Prijmout request
            ret = receiveRequest();
            if (ret == 0) {
                goto end_conn;
            }
            else if (ret == -1) {
                goto send_response;
            }

            // Sestaveni odpovedi
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
            }

    send_response:
            // Poslat odpoved
            if (status_page_)
            {
                this->sendResponse(packet_builder_sp_.packet());
                if (rparam_) 
                {
                    bool remove_orparam = true;
                    HttpStatusCode hsc = 
                        packet_builder_sp_.packet().header().statusCode();
                    switch (hsc)
                    {
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
            }
            
            else 
            {
                if (!packet_builder_.packet().header().hasEnd()) {
                    packet_builder_.packet().header().end();
                }
                this->sendResponse(packet_builder_.packet());
                // Prevence vuci ponechani resourcu v nekonzistentnim stavu
                if (rparam_ && !rparam_->isSet()) {
                    const_cast<Config::RParams*>(rparam_)->update();
                }
            }

            goto end_conn;
        }
    }

    catch (const std::exception& exc)
    {
        LOG_ERR("Error: %s", exc.what());
            
        if (rparam_) { 
            const_cast<Config::RParams*>(rparam_)->unlock();
            rparam_ = nullptr;
        }
        packet_builder_sp_.buildInternalServerError();
        this->sendResponse(packet_builder_sp_.packet());
    }

end_conn:
    try
    {
        // Ukoncit spojeni
        if (rparam_) 
        { 
            //LOG_DBG("Unlocking rparam...");
            const_cast<Config::RParams*>(rparam_)->unlock();
            rparam_ = nullptr;
            //LOG_DBG("RParam unlocked");
        }
        //LOG_DBG("Http1_0 -> ending connection...");
        this->endConnection();
        //LOG_DBG("Http1_0 -> connection ended");
    }
    catch (const std::exception& e) {
    }
}

bool Http1_0::compressDataToSend(std::string& dec_data, const void* data, const size_t data_size)
{
    const bool is_gzip = ((content_encoding_ == HttpContentEncoding::GZIP) || 
        (content_encoding_ == HttpContentEncoding::X_GZIP));

    if (!Codec::compress_data(dec_data, data, data_size, is_gzip))
    {
        //LOG_DBG("Failed to compress data to send");
        packet_builder_sp_.buildInternalServerError();
        status_page_ = true;
        return false;
    }

    return true;
}

int Http1_0::sendFileChunk(const uint64_t offset, const int file_fd, const uint32_t chunk_size) 
{
    int ret = 1;

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        return -1;
    }

    uint64_t page_aligned_offset = offset - (offset % page_size);
    uint64_t offset_diff = offset - page_aligned_offset;
    uint64_t mapping_size = chunk_size + offset_diff;

    void* mapping = mmap(NULL, mapping_size, PROT_READ, MAP_PRIVATE, file_fd, page_aligned_offset);
    if (mapping == MAP_FAILED) {
        //LOG_DBG("mmap failed");
        return -1;
    }

    void* data_to_send = static_cast<char*>(mapping) + offset_diff;
    size_t send_size = chunk_size;

    std::string dec_data;
    if (content_encoding_ != HttpContentEncoding::NONE) 
    {
        if (!compressDataToSend(dec_data, data_to_send, chunk_size)) 
        {
            //LOG_DBG("compress data failed");
            ret = -1;
            goto end_send;
        }
        data_to_send = const_cast<void*>(static_cast<const void*>(dec_data.data()));
        send_size = dec_data.size();

        if (http_version_ != HttpVersion::HTTP_1_0)
        {        
            char chunk_size_hex[50];
            snprintf(chunk_size_hex, sizeof(chunk_size_hex), "%zX\r\n", send_size);
            ret = tcp_server_->sendText(this->tcp_connection_, chunk_size_hex, strlen(chunk_size_hex));
            if (ret != 1) {
                goto end_send;
            }
        }
    }

    ret = tcp_server_->sendText(this->tcp_connection_, 
        static_cast<const char*>(data_to_send), send_size);
    if (ret <= 0) {
        goto end_send;
    }

    if (content_encoding_ != HttpContentEncoding::NONE &&
        http_version_ != HttpVersion::HTTP_1_0) {
        ret = tcp_server_->sendText(this->tcp_connection_, "\r\n");
    }

end_send:
    munmap(mapping, mapping_size);
    return ret;
}


bool Http1_0::sendResponse(HttpPacketBase& packetb)
{
    HttpPacket& packet = dynamic_cast<HttpPacket&>(packetb);
    const HttpPacket::Body::Data* packet_body;
    int file_fd = -1;

    // Kontrola zda neposilam prazdny packet
    if (packet.header().data().empty()) {
        return true;
    }

    // Uprava hlavicky pred jejim odeslanim
    if (content_encoding_ != HttpContentEncoding::NONE)
    {
        if (http_version_ != HttpVersion::HTTP_1_0)
        {
            packet.header().removeEnd();
            packet.header().removeContentLength();
            packet.header().transferEncoding(content_encoding_str_ + ", chunked");
            packet.header().end();
        }
    }

    // Odeslani hlavicky packetu
    int send_ret;
    if ((send_ret = tcp_server_->sendText(this->tcp_connection_, 
        packet.header().data())) == -1) 
    {
        goto err;
    }
    else if (send_ret == 0) {
        return true;
    }

    // Odeslani tela packetu
    packet_body = &packet.body().data();
    if (!packet_body->is_file_ && !packet_body->data_.empty())  // Odesilam jen pokud ma telo packetu nejaka data
    {
        const std::string* data_to_send = &packet_body->data_;

        // Kompresovat data
        std::string dec_data;
        if (content_encoding_ != HttpContentEncoding::NONE)
        {
            if (!compressDataToSend(dec_data, 
                data_to_send->data(), data_to_send->size())) 
            {
                goto err;
            }
            data_to_send = &dec_data;

            if (http_version_ != HttpVersion::HTTP_1_0)
            {
                char chunk_size_hex[50];
                snprintf(chunk_size_hex, sizeof(chunk_size_hex), "%zX\r\n", data_to_send->size());
                send_ret = tcp_server_->sendText(this->tcp_connection_, chunk_size_hex, strlen(chunk_size_hex));
                if (send_ret == -1) {
                    goto err;
                }
                else if (send_ret == 0) {
                    return true;
                }
            }
        }

        // Odeslani dat
        send_ret = tcp_server_->sendText(this->tcp_connection_, *data_to_send);
        if (send_ret == -1) {
            goto err;
        }
        else if (send_ret == 0) {
            return true;
        }

        if (content_encoding_ != HttpContentEncoding::NONE &&
            http_version_ != HttpVersion::HTTP_1_0) 
        {
            send_ret = tcp_server_->sendText(this->tcp_connection_, "\r\n");
            if (send_ret == -1) {
                goto err;
            }
            else if (send_ret == 0) {
                return true;
            }
        }
    }
    // Odesilam soubor jen pokud ho mam odesilat, tedy i prave pokud odpovidam na HEAD request
    else if (packet_body->is_file_ && !packet.header().isHeadMethod())
    {
        file_fd = open(packet_body->data_.c_str(), O_RDONLY | O_NOCTTY);
        if (file_fd == -1) 
        {
            LOG_ERR("Failed to open file to send (file: %s)", packet_body->data_.c_str());
            goto err;
        }

        uint64_t sent_bytes = 0;
        uint64_t chunk_size;
        while (sent_bytes < packet_body->content_length_)
        {
            chunk_size = std::min(packet_body->content_length_ - sent_bytes, 
                    static_cast<uint64_t>(Config::params().file_chunk_size)); 

            send_ret = sendFileChunk(sent_bytes, file_fd, static_cast<uint32_t>(chunk_size));
            if (send_ret == -1)
            {
                close(file_fd);
                goto err;
            }
            else if (send_ret == 0) {
                close(file_fd);
                return true;
            }

            sent_bytes += chunk_size;
        }
    
        close(file_fd);
    }

    // Zakonceni transfer encoding
    if (content_encoding_ != HttpContentEncoding::NONE &&
        http_version_ != HttpVersion::HTTP_1_0) 
    {
        send_ret = tcp_server_->sendText(this->tcp_connection_, "0\r\n\r\n");
        if (send_ret == -1) {
            goto err;
        }
        else if (send_ret == 0) {
            return true;
        }
    }

    return true;

err:
    // V pripade chyby behem zasilani dat jednoduse jen vratim z funkce false a uzivateli nic nesdeluju -> prohlizec bude cekat dokud nedostane vsechna data, ale nikdy je nedostane (-> refresh)
    if (file_fd != -1) { close(file_fd); }
    return false;
}


int Http1_0::receiveRequest()
{
    //LOG_DBG("Http1_0::receiveRequest()...");

    std::string buffer;
    int ret;

    // Nacteni hlavicky HTTP requestu
    ret = this->tcp_server_->receiveText(this->tcp_connection_, buffer, 
        Config::params().max_header_size, HEADERS_END, false);
    ret = receiveRetCheck(ret, -1);
    if (ret != 1) {
        return ret;
    }

    //LOG_DBG("Http1_0::receiveRequest(term) got data (ret: %d)", ret);
    ////LOG_DBG("Headers end index: %z", (size_t) buffer.find(HEADERS_END));
    LOG_DBG("Recvd data: %s\n", buffer.c_str());

    // Prekopirovani hlavicky
    request_data_ = std::move(buffer);

    // Rozparsovani a kontrola HTTP requestu
    httpparser::Request request;
    if (!httpParseRequest(request_data_, request))
    {
        packet_builder_sp_.buildBadRequest();
        status_page_ = true;
        return -1;
    }
    moveHttpRequest(request_, request);
    request_uri_ = request_.uri;

    if (request_uri_.empty()) {
        packet_builder_sp_.buildBadRequest();
        status_page_ = true;
        return -1;
    }
    if (!prepareRequestUri()) {
        return -1;
    }

    //LOG_DBG("Parsed request");
    ////LOG_DBG("URI: %s", request_uri_.c_str());
        
    // Nacteni HTTP metody requestu
    request_method_ = httpMethod(request_.method, this->http_version_);
    if (request_method_ == HttpMethod::NOT_ALLOWED)
    {
        packet_builder_sp_.buildNotImplemented();
        status_page_ = true;
        return -1;
    }

    //LOG_DBG("validateResource()...");
    ret = validateResource();
    if (ret == -1 || ret == -3) 
    {
        packet_builder_sp_.buildNotFound();
        status_page_ = true;
        return -1;
    }
    else if (ret == -2)
    {
        packet_builder_sp_.buildForbidden();
        status_page_ = true;
        return -1;
    }
    //LOG_DBG("validateResource() done");

    //LOG_DBG("getResourceParam()...");
    if (!getResourceParam()) 
    {
        packet_builder_sp_.buildInternalServerError();
        status_page_ = true;
        return -1;
    }
    //LOG_DBG("getResourceParam() done");

    //LOG_DBG("checkResourceConstraints()...");
    if (!checkResourceConstraints()) {
        return -1;
    }
    //LOG_DBG("checkResourceConstraints() done");
    
    // Zda budu prijimat i telo HTTP requestu (metody POST, PUT)
    return receiveRequestBody();
}

int Http1_0::receiveRequestBody()
{
    // Zjistit HTTP metodu
	if (request_method_ != HttpMethod::POST && 
        request_method_ != HttpMethod::PUT)
	{
		return 1;
	}

    //LOG_DBG("recv_body...");

    if (request_method_ == HttpMethod::POST) 
    {
        if (!this->requestPostMethod()) {
            return -1;
        }

        int ret = receiveRequestBodyPostMethod();
        if (ret == -1) {
            return -1;
        }
        else if (ret == 0) {
            return 0;
        }
    }
    else if (request_method_ == HttpMethod::PUT) 
    {
        if (!this->requestPutMethod()) {
            return -1;
        }

        int ret = receiveRequestBodyPutMethod();
        if (ret == -1) {
            return -1;
        }
        else if (ret == 0) {
            return 0;
        }
    }

    return 1;
}

int Http1_0::receiveRetCheck(const int ret, const int temporary_file_fd)
{
    // Klient se odpojil
    if (ret == 0) 
    {
        if (temporary_file_fd != -1) {
            this->deleteTemporaryFile(temporary_file_fd, *temp_file_);
        }
        return 0;
    }
    // Chyba
    else if (ret == -1)
    {
        if (temporary_file_fd != -1) {
            this->deleteTemporaryFile(temporary_file_fd, *temp_file_);
        }
        packet_builder_sp_.buildInternalServerError();
        status_page_ = true;
        return -1;
    }
    else if (ret == -2)
    {
        if (temporary_file_fd != -1) {
            this->deleteTemporaryFile(temporary_file_fd, *temp_file_);
        }
        packet_builder_sp_.buildContentTooLarge();
        status_page_ = true;
        return -1;
    }

    return 1;
}

int Http1_0::receiveRequestBodyPutMethod()
{
    uint64_t content_length;
    if (headersContentLength(&content_length) == -1) {
        return -1;
    }
    //LOG_DBG("content-len: %" PRIu64 "\nclient_max_body_size: %" PRIu64, content_length, Config::params().client_max_body_size);

    if (content_length > Config::params().client_max_body_size) 
    {
        packet_builder_sp_.buildContentTooLarge();
        status_page_ = true;
        return -1;
    }
    //LOG_DBG("Length processed");

    // Kontrola toho zda neni telo HTTP requestu vetsi nez nastavena velikost bufferu pro prijem tela
    int temporary_file_fd;
    if (content_length > Config::params().client_body_buffer_size)
    {
        // Pokud je, tak ukladam do temporary file
        if (!this->createTemporaryFile(temporary_file_fd, *const_cast<Http::TempFile*>(temp_file_))) 
        {
            packet_builder_sp_.buildInternalServerError();
            status_page_ = true;
            return -1;
        }
    }

    // Nacteni tela HTTP requestu
    uint64_t total = 0;
    std::string buffer;

    while (total < content_length)
    {
        buffer.resize(std::min(content_length, static_cast<uint64_t>(Config::params().client_body_buffer_size)));
        const uint64_t chunk_size = std::min(buffer.size(), content_length - total);
        int ret = this->tcp_server_->receiveText(this->tcp_connection_, buffer, chunk_size, false);
        ret = receiveRetCheck(ret, temporary_file_fd);
        if (ret != 1) {
            return ret;
        }

        // Dekompresovat data
        std::string enc_data;
        if (content_encoding_ != HttpContentEncoding::NONE)
        {
            const bool is_gzip = ((content_encoding_ == HttpContentEncoding::GZIP) || 
                                (content_encoding_ == HttpContentEncoding::X_GZIP));

            if (!Codec::decompress_string(enc_data, buffer, is_gzip))
            {
                //LOG_DBG("Failed to decompress received data");
                packet_builder_sp_.buildInternalServerError();
                status_page_ = true;
                return -1;
            }
            buffer = std::move(enc_data);
        }

        //LOG_DBG("Http1_0::receiveRequest(size) got data (ret: %d)", ret);
        ////LOG_DBG("Recvd data: %s\n", buffer.c_str());

        // Ulozeni casti tela HTTP requestu
        if (temp_file_->data_in_temp_file_) {
            write(temporary_file_fd, buffer.data(), chunk_size);
        }
        else {
            request_data_.append(buffer);
        }

        total += chunk_size;
    }

    if (temp_file_->data_in_temp_file_)
    {
        if (fsync(temporary_file_fd) == -1)
        {
            LOG_ERR("Failed to store request body into temp file (error: %s)", strerror(errno));
            this->deleteTemporaryFile(temporary_file_fd, *temp_file_);
            close(temporary_file_fd);
            packet_builder_sp_.buildInternalServerError();
            status_page_ = true;
            return -1;
        }

        close(temporary_file_fd);
    }

    if (!this->requestPutMethodFunc()) {
        return -1;
    }

    return 1;
}

int Http1_0::receiveRequestBodyPostMethod()
{
    static const std::string content_disp = "Content-Disposition:";
    static const std::string content_disp_filename = "filename=";
    const std::string boundary = "--" + boundary_;
    const std::string end_boundary = boundary + "--";
    int ret;

    size_t boundary_ind = request_data_.find(boundary);

    // Kontrola zda POST request nebyl zaslan v hlavicce
    if (boundary_ind != std::string::npos)
    {
        const size_t cont_disp_ind = request_data_.find(content_disp, boundary_ind);
        if (cont_disp_ind == std::string::npos)
        {
            packet_builder_sp_.buildBadRequest();
            status_page_ = true;
            return -1;
        }

        const size_t headers_end_ind = request_data_.find(HEADERS_END, cont_disp_ind);
        if (headers_end_ind == std::string::npos)
        {
            packet_builder_sp_.buildBadRequest();
            status_page_ = true;
            return -1;
        }

        ret = httpCheckContentDisposition(request_data_.substr(cont_disp_ind, headers_end_ind-cont_disp_ind));
        if (ret == -1)
        {
            packet_builder_sp_.buildInternalServerError();
            status_page_ = true;
            return -1;
        }
        else if (ret == -2)
        {
            return 1;
        }

        const size_t filename_ind = request_data_.find(content_disp_filename, cont_disp_ind);
        const size_t quote1_ind = request_data_.find("\"", filename_ind);
        const size_t quote2_ind = request_data_.find("\"", quote1_ind+1);
        file_name_ = request_data_.substr(quote1_ind+1, quote2_ind-quote1_ind+1);

        // Ulozim telo
        std::string resource_data;
        const size_t value_ind = headers_end_ind+strlen(HEADERS_END);

        boundary_ind = request_data_.find(boundary, value_ind);
        if (boundary_ind == std::string::npos)
        {
            packet_builder_sp_.buildBadRequest();
            status_page_ = true;
            return -1;
        }

        resource_data = request_data_.substr(value_ind, boundary_ind-value_ind);
        request_data_ = std::move(resource_data);

        if (!this->requestPutMethodFunc()) {
            return -1;
        }

        // Vyndani poslednich dat
        std::string buffer;
        this->tcp_server_->receiveText(this->tcp_connection_, buffer, end_boundary.size(), end_boundary, false);
    }

    return 1;
}

void Http1_0::removeRequestDataHeaders()
{
    const size_t headers_end_ind = request_data_.find(HEADERS_END);
    const size_t erase_count = headers_end_ind + sizeof(HEADERS_END) - 1;
    request_data_.erase(0, erase_count);
}


bool Http1_0::checkResourceConstraints()
{
    if (rparam_)
    {
        // Kontrola HTTP metody
        const auto meth_it = std::find(rparam_->methods_allowed.cbegin(), rparam_->methods_allowed.cend(), request_.method);
        if (meth_it == rparam_->methods_allowed.cend())
        {
            packet_builder_sp_.buildMethodNotAllowed(rparam_);
            status_page_ = true;
            return false;
        }
    }

    return true;
}

bool Http1_0::prepareRequestUri()
{
    static const std::string http_str = "http://";
    static const std::string https_str = "https://";

    if (request_uri_ == HTTP_ASTERISK) 
    {
        if (request_.method != HTTP_METHOD_OPTIONS)
        {
            packet_builder_sp_.buildBadRequest();
            status_page_ = true;
            return false;
        }

        packet_builder_sp_.buildServerOptions();
        status_page_ = true;
        return false;
    }

    else if (request_uri_ == DEFAULT_WEB_PAGE) {
        request_uri_ += "index.html";
	}

    else if (request_uri_.find(http_str) != std::string::npos) {
        request_uri_.erase(0, http_str.size() - 1);
    }

    else if (request_uri_.find(https_str) != std::string::npos) {
        request_uri_.erase(0, https_str.size() - 1);
    }

    return true;
}

void Http1_0::moveHttpRequest(httpparser::Request& req, const httpparser::Request& req2)
{
	req.method = std::move(req2.method);
	req.uri = std::move(req2.uri);
	req.versionMajor = req2.versionMajor;
	req.versionMinor = req2.versionMinor;
	req.headers = std::move(req2.headers);
    req.content = std::move(req2.content);
	req.keepAlive = req2.keepAlive;
}

void Http1_0::resetHttpRequest(httpparser::Request& req)
{
    req.method.clear();
	req.uri.clear();
	req.versionMajor = 0;
	req.versionMinor = 0;
	req.headers.clear();
    req.content.clear();
	req.keepAlive = false;
}