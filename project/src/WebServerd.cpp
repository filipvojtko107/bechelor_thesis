#include "WebServer.hpp"
#include "Logger.hpp"
#include <stdio.h>
#include <signal.h>
#include <atomic>
#include <systemd/sd-daemon.h>

volatile std::atomic<bool> daemon_run;
volatile std::atomic<bool> daemon_reload;

void sigTermHandler(int signum)
{ 
	daemon_run = false;
	daemon_reload = false;
	WebServer::stop();
}

void sigHupHandler(int signum)
{
	daemon_run = true;
	daemon_reload = true;
	WebServer::stop();
}

void serviceInit()
{
	daemon_run = true;
	daemon_reload = false;

	// Nastavit stdout bez bufferu, aby se informace hned logovaly
	setvbuf(stdout, NULL, _IONBF, 0);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, sigTermHandler);
	signal(SIGHUP, sigHupHandler);
}

bool serviceStart()
{
	sd_notify(0, "STATUS=Starting...");
	LOG_INFO("Starting...");

	if (!WebServer::start()) {
		return false;
	}

	if (sd_notify(0, "READY=1") < 0) 
	{
		sd_notify(0, "STATUS=Starting failed");
		LOG_ERR("Start failed");
		WebServer::stop();
		return false;
	}

	sd_notify(0, "STATUS=Started");
	LOG_INFO("Started");
	return true;
}

void serviceReload()
{
	try
	{
		sd_notify(0, "STATUS=Reloading...");
		LOG_INFO("Reloading...");

		if (sd_notify(0, "RELOADING=1") < 0) {
			goto err;
		}

		WebServer::reset();
		if (!WebServer::start()) {
			goto err;
		}

		if (sd_notify(0, "READY=1") < 0) {
			goto err;
		}

		daemon_run = true;
		daemon_reload = false;

		sd_notify(0, "STATUS=Reloaded");
		LOG_INFO("Reloaded...");
	}
	catch (const std::exception& e) {
		goto err;
	}

	return;

err:
	daemon_run = false;
	sd_notify(0, "STATUS=Reloading failed");
	LOG_ERR("Reloading failed");
	WebServer::stop();
}

void serviceStop()
{
	sd_notify(0, "STATUS=Stopping...");
	LOG_INFO("Stopping...");

	if (sd_notify(0, "STOPPING=1") < 0)
	{
		sd_notify(0, "STATUS=Stopping failed");
		LOG_ERR("Failed to shut down");
		return;
	}

	WebServer::stop();
	sd_notify(0, "STATUS=Stopped");
	LOG_INFO("Stopped");
}

int main(int argc, char* argv[])
{
	try
	{
		serviceInit();
		if (!serviceStart()) {
			return EXIT_FAILURE;
		}

		// Main loop
		while (daemon_run)
		{
			WebServer::run();
			if (daemon_reload) {
				serviceReload();
			}
		}

		serviceStop();
	}
	
	catch (const std::exception& e)
	{
		LOG_ERR("Error: %s", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
