#ifndef __WEB_SERVER_HPP__
#define __WEB_SERVER_HPP__
#include "TcpServer.hpp"
#include "Http.hpp"
#include "SslConfig.hpp"
#include <memory>
#include <thread>


class WebServer
{
	public:
		~WebServer() = default;		
		static bool start();
		static void run();
		static bool stop();
		static bool reset();
		static bool isRunning() { return server_.tcp_server_->isRunning(); }
		static bool isDeactivated() { return server_.tcp_server_->isDeactivated(); }	
	
	private:
		WebServer();
		WebServer(const WebServer& obj) = delete;
		WebServer(WebServer&& obj) = delete;
		WebServer& operator=(const WebServer& obj) = delete;
		WebServer& operator=(WebServer&& obj) = delete;
		
		void createSession(std::shared_ptr<TcpServer::Connection> connection);
		void createSessionSsl(std::shared_ptr<TcpServer::Connection> connection);
		bool loadConfigFiles();
		bool getClientHttpVersion(std::shared_ptr<TcpServer::Connection>& connection, std::unique_ptr<Http>& http_client);
			
	private:
		std::shared_ptr<TcpServer> tcp_server_;
		SslConfig ssl_config_;
		std::thread https_thread_;
		static WebServer server_;
};


#endif
