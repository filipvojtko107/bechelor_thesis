#include "TcpServer.hpp"
#include "Logger.hpp"
#include "Configuration.hpp"
#include "openssl/ssl.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>

#define RTT_2 (50000)	// RTT / 2 = 50ms; RTT = 100ms


TcpServer::Connection::~Connection()
{
	if (ssl_) {
		SSL_free(ssl_);
	}
}

TcpServer::TcpServer() :
	run_(false),
	deactivated_(false),
	socket_(-1),
	socket_ssl_(-1),
	server_({0}),
	server_ssl_({0}),
	max_connections_(0)
{

}

bool initSocket(int& sock, sockaddr_in& server)
{
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		LOG_ERR("Failed to create server socket");
		return false;
	}
	
	if (bind(sock, (sockaddr*) &server, sizeof(server)) == -1)
	{
		LOG_ERR("Failed to bind server socket");
		return false;
	}
	
	if (listen(sock, SOMAXCONN) == -1)
	{
		LOG_ERR("Failed to listen on server socket");
		return false;
	}

	return true;
}

bool TcpServer::start()
{
	if (!run_)
	{
		//LOG_DBG("Spawning threads...");
		if (!thread_pool_.start()) 
		{
			LOG_ERR("Failed to spawn threads");
			return false;
		}

		// Vytvoreni socketu pro pripojovani
		if (!initSocket(socket_, server_)) {
			return false;
		}
		if (Config::params().https_enabled) 
		{
			if (!initSocket(socket_ssl_, server_ssl_)) 
			{
				return false;
			}
		}

		run_ = true;
		deactivated_ = false;
		//LOG_DBG("Threads spawned");
		
		return true;
	}
	
	return false;
}


bool TcpServer::deactivate()
{
	try
	{
		if (run_ && !deactivated_)
		{
			//LOG_DBG("Shutting down server socket...");
			if (shutdown(socket_, SHUT_RD) == -1) 
			{
				LOG_ERR("Failed to shut down server socket");
				return false;
			}
			if (Config::params().https_enabled)
			{
				if (shutdown(socket_ssl_, SHUT_RD) == -1) 
				{
					LOG_ERR("Failed to shut down server SSL socket");
					return false;
				}
			}

			//LOG_DBG("Closing server socket...");
			if (close(socket_) == -1) 
			{
				LOG_ERR("Failed to close server socket");
				return false;
			}
			if (Config::params().https_enabled)
			{
				if (close(socket_ssl_) == -1) 
				{
					LOG_ERR("Failed to close server SSL socket");
					return false;
				}
			}

			deactivated_ = true;
			//LOG_DBG("TCP server deactivated");

			return true;
		}
	}

	catch (const std::exception& e) {
		LOG_ERR("Failed to deactivate TCP server (error: %s)", e.what());
	}

	return false;
}


bool TcpServer::stop()
{
	try
	{
		bool ret = true;
		if (run_)
		{
			//LOG_DBG("Stopping TCP server...");

			if (!deactivated_) {
				ret = deactivate();
			}

			run_ = false;

			for (std::shared_ptr<TcpServer::Connection>& conn : connections_)
			{
				if (shutdown(conn->socket_, SHUT_RDWR) == -1) 
				{
					//LOG_DBG("Failed to shut down client socket");
					ret = false;
				}
	
				if (close(conn->socket_) == -1)
				{
					//LOG_DBG("Failed to close client socket");
					ret = false;
				}
			}
			
			//LOG_DBG("Stopping threads...");
			if (!thread_pool_.stop(true))
			{
				LOG_ERR("Failed to stop TCP server (failed to stop threads)");
				ret = false;
			}
			
			//LOG_DBG("TCP server stopped");
			return ret;
		}
	}

	catch (const std::exception& e) {
		LOG_ERR("Failed to stop TCP server (error: %s)", e.what());
	}
	
	return false;
}


bool TcpServer::reset()
{
	if (!run_)
	{
		deactivated_ = false;
		socket_ = -1;
		socket_ssl_ = -1;
		memset(&server_, 0, sizeof(server_));
		memset(&server_ssl_, 0, sizeof(server_ssl_));
		max_connections_ = 0;
		connections_.clear();

		if (!thread_pool_.reset()) {
			return false;
		}

		return true;
	}
	
	return false;
}


bool TcpServer::set()
{
	if (!run_)
	{
		const Config::Params& params = Config::params();

		max_connections_ = params.client_threads;
		thread_pool_.resize(params.client_threads);
		server_.sin_family = AF_INET;
		server_.sin_port = htons(params.port);
		server_ssl_.sin_family = AF_INET;
		server_ssl_.sin_port = htons(params.port_https);
		inet_aton(params.ip_address.c_str(), &server_.sin_addr);
		inet_aton(params.ip_address.c_str(), &server_ssl_.sin_addr);

		return true;
	}
	
	return false;
}


std::shared_ptr<TcpServer::Connection> TcpServer::acceptConnection()
{
	std::shared_ptr<TcpServer::Connection> connection(new TcpServer::Connection());
	if (run_ && !deactivated_)
	{
		connection->socket_ = accept(socket_, nullptr, 0);
		if (connection->socket_ == -1) {
			//LOG_DBG("Failed to accept client connection (error: %s)", strerror(errno));
		}
	}

	return connection;
}


std::shared_ptr<TcpServer::Connection> TcpServer::acceptConnectionSsl()
{
	std::shared_ptr<TcpServer::Connection> connection(new TcpServer::Connection());
	if (run_ && !deactivated_)
	{
		connection->socket_ = accept(socket_ssl_, nullptr, 0);
		if (connection->socket_ == -1) {
			//LOG_DBG("Failed to accept client connection (error: %s)", strerror(errno));
		}
	}

	return connection;
}

bool TcpServer::handleConnection(std::shared_ptr<TcpServer::Connection>& connection, const Task& task)
{
	try
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (run_)
		{
			// Kontrola zda jiz nepresahuji max. pocet povolenych pripojeni
			if (connections_.size() == max_connections_) {
				return false;
			}

			//LOG_DBG("concection_count checked\n");

			connections_.push_back(connection);
			thread_pool_.queueTask(task);

			//LOG_DBG("Handling client connection");
			return true;
		}
	}

	catch (const std::exception& e) {
		//LOG_DBG("Failed to handle client connection (error: %s)", e.what());
	}
	
	return false;
}


bool TcpServer::endConnection(std::shared_ptr<TcpServer::Connection>& connection)
{
	try
	{
		//LOG_DBG("TcpServer::endConnection()...");
		std::lock_guard<std::mutex> lock(mutex_);
		if (run_)
		{	
			//LOG_DBG("TcpServer::endConnection() jede");
			if (connection->socket_ == -1 || connections_.empty()) {
				return false;
			}
			
			//LOG_DBG("Closing socket...");
			
			// Zavreni socketu
			bool ret = true;
			if (shutdown(connection->socket_, SHUT_RDWR) == -1) 
			{
				//LOG_DBG("Failed to shut down client socket (error: %s)", strerror(errno));
				ret = false;
			}
			if (close(connection->socket_) == -1)
			{
				//LOG_DBG("Failed to close client socket");
				ret = false;
			}
			
			//LOG_DBG("TcpServer::endConnection() mazani");

			// Odstraneni z ulozenych pripojeni
			auto connection_it = std::find_if(connections_.begin(), connections_.end(),
			[&connection](const std::shared_ptr<TcpServer::Connection>& conn)
			{
				return (connection->socket_ == conn->socket_);
			});
			
			if (connection_it != connections_.end()) {
				connections_.erase(connection_it);
			}
			
			connection->socket_ = -1;
				
			//LOG_DBG("Client disconnected");
			return ret;
		}
	}

	catch (const std::exception& e) {
		//LOG_DBG("Failed to end client connection (error: %s)", e.what());
	}
	
	return false;
}


int TcpServer::sendAll(const std::shared_ptr<TcpServer::Connection>& connection, const char* data, const size_t size)
{
    size_t total = 0;
    size_t bytesleft = size;
    int n;
	int ret;

    while (total < size)
	{
		errno = 0;
		if (connection->ssl_)	
		{
			size_t nn = 0;
			ret = SSL_write_ex(connection->ssl_, data + total, bytesleft, &nn);
			n = static_cast<int>(nn);
		}
		else 
		{
        	ret = send(connection->socket_, data + total, bytesleft, MSG_NOSIGNAL);
			n = ret;
		}

        total += n;
        bytesleft -= n;
    }

	return 1;
}

int TcpServer::sendText(const std::shared_ptr<TcpServer::Connection>& connection, const std::string& data)
{
	return sendText(connection, data.c_str(), data.size());
}

int TcpServer::sendText(const std::shared_ptr<TcpServer::Connection>& connection, const char* data, const size_t size)
{
	if (run_ && data != nullptr && size > 0)
	{
		int ret = sendAll(connection, data, size);
		if (ret == -1) {
			//LOG_DBG("Failed to send data");
		}
		
		return ret;
	}
	
	return -1;
}


int TcpServer::waitForData(const std::shared_ptr<TcpServer::Connection>& connection) {
    //LOG_DBG("TcpServer::waitForData()...");

	int ret;
    if (connection->ssl_) 
	{
        ret = SSL_pending(connection->ssl_);
        if (ret > 0) {
            //LOG_DBG("SSL data pending: %d bytes", ret);
            return 1;
        }
	}

	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(connection->socket_, &read_fds);
	struct timeval timeout = {0, 0};
	ret = select(connection->socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
	if (ret == -1) {
		//LOG_DBG("select error: %s", strerror(errno));
		return -1;
	} 
	else if (ret == 0) {
		//LOG_DBG("No data on socket");
		return 0;
	}

	uint8_t buffer;
	if (connection->ssl_) {
		ret = SSL_peek(connection->ssl_, &buffer, sizeof(buffer));
	}
	else {
		ret = recv(connection->socket_, &buffer, sizeof(buffer), MSG_PEEK);
	}

	if (ret > 0) {
		return 1;
	} 
	else {
		//LOG_DBG("SSL_peek error: %d", SSL_get_error(connection->ssl_, ret));
		return -1;
	}
}

int TcpServer::receiveText(const std::shared_ptr<TcpServer::Connection>& connection, std::string& data, const uint64_t max_size_to_recv, const std::string& terminator, const bool peek_data)
{
	//LOG_DBG("receiveText(term) called...");

	uint64_t total = 0;
	int data_available;
	int n;
	int ret;

	// Pockat az budou nejaka data dostupna
	ret = waitForData(connection);
	if (ret != 1) {
		//LOG_DBG("Returning...");
		return ret; }

	//LOG_DBG("Wait for data done");

	// Nacteni samotnych dat
	data.reserve(max_size_to_recv);

	while (isConnected(connection))
	{
		bool ssl_read_attempted = false;

        if (connection->ssl_) 
		{
            // Attempt to read at least 1 byte to trigger SSL processing
			size_t nn = 0;
            uint8_t buffer;
            ret = SSL_peek_ex(connection->ssl_, &buffer, sizeof(buffer), &nn);
            if (ret == 1) 
			{
                data_available = SSL_pending(connection->ssl_) + nn;
                ssl_read_attempted = true;
            } 
			else 
			{
                int ssl_err = SSL_get_error(connection->ssl_, ret);
                if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) 
				{
                    usleep(RTT_2);
                    continue;
                }
				else {
                    return -1;
                }
            }
        } 

		else 
		{
            if (ioctl(connection->socket_, FIONREAD, &data_available) == -1) 
			{
                //LOG_DBG("Failed to get socket buffer data amount");
                return -1;
            }
        }

        if (!ssl_read_attempted && data_available == 0) {
            usleep(RTT_2);
            continue;
        }
		
		// Nactu vsechna data ktera jsou aktualne dostupna v socketu
		data.resize(data_available);
		while (total != data.size())
		{
			errno = 0;

			if (connection->ssl_)
			{
				size_t nn = 0;
				ret = SSL_peek_ex(connection->ssl_, const_cast<char*>(data.data()) + total, data.size() - total, &nn);
				n = static_cast<int>(nn);
			}
			else 
			{
				ret = recv(connection->socket_, const_cast<char*>(data.data()) + total, data.size() - total, MSG_PEEK);
				n = ret;
			}

			// Kontrola chyby
			if (ret == -1){
				return -1;
			}
			// Kontrola zda nebylo preruseno spojeni
			else if (ret == 0) {
				return 0;
			}

			total += n;
		}

		const size_t end_header_index = data.rfind(terminator);
		if (end_header_index == std::string::npos)  // Zatim terminator nenalezen
		{
			if (total >= data.capacity()) { return -2; }
			else { continue; }
		}
		else 
		{
			// Odstranit nacetla data, ktera se nacetla navic
			if (data.size() > (end_header_index + terminator.size())) {
				data.erase(end_header_index + terminator.size());
			}

			// Terminator nalezen, ale prekrocil maximalni limit
			if (total > data.capacity()) { return -2; }
			else { break; }
		}
	}

	//LOG_DBG("\nTcpServer::receiveText(term) -> %s\n", data.c_str());

	// Vyndat data ze socketu
	if (!peek_data)
	{
		total = 0;
		while (total != data.size())
		{
			errno = 0;

			if (connection->ssl_)
			{
				size_t nn = 0;
				ret = SSL_read_ex(connection->ssl_, const_cast<char*>(data.data()) + total, data.size() - total, &nn);
				n = static_cast<int>(nn);
			}
			else
			{
				ret = recv(connection->socket_, const_cast<char*>(data.data()) + total, data.size() - total, MSG_WAITALL);
				n = ret;
			}

			// Kontrola chyby
			if (ret == -1){
				return -1; 
			}
			// Kontrola zda nebylo preruseno spojeni
			else if (ret == 0) {
				return 0;
			}

			total += n;
		}
	}

	return 1;
}

int TcpServer::receiveText(const std::shared_ptr<TcpServer::Connection>& connection, std::string& data, const uint64_t bytes_to_recv, const bool peek_data)
{
	//LOG_DBG("receiveText(size) called...");

	const int recv_flag = ((peek_data) ? MSG_PEEK : MSG_WAITALL);
	uint64_t total = 0;
	int n;
	int ret;

	// Pockat az budou nejaka data dostupna
	ret = waitForData(connection);
	if (ret != 1) { return ret; }

	//LOG_DBG("Wait for data done");

	// Nacteni samotnych dat
	while (total != bytes_to_recv)
	{
		//LOG_DBG("Data receiving...");

		errno = 0;

		if (connection->ssl_)
		{
			size_t nn = 0;
			if (peek_data) {
				ret = SSL_peek_ex(connection->ssl_, const_cast<char*>(data.data()) + total, bytes_to_recv - total, &nn);
			}
			else {
				ret = SSL_read_ex(connection->ssl_, const_cast<char*>(data.data()) + total, bytes_to_recv - total, &nn);
			}
			n = static_cast<int>(nn);
		}
		else
		{
			ret = recv(connection->socket_, const_cast<char*>(data.data()) + total, bytes_to_recv - total, recv_flag);
			n = ret;
		}

		// Kontrola chyby
		if (ret == -1) {
			return -1;
		}
		// Kontrola zda nebylo preruseno spojeni
		else if (ret == 0) {
			return 0;
		}

		total += n;
	}

	return 1;
}


bool TcpServer::isConnected(const std::shared_ptr<TcpServer::Connection>& connection) const
{
    if (run_)
    {
		errno = 0;
		uint8_t buffer;
		const int ret = recv(connection->socket_, &buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);

		// Vraci 0 kdyz peer ukoncil spojeni, ale jen pokud uz v kernel bufferu socketu nejsou zadna data k precteni, tedy az po precteni vsech dat.
		if (ret == 0) { 
			return false; 
		}
		else if (ret == -1)
		{ 
			if (errno == EAGAIN || errno == EWOULDBLOCK) { 
				return true; 
			}
			else { 
				return false;
			}
		}
		else { 
			return true;
		}
	}

    return false;
}

