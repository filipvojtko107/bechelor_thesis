#ifndef __GLOBALS_HPP__
#define __GLOBALS_HPP__


#define CONFIG_FILES_DIR                                        "/etc/WebServerd"
#define CONFIG_FILE_FPATH						                CONFIG_FILES_DIR "/WebServerd.conf"
#define RESOURCES_CONFIG_FILE_FPATH                             CONFIG_FILES_DIR "/resources.conf"
#define RESOURCES_DIR					                        "/var/WebServerd"
#define TEMPORARY_FILES_DIR                                     RESOURCES_DIR "/.tmpfiles"
#define DEFAULTS_DIR                                            "defaults/"

#define BAD_REQUEST_WEB_PAGE     				                DEFAULTS_DIR "bad_request.html"
#define FORBIDDEN_WEB_PAGE                                      DEFAULTS_DIR "forbidden.html"
#define NOT_FOUND_WEB_PAGE				                        DEFAULTS_DIR "not_found.html"
#define METHOD_NOT_ALLOWED_WEB_PAGE                             DEFAULTS_DIR "method_not_allowed.html"
#define NOT_ACCEPTABLE_WEB_PAGE                                 DEFAULTS_DIR "not_acceptable.html"
#define LENGTH_REQUIRED_WEB_PAGE                                DEFAULTS_DIR "length_required.html"
#define CONTENT_TOO_LARGE_WEB_PAGE                              DEFAULTS_DIR "content_too_large.html"
#define INTERNAL_SERVER_ERROR_WEB_PAGE	                        DEFAULTS_DIR "internal_server_error.html"
#define HTTP_VERSION_NOT_SUPPORTED_ERROR_WEB_PAGE               DEFAULTS_DIR "http_version_not_supported.html"
#define SERVICE_UNAVAILABLE_WEB_PAGE                            DEFAULTS_DIR "service_unavailable.html"
#define UNSUPPORTED_MEDIA_TYPE_WEB_PAGE                         DEFAULTS_DIR "unsupported_media_type.html"


#endif
