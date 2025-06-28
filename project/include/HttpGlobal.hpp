#ifndef __HTTP_GLOBAL_HPP__
#define __HTTP_GLOBAL_HPP__
#include "HttpGlobal2.hpp"
#include "Globals.hpp"
#include "request.h"
#include <map>
#include <string>
#include <inttypes.h>

extern const std::string DEFAULT_WEB_PAGE;

extern const std::string HTTP_VERSION_1_0;
extern const std::string HTTP_VERSION_1_1;
extern const std::string HTTP_VERSION_UNSUPPORTED;
//extern const std::string HTTP_VERSION_2_0;

// Http/1.0
extern const std::string HTTP_METHOD_GET;
extern const std::string HTTP_METHOD_POST;
extern const std::string HTTP_METHOD_PUT;
extern const std::string HTTP_METHOD_DELETE;
extern const std::string HTTP_METHOD_HEAD;
// Http/1.1
//extern const std::string HTTP_METHOD_PATCH;
extern const std::string HTTP_METHOD_OPTIONS;

extern const std::string HTTP_ALL_SUPPORTED_METHODS;

// Jeste mozna match pro query string a port (:[0-9]{1,4})?(\\?([A-Za-z0-9_\\-]=*&)+)
#define REQUEST_LINE_PATTERN_PART1	"^(GET|POST|PUT|DELETE|HEAD|OPTIONS)$"
#define REQUEST_LINE_PATTERN_PART2	"^((\\*)|((/|http://|https://)?([A-Za-z0-9.-]|[/~:@?&=#%+!,;$*()\\[\\]])+))$"
#define REQUEST_LINE_PATTERN_PART3	"^(HTTP/(1\\.0|1\\.1))$"
#define REQUEST_LINE_PATTERN_COUNT	3

#define HTTP_ASTERISK	"*"

#define HEADERS_ENDLINE	"\r\n"
#define HEADERS_END	"\r\n\r\n"

#define TEMPORARY_DIR_NAME_FORMAT	(TEMPORARY_FILES_DIR "/%d")
#define TEMPORARY_FILE_NAME_FORMAT	(TEMPORARY_FILES_DIR "/%d/%" PRIu32)

#define HTTP_DATE_FORMAT	"%a, %d %b %Y %H:%M:%S GMT"

#define HTTP_CONTENT_NEGOTIATION_FIELDS		"Accept-Encoding"

#define HTTP_RANGE_HEADER_REGEX_PATTERN		"^bytes=(-[0-9]+|[0-9]+-[0-9]*)(,[[:space:]](-[0-9]+|[0-9]+-[0-9]*))*$"

//#define HTTP_CONTENT_DISPOSITION_PATTERN_NAME 	"^Content-Disposition:\\s*form-data;\\s*name=\"([^\"]+)\"\\s*;\\s*$"
#define HTTP_CONTENT_DISPOSITION_PATTERN_FILENAME	"^Content-Disposition:\\s*form-data;\\s*name=\"([^\"]+)\"\\s*;\\s*filename=\"([^\"]+)\"$"


enum class HttpContentType
{
	// multipart
	FORM_DATA,

	// application
	PDF,
	JSON,
	JAVASCRIPT,
	XML,
	ZIP,
	JAR,
	SEVEN_ZIP,
	XHTML,
	XLS,
	XLSX,
	DOC,
	DOCX,
	
	// image
	GIF,
	JPEG,
	PNG,
	X_ICON,
	AVIF,
	
	// text
	CSS,
	CSV,
	HTML,
	PLAIN,
	
	// audio
	MP3,
	
	// video
	MPEG,
	MP4
};


struct HttpContentTypeS
{
	HttpContentTypeS(const HttpContentType t, const char* lbl) :
		content_type_code(t),
		content_type_label(lbl)
	{
	}

	HttpContentType content_type_code;
	const char* content_type_label = nullptr;
};

extern const std::map<std::string, HttpContentTypeS> http_content_types;
const HttpContentTypeS* httpContentType(const std::string& resource_file_suffix);
const char* httpContentTypeSuffix(const std::string& content_type);

enum class HttpStatusCode
{
	CONTINUE = 100,
	OK = 200,
	CREATED = 201,
	NO_CONTENT = 204,
	PARTIAL_CONTENT = 206,
	NOT_MODIFIED = 304,
	BAD_REQUEST = 400,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	NOT_ACCEPTABLE = 406,
	LENGTH_REQUIRED = 411,
	PRECONDITION_FAILED = 412,
	CONTENT_TOO_LARGE = 413,
	UNSUPPORTED_MEDIA_TYPE = 415,
	RANGE_NOT_SATISFIABLE = 416,
	EXPECTATION_FAILED = 417,
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	SERVICE_UNAVAILABLE = 503,
	HTTP_VERSION_NOT_SUPPORTED = 505,
	UNSUPPORTED = -1
};

extern const std::map<HttpStatusCode, const char*> http_status_codes;
const char* httpStatusCode(const HttpStatusCode code);


enum class HttpVersion
{
	HTTP_1_0,
	HTTP_1_1,
	//HTTP_2_0,
	UNSUPPORTED
};

const std::string& httpVersion(const HttpVersion version);
HttpVersion httpVersionStr(const std::string& version);


enum class HttpMethod
{
	GET,
	POST,
	PUT,
	DELETE,
	HEAD,
	//PATCH,
	OPTIONS,
	NOT_ALLOWED
};

HttpMethod httpMethod(const std::string& method, const HttpVersion& version);
bool httpIsStateChangingMethod(const HttpMethod method);


enum class HttpContentEncoding
{
	GZIP,
	X_GZIP,
	DEFLATE,
	NONE
};

extern const std::map<std::string, HttpContentEncoding> content_encodings;
const std::pair<const std::string, HttpContentEncoding>* 
	httpContentEncoding(const std::string& encodings, const bool accept_encoding);

int httpCheckRangeHeader(const std::string& ranges);

bool httpParseRequest(const std::string& request_data, httpparser::Request& request);
int httpCheckRequestLine(const std::string& request_data);

int httpCheckContentDisposition(const std::string& content_disp);

#endif
