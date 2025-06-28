#ifndef __CONFIGURATION_HPP__
#define __CONFIGURATION_HPP__
#include "Globals.hpp"
#include "HttpGlobal2.hpp"
#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <pthread.h>
#include <atomic>


class Config
{
	public:
		// WebServerd.conf params
		struct Params
		{
			void reset();

			std::string web_server_name;
			std::string ip_address;
			uint16_t port = 0;
			uint16_t port_https = 0;
			bool https_enabled;
			std::string ssl_certificate_rsa;
			std::string private_key_rsa;
			std::string ssl_certificate_ecdsa;
			std::string private_key_ecdsa;
			std::vector<std::string> cipher_suites;

			uint16_t client_threads = 0;
			uint32_t file_chunk_size = 0;
			uint16_t max_header_size = 0;
			uint16_t client_body_buffer_size = 0;
			uint64_t client_max_body_size = 0;
			bool prefer_content_encoding = false;
		};

		// resources.conf params
		struct RParams
		{
			RParams();
			RParams(const RParams& obj) = delete;
			RParams(RParams&& obj);
			~RParams();
			RParams& operator=(const RParams& obj) = delete;
			RParams& operator=(RParams&& obj);

			void lock(const bool resource_lock_shared);
			void unlock();
			void releaseFileLock();
			void update();
			void updateLastAccess();
			bool isSet() const { return (last_modified != -1 && !etag.empty() && resource_fd != -1); }
			static std::string generateETag(const int64_t sec, const int64_t nsec);

			std::string resource_path;  // Uklada se vzdy bez "/" jako prvni znak
			std::vector<std::string> methods_allowed;
			std::string accept_ranges;
			int32_t expires;
			std::string cache_type;
		
			uint64_t resource_size;
			time_t last_modified;
			ETag etag;

			pthread_rwlock_t access_lock;
			std::atomic<uint32_t> access_counter;
			std::mutex update_lock;
			int resource_fd;

			// Posledni cas pristupu k resource
			time_t last_resource_access;
		};

		~Config() = default;
		static const Config::Params& params() { return obj_.params_; }
		static Config::RParams& rparams(const std::string& resource, const bool resource_lock_shared);
		static Config::RParams* orparams(const std::string& resource, const bool resource_lock_shared);
		static void orparamsRemove(const Config::RParams* rparam);
		static bool loadConfig();
		static bool loadResourcesConfig();
		static void reset();

	private:
		Config() = default;
		bool buildParams();
		bool buildRParams();

	private:
		static Config obj_;
		Config::Params params_;
		std::unordered_map<std::string, Config::RParams> rparams_;  // rparams_.first -> resource name v konfiguraku 
		std::unordered_map<std::string, Config::RParams> other_rparams_;  // resource params pro resources, ktere nejsou devinovany v resources.conf
		std::mutex orparams_mutex_;
};


#endif
