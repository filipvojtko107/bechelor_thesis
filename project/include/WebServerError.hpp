#ifndef __WEB_SERVER_ERROR__
#define __WEB_SERVER_ERROR__
#include <exception>
#include <string>


class WebServerError : public std::exception
{
    public:
        WebServerError(const char* emess);
        WebServerError(const std::string& emess);
        WebServerError(std::string&& emess);
        WebServerError& operator=(const WebServerError& obj);
        const char* what() const noexcept override;

    private:
        std::string emess_;
};

#endif