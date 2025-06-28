#include "HttpGlobal.hpp"
#include "httprequestparser.h"
#include <algorithm>
#include <string.h>
#include <regex.h>
/// TODO: odendat
#include "Logger.hpp"


const std::string DEFAULT_WEB_PAGE = "/";

const std::string HTTP_VERSION_1_0_STR = "1.0";
const std::string HTTP_VERSION_1_1_STR = "1.1";
const std::string HTTP_VERSION_UNSUPPORTED = "unsupported";
//const std::string HTTP_VERSION_2_0_STR = "2.0";

const std::string HTTP_METHOD_GET = "GET";
const std::string HTTP_METHOD_POST = "POST";
const std::string HTTP_METHOD_PUT = "PUT";
const std::string HTTP_METHOD_DELETE = "DELETE";
const std::string HTTP_METHOD_HEAD = "HEAD";
//const std::string HTTP_METHOD_PATCH = "PATCH";
const std::string HTTP_METHOD_OPTIONS = "OPTIONS";
const std::string HTTP_ALL_SUPPORTED_METHODS = "GET, POST, PUT, DELETE, HEAD, OPTIONS";


const std::map<std::string, HttpContentTypeS> http_content_types = 
{
	{ "", HttpContentTypeS{ HttpContentType::FORM_DATA, "multipart/form-data" } },
    { ".pdf", HttpContentTypeS{ HttpContentType::PDF, "application/pdf" } },
    { ".json", HttpContentTypeS{ HttpContentType::JSON, "application/json" } },
    { ".js", HttpContentTypeS{ HttpContentType::JAVASCRIPT, "application/javascript" } },
    { ".xml", HttpContentTypeS{ HttpContentType::XML, "application/xml" } },
    { ".zip", HttpContentTypeS{ HttpContentType::ZIP, "application/zip" } },
    { ".jar", HttpContentTypeS{ HttpContentType::JAR, "application/java-archive" } },
    { ".7z", HttpContentTypeS{ HttpContentType::SEVEN_ZIP, "application/x-7z-compressed" } },
    { ".xhtml", HttpContentTypeS{ HttpContentType::XHTML, "application/xhtml+xml" } },
    { ".xls", HttpContentTypeS{ HttpContentType::XLS, "application/vnd.ms-excel" } },
    { ".xlsx", HttpContentTypeS{ HttpContentType::XLSX, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" } },
    { ".doc", HttpContentTypeS{ HttpContentType::DOC, "application/msword" } },
    { ".docx", HttpContentTypeS{ HttpContentType::DOCX, "application/vnd.openxmlformats-officedocument.wordprocessingml.document" } },
    
    { ".gif", HttpContentTypeS{ HttpContentType::GIF, "image/gif" } },
    { ".jpg", HttpContentTypeS{ HttpContentType::JPEG, "image/jpeg" } },
    { ".jpeg", HttpContentTypeS{ HttpContentType::JPEG, "image/jpeg" } },
    { ".png", HttpContentTypeS{ HttpContentType::PNG, "image/png" } },
    { ".ico", HttpContentTypeS{ HttpContentType::X_ICON, "image/x-icon" } },
    { ".avif", HttpContentTypeS{ HttpContentType::AVIF, "image/avif" } },
    
    { ".css", HttpContentTypeS{ HttpContentType::CSS, "text/css" } },
    { ".csv", HttpContentTypeS{ HttpContentType::CSV, "text/csv" } },
    { ".htm", HttpContentTypeS{ HttpContentType::HTML, "text/html" } },
    { ".html", HttpContentTypeS{ HttpContentType::HTML, "text/html" } },
    { ".txt", HttpContentTypeS{ HttpContentType::PLAIN, "text/plain" } },
    
    { ".mp3", HttpContentTypeS{ HttpContentType::MP3, "audio/mpeg" } },
    
    { ".mpeg", HttpContentTypeS{ HttpContentType::MPEG, "video/mpeg" } },
    { ".mp4", HttpContentTypeS{ HttpContentType::MP4, "video/mp4" } }
};

const HttpContentTypeS* httpContentType(const std::string& resource_file_suffix)
{
	const auto it = http_content_types.find(resource_file_suffix);
	if (it == http_content_types.end()) { return nullptr; }
	return &it->second;
}

const char* httpContentTypeSuffix(const std::string& content_type)
{
	const auto it = std::find_if(http_content_types.cbegin(), http_content_types.cend(), 
	[&content_type](const std::pair<std::string, HttpContentTypeS>& ct)
	{
		return (strcmp(content_type.c_str(), ct.second.content_type_label) == 0);
	});

	if (it == http_content_types.cend()) { return nullptr; }
	return it->second.content_type_label;
}


const std::map<HttpStatusCode, const char*> http_status_codes = 
{
	{ HttpStatusCode::CONTINUE, "Continue" },
	{ HttpStatusCode::OK, "OK" },
	{ HttpStatusCode::CREATED, "Created" },
	{ HttpStatusCode::NO_CONTENT, "No Content" },
	{ HttpStatusCode::PARTIAL_CONTENT, "Partial Content" },
	{ HttpStatusCode::NOT_MODIFIED, "Not Modified" },
	{ HttpStatusCode::BAD_REQUEST, "Bad Request" },
	{ HttpStatusCode::FORBIDDEN, "Forbidden" },
	{ HttpStatusCode::NOT_FOUND, "Not Found" },
	{ HttpStatusCode::METHOD_NOT_ALLOWED, "Method Not Allowed" },
	{ HttpStatusCode::NOT_ACCEPTABLE, "Not Acceptable" },
	{ HttpStatusCode::LENGTH_REQUIRED, "Length Required" },
	{ HttpStatusCode::PRECONDITION_FAILED, "Precondition Failed" },
	{ HttpStatusCode::CONTENT_TOO_LARGE, "Content Too Large" },
	{ HttpStatusCode::UNSUPPORTED_MEDIA_TYPE, "Unsupported Media Type" },
	{ HttpStatusCode::RANGE_NOT_SATISFIABLE, "Range Not Satisfiable" },
	{ HttpStatusCode::EXPECTATION_FAILED, "Expectation Failed" },
	{ HttpStatusCode::INTERNAL_SERVER_ERROR, "Internal Server Error" },
	{ HttpStatusCode::NOT_IMPLEMENTED, "Not Implemented" },
	{ HttpStatusCode::SERVICE_UNAVAILABLE, "Service Unavailable" },
	{ HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED, "HTTP Version Not Supported" }
};

const char* httpStatusCode(const HttpStatusCode code)
{	
	const auto item = http_status_codes.find(code);
	if (item == http_status_codes.end()) { return nullptr; }
	return item->second;
}


const std::string& httpVersion(const HttpVersion version)
{
	switch (version)
	{
		case HttpVersion::HTTP_1_0:
			return HTTP_VERSION_1_0_STR;
		case HttpVersion::HTTP_1_1:
			return HTTP_VERSION_1_1_STR;
		case HttpVersion::UNSUPPORTED:
			return HTTP_VERSION_UNSUPPORTED;
		/*case HttpVersion::HTTP_2_0:
			return HTTP_VERSION_2_0_STR;*/
	}

	return HTTP_VERSION_UNSUPPORTED;		
}

HttpVersion httpVersionStr(const std::string& version)
{
	if (version == HTTP_VERSION_1_0_STR) { return HttpVersion::HTTP_1_0; }
	else if (version == HTTP_VERSION_1_1_STR) { return HttpVersion::HTTP_1_1; }
	//else if (version == HTTP_VERSION_2_0_STR) { return HttpVersion::HTTP_2_0; }
	return HttpVersion::UNSUPPORTED;
}


HttpMethod httpMethod(const std::string& method, const HttpVersion& version)
{
	if (method == HTTP_METHOD_GET) { return HttpMethod::GET; }
	else if (method == HTTP_METHOD_POST) { return HttpMethod::POST; }
	else if (method == HTTP_METHOD_PUT) { return HttpMethod::PUT; }
	else if (method == HTTP_METHOD_DELETE) { return HttpMethod::DELETE; }
	else if (method == HTTP_METHOD_HEAD) { return HttpMethod::HEAD; }

	if (version != HttpVersion::HTTP_1_0)
	{
		if (method == HTTP_METHOD_OPTIONS) { return HttpMethod::OPTIONS; }
		// else if (method == HTTP_METHOD_PATCH) { return HttpMethod::PATCH; }
	}

	return HttpMethod::NOT_ALLOWED;
}

bool httpIsStateChangingMethod(const HttpMethod method)
{
	return (method == HttpMethod::POST || 
			method == HttpMethod::PUT || 
			method == HttpMethod::DELETE);
}


const std::map<std::string, HttpContentEncoding> content_encodings = 
{
	{ "gzip", HttpContentEncoding::GZIP },
	{ "x-gzip", HttpContentEncoding::X_GZIP },
	{ "deflate", HttpContentEncoding::DEFLATE }
};

const std::pair<const std::string, HttpContentEncoding>* httpContentEncoding(const std::string& encodings, const bool accept_encoding)
{
	const char* delimiters = ",; \r\n";

	if (accept_encoding && encodings == HTTP_ASTERISK) {
		return &(*content_encodings.find("gzip"));
	}

	std::vector<std::string> enc;
	char encodings_cpy[encodings.size() + 1] = {0};
	strncpy(encodings_cpy, encodings.c_str(), encodings.size());
	char* saveptr = nullptr;

    char* token = strtok_r(encodings_cpy, delimiters, &saveptr);
	if (token == nullptr) {
		enc.push_back(encodings_cpy);
	}
	else
	{
		do
		{
			if (strstr(token, "q=") != NULL) 
			{
				if (!accept_encoding) {
					return nullptr;
				}
			}
			else 
			{
				enc.push_back(token);
			}

			token = strtok_r(nullptr, delimiters, &saveptr);
		} while (token != nullptr);
	}

	/*LOG_DBG("\nEncodings:");
	for (const std::string& eee : enc) {
		LOG_DBG("%s", eee.c_str());
	}
	LOG_DBG("");*/

	const std::pair<const std::string, HttpContentEncoding>* encoding_res = nullptr;
	for (const auto& ce : content_encodings)
	{
		const auto enc_it = std::find_if(enc.cbegin(), enc.cend(), 
		[&ce](const std::string& it)
		{
			return (ce.first == it);
		});

		if (enc_it != enc.cend())
		{
			encoding_res = &ce;
			break;
		}
	}

	return encoding_res;
}


int httpCheckRangeHeader(const std::string& ranges)
{
	regex_t regex;
	if (regcomp(&regex, HTTP_RANGE_HEADER_REGEX_PATTERN, REG_EXTENDED) != 0) {
		return -1;
	}

	if (regexec(&regex, ranges.c_str(), 0, NULL, 0) != 0) {
		return -2;
	}

	return 0;
}


bool httpParseRequest(const std::string& request_data, httpparser::Request& request)
{
	httpparser::HttpRequestParser parser;
	const httpparser::HttpRequestParser::ParseResult parse_result = 
		parser.parse(request, request_data.c_str(), request_data.c_str() + request_data.size());
	
	if (parse_result != httpparser::HttpRequestParser::ParsingCompleted) {
		return false;
	}

	return true;
}


int httpCheckRequestLine(const std::string& request_data)
{
	const char* patterns[REQUEST_LINE_PATTERN_COUNT] = { 
		REQUEST_LINE_PATTERN_PART1, 
		REQUEST_LINE_PATTERN_PART2, 
		REQUEST_LINE_PATTERN_PART3 
	};

	regex_t regex[REQUEST_LINE_PATTERN_COUNT];
	for (int i = 0; i < REQUEST_LINE_PATTERN_COUNT; ++i) 
	{
		if (regcomp(&regex[i], patterns[i], REG_EXTENDED) != 0) {
			return -1;
		}
	}

	std::string request_line_parts[REQUEST_LINE_PATTERN_COUNT];
	size_t last_part_index = 0;
	for (int i = 1; i <= REQUEST_LINE_PATTERN_COUNT; ++i)
	{
		if (last_part_index >= request_data.size()) {
			return -2;
		}

		const size_t part_index = request_data.find(' ', last_part_index);
		if (part_index == std::string::npos && 
			i < REQUEST_LINE_PATTERN_COUNT)
		{
			return -2;
		}

		request_line_parts[i-1] = request_data.substr(last_part_index, part_index);
		last_part_index = part_index + 1;
	}

	int ret = 0;
	for (int i = 0; i < REQUEST_LINE_PATTERN_COUNT; ++i)
	{
		if (regexec(&regex[i], request_line_parts[i].c_str(), 0, NULL, 0) != 0)
		{
			ret = -3;
			break;
		}
	}

	for (int i = 0; i < REQUEST_LINE_PATTERN_COUNT; ++i) {
		regfree(&regex[i]);
	}

    return ret;
}


int httpCheckContentDisposition(const std::string& content_disp)
{
	int ret = 0;
	regex_t regex;
	if (regcomp(&regex, HTTP_CONTENT_DISPOSITION_PATTERN_FILENAME, REG_EXTENDED) != 0) {
		return -1;
	}

	if (regexec(&regex, content_disp.c_str(), 0, NULL, 0) != 0) {
		ret = -2;
	}

	regfree(&regex);
	return ret;
}
