#pragma once

#include <windows.h>
#include <thread>
#include <mutex>
#include <vector>
#include <list>
#include <functional>

#include "NamedPipesServerClientHandler.hpp"

// Multi-threaded named pipes server.
//
// Accepts connections on a specified named pipe path.
// Provides facilities to send a message to all connected clients.
// Provides callback for handling incoming messages from connected clients.
//
// Named pipe operates in MESSAGE mode.
//
class NamedPipesServer
{
public:
	NamedPipesServer(
		const char* const pipeName,
		NamedPipesServerClientHandler::msg_handler_t msgHandler,
		size_t maxClients = 5);

	virtual ~NamedPipesServer();

public:
	void Start();
	void Stop();
	bool IsRunning();

	void WriteMessage(const char* const buffer, const size_t length);

private:
	void ThreadProc();
	bool CanAddHandler();
	void RemoveDisconnectedClients();
	void DisconnectAllClients();

private:
	size_t m_maxClients;
	std::recursive_mutex m_mutex;
	std::string m_pipeName;
	bool m_running;
	std::thread m_thread;
	NamedPipesServerClientHandler::msg_handler_t m_msgHandler;

	std::list<NamedPipesServerClientHandler*> m_clients;
};
