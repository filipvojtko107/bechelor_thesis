#include "HttpPacket.hpp"
#include "Logger.hpp"
#include <cstdio>


std::string& operator<<(std::string& str, const std::string& str_append)
{
	str.append(str_append);
	return str;
}

std::string& operator<<(std::string& str, const char* str_append)
{
	str.append(str_append);
	return str;
}

void HttpPacket::Header::reset()
{
	data_.clear();
	is_head_method_ = false;
	is_connection_closed_ = false;
	has_end_ = false;
	http_version_ = HttpVersion::UNSUPPORTED;
    status_code_ = HttpStatusCode::UNSUPPORTED;
}

void HttpPacket::Body::reset()
{
	data_.is_file_ = false;
	data_.data_.clear();
	data_.content_length_ = 0;
}

void HttpPacket::reset()
{
	header_.reset();
	body_.reset();
}

void HttpPacket::Header::statusLine(const HttpVersion version, const HttpStatusCode status_code)
{
	char status_code_str[10];
	snprintf(status_code_str, sizeof(status_code_str), "%d", static_cast<int>(status_code));
	data_ << "HTTP/" << httpVersion(version) << " " << status_code_str << " " << httpStatusCode(status_code) << HEADERS_ENDLINE;
	http_version_ = version;
	status_code_ = status_code;
}

void HttpPacket::Header::server(const std::string& name)
{
	data_ << "Server: " << name << HEADERS_ENDLINE;
}

void HttpPacket::Header::connection(const std::string& conn)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	if (conn == "close") {
		is_connection_closed_ = true;
	}
	data_ << "Connection: " << conn << HEADERS_ENDLINE;
}

void HttpPacket::Header::contentType(const char* content_type)
{
	data_ << "Content-Type: " << content_type << HEADERS_ENDLINE;
}

void HttpPacket::Header::contentLength(const uint64_t len)
{
	char len_str[50];
	snprintf(len_str, sizeof(len_str), "%" PRIu64, len);
	data_ << "Content-Length: " << len_str << HEADERS_ENDLINE;
}

void HttpPacket::Header::date(const char* dt)
{
	data_ << "Date: " << dt << HEADERS_ENDLINE;
}

void HttpPacket::Header::contentEncoding(const std::string& encoding)
{
	data_ << "Content-Encoding: " << encoding << HEADERS_ENDLINE;
}

void HttpPacket::Header::vary()
{
	data_ << "Vary: " << HTTP_CONTENT_NEGOTIATION_FIELDS << HEADERS_ENDLINE;
}

void HttpPacket::Header::allow(const std::string& methods_allowed)
{
	data_ << "Allow: " << methods_allowed << HEADERS_ENDLINE;
}

void HttpPacket::Header::acceptRanges(const char* ranges)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	data_ << "Accept-Ranges: " << ranges << HEADERS_ENDLINE;
}

void HttpPacket::Header::expires(const char* exp)
{
	data_ << "Expires: " << exp << HEADERS_ENDLINE;
}

void HttpPacket::Header::cacheControl(const std::string& cache_control, const int32_t max_age)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	data_ << "Cache-Control: " << cache_control;
	if (cache_control == "no-store") { data_ << ", must-understand"; }
	if (max_age > 0) 
	{ 
		char max_age_str[25];
		snprintf(max_age_str, sizeof(max_age_str), "%" PRIi32, max_age);
		data_ << ", max-age=" << max_age_str << ", must-revalidate";

		// shared cache
		if (cache_control  == "public") { data_ << ", s-maxage=" << max_age_str << ", proxy-revalidate"; }
	}
	data_ << HEADERS_ENDLINE;
}

void HttpPacket::Header::lastModified(const char* last_modified)
{
	data_ << "Last-Modified: " << last_modified << HEADERS_ENDLINE;
}

void HttpPacket::Header::etag(const ETag e)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	char etag_str[50];
	snprintf(etag_str, sizeof(etag_str), "%s", e.c_str());
	data_ << "ETag: " << etag_str << HEADERS_ENDLINE;
}

void HttpPacket::Header::location(const std::string& rel_path)
{
	data_ << "Location: " << rel_path << HEADERS_ENDLINE;
}

void HttpPacket::Header::contentLocation(const std::string& rel_path)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	data_ << "Content-Location: " << rel_path << HEADERS_ENDLINE;
}

void HttpPacket::Header::contentRange(const std::string& range, const uint64_t size)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	data_ << "Content-Range: bytes " << range << "/" << std::to_string(size) << HEADERS_ENDLINE;
}

void HttpPacket::Header::contentRange(const uint64_t start, const uint64_t end, const uint64_t size)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	data_ << "Content-Range: bytes " << std::to_string(start) << "-" << 
		std::to_string(end) << "/" << std::to_string(size) << HEADERS_ENDLINE;
}

void HttpPacket::Header::transferEncoding(const std::string& transfer_encoding)
{
	if (http_version_ != HttpVersion::HTTP_1_1) {
		return;
	}
	data_ << "Transfer-Encoding: " << transfer_encoding << HEADERS_ENDLINE;
}

void HttpPacket::Header::end()
{
	data_ << HEADERS_ENDLINE;
	has_end_ = true;
}

void HttpPacket::Header::removeContentLength()
{
	size_t pos;
	if ((pos = data_.rfind("Content-Length")) != std::string::npos) 
	{
		const size_t count = data_.find(HEADERS_ENDLINE, pos) - pos + sizeof(HEADERS_ENDLINE)-1;
		data_.erase(pos, count);
	}
}

void HttpPacket::Header::removeContentRange()
{
	size_t pos;
	if ((pos = data_.rfind("Content-Range")) != std::string::npos) 
	{
		const size_t count = data_.find(HEADERS_ENDLINE, pos) - pos + sizeof(HEADERS_ENDLINE)-1;
		data_.erase(pos, count);
	}
}

void HttpPacket::Header::removeEnd()
{
	size_t pos;
	if ((pos = data_.rfind(HEADERS_END)) != std::string::npos) {
		data_.erase(pos + sizeof(HEADERS_ENDLINE)-1);
	}
}

bool HttpPacket::Body::addData(const std::string& data)
{
	data_.is_file_ = false;
	data_.data_ = data;
	data_.content_length_ = data.size();
	return true;
}

bool HttpPacket::Body::addData(std::string&& data)
{
	data_.is_file_ = false;
	data_.data_ = std::move(data);
	data_.content_length_ = data.size();
	return true;
}

bool HttpPacket::Body::addFile(const std::string& rel_path, const uint64_t file_size)
{
	std::string fpath = std::string(RESOURCES_DIR) + "/" + rel_path;
	data_.is_file_ = true;
	data_.data_ = std::move(fpath);
	data_.content_length_ = file_size;

	return true;
}
