#pragma once

#include <windows.h>
#include <string>
#include <thread>
#include <mutex>

// Single named pipes client handler.
//
// Instantiated and managed by NamedPipesServer class.
//
// Provides facility to send messages to the connected client.
// Provides callback to handle incoming message from client.
//
class NamedPipesServerClientHandler
{
public:
	typedef std::function<void(const char* const, const size_t)> msg_handler_t;

public:
	NamedPipesServerClientHandler(HANDLE hPipe, msg_handler_t msgHandler);
	virtual ~NamedPipesServerClientHandler();

public:
	bool IsConnected();
	void Disconnect();
	bool WriteMessage(const char* const buffer, size_t length);

private:
	void ThreadProc();

private:
	HANDLE m_hPipe;
	std::thread m_thread;
	msg_handler_t m_msgHandler;
	std::recursive_mutex m_mutex;
};

