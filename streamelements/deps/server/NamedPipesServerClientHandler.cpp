#include "NamedPipesServerClientHandler.hpp"
#include <memory.h>
#include <obs.h>

static const size_t BUFLEN = 32768;

NamedPipesServerClientHandler::NamedPipesServerClientHandler(HANDLE hPipe, msg_handler_t msgHandler) :
	m_hPipe(hPipe),
	m_msgHandler(msgHandler)
{
	m_thread = std::thread([this]() {
		ThreadProc();
	});
}

NamedPipesServerClientHandler::~NamedPipesServerClientHandler()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	Disconnect();

	if (m_thread.joinable()) {
		m_thread.join();
	}
}

bool NamedPipesServerClientHandler::IsConnected()
{
	return (m_hPipe != INVALID_HANDLE_VALUE);
}

void NamedPipesServerClientHandler::Disconnect()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (IsConnected()) {
		DisconnectNamedPipe(m_hPipe);

		CloseHandle(m_hPipe);

		m_hPipe = INVALID_HANDLE_VALUE;
	}
}

bool NamedPipesServerClientHandler::WriteMessage(const char* const buffer, size_t length)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!IsConnected()) {
		return false;
	}

	bool fWriteSuccess = TRUE == WriteFile(
		m_hPipe,
		buffer,
		length,
		NULL,
		NULL);

	if (!fWriteSuccess) {
		blog(LOG_WARNING, "obs-browser: NamedPipesServerClientHandler::SendMessage: client disconnected");

		Disconnect();
	}

	return fWriteSuccess;
}

void NamedPipesServerClientHandler::ThreadProc()
{
	while (IsConnected()) {
		bool had_activity = false;

		{
			std::lock_guard<std::recursive_mutex> guard(m_mutex);

			DWORD bytesRead;
			DWORD totalBytesAvail;
			DWORD bytesLeftThisMessage;

			const bool fPeekSuccess = PeekNamedPipe(
				m_hPipe,
				NULL,
				0,
				&bytesRead,
				&totalBytesAvail,
				&bytesLeftThisMessage);

			if (!fPeekSuccess) {
				blog(LOG_WARNING, "obs-browser: NamedPipesServerClientHandler: PeekNamedPipe: client disconnected");

				Disconnect();
			}
			else if (bytesLeftThisMessage > 0) {
				char* buffer = new char[bytesLeftThisMessage + 1];
				bool fReadSuccess = TRUE == ReadFile(
					m_hPipe,
					buffer,
					bytesLeftThisMessage,
					NULL,
					NULL);

				if (fReadSuccess) {
					buffer[bytesLeftThisMessage] = 0;

					m_msgHandler(buffer, bytesLeftThisMessage);

					blog(LOG_DEBUG, "obs-browser: NamedPipesServerClientHandler: incoming message: %s", buffer);
				}
				else {
					blog(LOG_WARNING, "obs-browser: NamedPipesServerClientHandler: ReadFile: client disconnected");

					Disconnect();
				}

				delete[] buffer;

				had_activity = true;
			}
		}
		
		if (!had_activity) {
			Sleep(250);
		}
	}
}
