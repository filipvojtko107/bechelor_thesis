#include "SslConfig.hpp"
#include "Logger.hpp"
#include "Configuration.hpp"


SslConfig::~SslConfig()
{
    this->reset();
}

void SslConfig::reset()
{
    if (ctx_) 
    {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

bool SslConfig::set()
{
    if (Config::params().https_enabled) 
	{
        bool cert_rsa_e = Config::params().ssl_certificate_rsa.empty();
        bool key_rsa_e = Config::params().private_key_rsa.empty();
        bool cert_ecdsa_e = Config::params().ssl_certificate_ecdsa.empty();
        bool key_ecdsa_e = Config::params().private_key_ecdsa.empty();
        if ((!cert_rsa_e && key_rsa_e) || (cert_rsa_e && !key_rsa_e)) 
        {
            LOG_ERR("RSA certificate and private key setting failed");
            return false;
        }
        if ((!cert_ecdsa_e && key_ecdsa_e) || (cert_ecdsa_e && !key_ecdsa_e)) 
        {
            LOG_ERR("ECDSA certificate and private key setting failed");
            return false;
        }

        initOpenSSL();
        if (!createContext()) {
            return false;
        }
        if (!configureContext()) {
            return false;
        }
    }

    return true;
}

void SslConfig::initOpenSSL()
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

bool SslConfig::createContext()
{
    const SSL_METHOD* method = TLS_server_method();
    ctx_ = SSL_CTX_new(method);
    if(!ctx_) 
    {
        LOG_ERR("SSL setup failed");
        return false;
    }

    return true;
}

bool SslConfig::configureContext()
{
    // Cipher suites
    std::string cipher_suites;
    for (const std::string& cs : Config::params().cipher_suites) {
        cipher_suites += cs;
        cipher_suites += ":";
    }
    cipher_suites.erase(cipher_suites.size()-1);

    // RSA
    if (SSL_CTX_use_certificate_chain_file(ctx_, Config::params().ssl_certificate_rsa.c_str()) <= 0) {
        LOG_ERR("Failed to load RSA certificate");
        return false;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx_, Config::params().private_key_rsa.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG_ERR("Failed to load RSA private key");
        return false;
    }

    // ECDSA
    if (SSL_CTX_use_certificate_chain_file(ctx_, Config::params().ssl_certificate_ecdsa.c_str()) <= 0) {
        LOG_ERR("Failed to load ECDSA certificate");
        return false;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx_, Config::params().private_key_ecdsa.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG_ERR("Failed to load ECDSA private key");
        return false;
    }

    SSL_CTX_set_ciphersuites(ctx_, cipher_suites.c_str());
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);

    return true;
}
