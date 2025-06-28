#ifndef __TCP_SERVER_HPP__
#define __TCP_SERVER_HPP__
#include "ThreadPool.hpp"
#include "HttpGlobal.hpp"
#include "SslConfig.hpp"
#include <vector>
#include <deque>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include <memory>


class TcpServer
{
	using Task = std::function<void()>;

	public:
		struct Connection
		{
			friend class TcpServer;
		
			public:
				Connection() = default;
				Connection(const Connection& obj) = default;
				Connection(Connection&& obj) = default;
				Connection& operator=(const Connection& obj) = default;
				Connection& operator=(Connection&& obj) = default;
				~Connection();

				int getSocket() const { return socket_; }
				bool hasSocket() const { return (socket_ != -1); }
				bool hasSsl() const { return (ssl_ != nullptr); }
				void setSsl(SSL* ssl) { ssl_ = ssl; }

			private:
				int socket_ = -1;
				SSL* ssl_ = nullptr;
		};
			
		TcpServer();
		TcpServer(const TcpServer& obj) = delete;
		TcpServer(TcpServer&& obj) = delete;
		~TcpServer() = default;
		
		TcpServer& operator=(const TcpServer& obj) = delete;
		TcpServer& operator=(TcpServer&& obj) = delete;
		
		bool start();
		bool stop();
		bool reset();
		bool set();
		std::shared_ptr<TcpServer::Connection> acceptConnection();
		std::shared_ptr<TcpServer::Connection> acceptConnectionSsl();
		bool handleConnection(std::shared_ptr<TcpServer::Connection>& connection, const Task& task);
		bool endConnection(std::shared_ptr<TcpServer::Connection>& connection);
		int sendText(const std::shared_ptr<TcpServer::Connection>& connection, const char* data, const size_t size);
		int sendText(const std::shared_ptr<TcpServer::Connection>& connection, const std::string& data);
		int receiveText(const std::shared_ptr<TcpServer::Connection>& connection, std::string& data, 
						const uint64_t max_size_to_recv, const std::string& terminator, const bool peek_data);
		int receiveText(const std::shared_ptr<TcpServer::Connection>& connection, std::string& data, 
						const uint64_t bytes_to_recv, const bool peek_data);
		bool isConnected(const std::shared_ptr<TcpServer::Connection>& connection) const;
		bool isRunning() const { return run_; }
		bool isDeactivated() const { return deactivated_; }
		const std::vector<std::shared_ptr<TcpServer::Connection>>& connections() const { return connections_; }
		uint32_t maxConnections() const { return max_connections_; }

	private:
		bool deactivate();
		int waitForData(const std::shared_ptr<TcpServer::Connection>& connection);
		int sendAll(const std::shared_ptr<TcpServer::Connection>& connection, const char* data, const size_t size);
		
	private:
		volatile std::atomic<bool> run_;
		std::atomic<bool> deactivated_;
		mutable std::mutex mutex_;
		int socket_;
		int socket_ssl_;
		sockaddr_in server_;
		sockaddr_in server_ssl_;
		uint32_t max_connections_;
		std::vector<std::shared_ptr<TcpServer::Connection>> connections_;
		ThreadPool thread_pool_;
};


#endif
