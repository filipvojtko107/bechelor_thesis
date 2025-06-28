#ifndef __SSL_CONFIG_HPP__
#define __SSL_CONFIG_HPP__
#include <memory>
#include "openssl/ssl.h"


class SslConfig
{
    public:
        struct Certificates 
        {
            X509* rsa_cert = nullptr;
            EVP_PKEY* rsa_key = nullptr;
            X509* ecdsa_cert = nullptr;
            EVP_PKEY* ecdsa_key = nullptr;
        };

        SslConfig() = default;
        SslConfig(const SslConfig& obj) = delete;
        SslConfig(SslConfig&& obj) = delete;
        SslConfig& operator=(const SslConfig& obj) = delete;
        SslConfig& operator=(SslConfig&& obj) = delete;
        ~SslConfig();
        
        bool set();
        void reset();
        SSL_CTX* ctx() { return ctx_; }

    private:
        void initOpenSSL();
        bool createContext();
        bool configureContext();

    private:
        SSL_CTX* ctx_ = nullptr;
        Certificates certs_;
};

#endif