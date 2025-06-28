#include "WebServerError.hpp"

WebServerError::WebServerError(const char* emess)
    : emess_(emess)
{

}

WebServerError::WebServerError(const std::string& emess)
    : emess_(emess)
{

}

WebServerError::WebServerError(std::string&& emess)
    : emess_(std::move(emess))
{

}

WebServerError& WebServerError::operator=(const WebServerError& obj)
{
    if (this != &obj) {
        emess_ = obj.emess_;
    }
    return *this;
}

const char* WebServerError::what() const noexcept
{
    return emess_.c_str();
}