#include "WebServer.hpp"
#include "Logger.hpp"
#include "Globals.hpp"
#include "HttpGlobal.hpp"
#include "Configuration.hpp"
#include "Http.hpp"
#include "SslConfig.hpp"
#include "Http1_0.hpp"
#include "Http1_1.hpp"
//#include "Http2_0.hpp"
#include "HttpPacketBuilder.hpp"
#include "request.h"
#include "openssl/err.h"
#include <string>
#include <unistd.h>


WebServer WebServer::server_;

void sendToClient(std::shared_ptr<TcpServer::Connection>& connection, HttpPacketBuilder& hpb, const std::shared_ptr<TcpServer>& tcp_server)
{
	Http1_0 http_client = Http1_0(tcp_server, connection);
	if (!http_client.sendResponse(hpb.packet())) {
		//LOG_DBG("sendToClient() failed");
	}
	http_client.endConnection();
}


WebServer::WebServer()
{
	tcp_server_ = std::make_shared<TcpServer>();
}

bool WebServer::start()
{		
	if (!server_.loadConfigFiles()) {
		return false;
	}

	if (!server_.tcp_server_->start()) {
		return false;
	}
		
	LOG_INFO("Web server started");	
	return true;
}


bool WebServer::stop()
{
	if (server_.isRunning())
	{
		server_.tcp_server_->stop();
		if (Config::params().https_enabled &&
			server_.https_thread_.joinable()) 
		{
			server_.https_thread_.join();
		}

		return true;
	}
	
	return false;
}


bool WebServer::reset()
{
	if (!server_.isRunning())
	{
		Config::reset();
		server_.ssl_config_.reset();
		server_.tcp_server_->reset();
		return true;
	}

	return false;
}


bool WebServer::getClientHttpVersion(std::shared_ptr<TcpServer::Connection>& connection, std::unique_ptr<Http>& http_client)
{
	if (isRunning())
	{			
		std::string request_data;
		HttpPacketBuilder hpb(HttpVersion::HTTP_1_0);

		//LOG_DBG("getClientHttpVersion()...");
		//LOG_DBG("getClientHttpVersion()::receiveText()...");

		int ret = tcp_server_->receiveText(connection, request_data, Config::params().max_header_size, HEADERS_END, true);
		// Klient se odpojil
		if (ret == 0) 
		{
			tcp_server_->endConnection(connection);
			return false;
		}
		// Chyba
		else if (ret == -1)
		{
			hpb.buildInternalServerError();
			sendToClient(connection, hpb, tcp_server_);
			return false;
		}

		//LOG_DBG("getClientHttpVersion()::receiveText()");
		//LOG_DBG("\nRecvd data: %s\n", request_data.c_str());

		httpparser::Request request;
		if (!httpParseRequest(request_data, request))
		{
			hpb.buildBadRequest();
			sendToClient(connection, hpb, tcp_server_);
			return false;
		}
		
		const HttpVersion http_version = 
			httpVersionStr(std::to_string(request.versionMajor) + "." + std::to_string(request.versionMinor));
		switch (http_version)
		{
			case HttpVersion::HTTP_1_0:
				//LOG_DBG("Creating Http1_0 object");
				http_client = std::unique_ptr<Http1_0>(new Http1_0(tcp_server_, connection));
				break;
			case HttpVersion::HTTP_1_1:
			//LOG_DBG("Creating Http1_1 object");
			http_client = std::unique_ptr<Http1_1>(new Http1_1(tcp_server_, connection));
				break;
			/*case HttpVersion:HTTP_2_0:
				//LOG_DBG("Creating Http2_0 object");
				http_client = std::unique_ptr<Http2_0>(new Http2_0(tcp_server_, connection));
				break;*/
			case HttpVersion::UNSUPPORTED:
				hpb.buildHttpVersionNotSupported();
				sendToClient(connection, hpb, tcp_server_);
				return false;
		}

		return true;
	}
	
	return false;
}

void WebServer::run()
{
	try
	{
		if (Config::params().https_enabled)
		{
			server_.https_thread_ = std::thread([]()
			{
				try
				{
					WebServer& server = WebServer::server_;
					while (server.isRunning() && !server.isDeactivated())
					{
						std::shared_ptr<TcpServer::Connection> connection = 
							server.tcp_server_->acceptConnectionSsl();
						if (!connection->hasSocket()) {
							continue;
						}
		
						//LOG_DBG("Client connected");
						server.createSessionSsl(connection);
					}
				}

				catch (const std::exception& e) {
					LOG_ERR("Error: %s", e.what());
				}
			});
		}

		try
		{
			while (server_.isRunning() && !server_.isDeactivated())
			{
				std::shared_ptr<TcpServer::Connection> connection = 
					server_.tcp_server_->acceptConnection();
				if (!connection->hasSocket()) {
					continue;
				}

				//LOG_DBG("Client connected");
				server_.createSession(connection);
			}
		}
		catch (const std::exception& e) {
			//LOG_DBG("Error: %s", e.what());
		}
	}

	catch (const std::exception& e) {
		LOG_ERR("Error: %s", e.what());
	}
}

void WebServer::createSession(std::shared_ptr<TcpServer::Connection> connection)
{
	//LOG_DBG("\nCreating NORMAL session...\n");

	const bool result = 
		tcp_server_->handleConnection(connection, [this, connection]() mutable
	{
		try
		{
			if (tcp_server_->isConnected(connection))
			{
				std::unique_ptr<Http> http_client;
				if (!getClientHttpVersion(connection, http_client)) {
					return;
				}

				http_client->handleConnection();
			}

			else {
				tcp_server_->endConnection(connection);
			}
		}
		
		catch (const std::exception& e)
		{
			//LOG_DBG("Error: %s", e.what());
			tcp_server_->endConnection(connection);
		}

		usleep(150);
	});
	
	if (!result)
	{
		HttpPacketBuilder hpb(HttpVersion::HTTP_1_0);
		hpb.buildServiceUnavailable();
		sendToClient(connection, hpb, tcp_server_);
	}
}

void WebServer::createSessionSsl(std::shared_ptr<TcpServer::Connection> connection)
{
	//LOG_DBG("\nCreating SSL session...\n");

	const bool result = 
		tcp_server_->handleConnection(connection, [this, connection]() mutable
	{
		try
		{
			if (tcp_server_->isConnected(connection))
			{
				std::unique_ptr<Http> http_client;
				SSL* ssl = SSL_new(ssl_config_.ctx());
				SSL_set_fd(ssl, connection->getSocket());
	
				// TLS handshake
				if (SSL_accept(ssl) != 1) 
				{
					//LOG_DBG("SSL handshake failed");
					#ifdef DBG
					ERR_print_errors_fp(stderr);
					#endif
					if (ssl) { SSL_free(ssl); }
					tcp_server_->endConnection(connection);
					return;
				}
				connection->setSsl(ssl);

				if (!getClientHttpVersion(connection, http_client)) {
					return;
				}

				http_client->handleConnection();
			}

			else {
				tcp_server_->endConnection(connection);
			}
		}
		
		catch (const std::exception& e)
		{
			//LOG_DBG("Error: %s", e.what());
			tcp_server_->endConnection(connection);
		}
	});
	
	if (!result)
	{
		HttpPacketBuilder hpb(HttpVersion::HTTP_1_0);
		hpb.buildServiceUnavailable();
		sendToClient(connection, hpb, tcp_server_);
	}
}


bool WebServer::loadConfigFiles()
{
	//LOG_DBG("Loading config...");
	if (!Config::loadConfig()) {
		return false;
	}

	//LOG_DBG("Loading resources config...");
	if (!Config::loadResourcesConfig()) {
		return false;
	}

	//LOG_DBG("Setting SSL...");
	if (!ssl_config_.set()) {
		return false;
	}
	//LOG_DBG("SSL done");

	return tcp_server_->set();
}
