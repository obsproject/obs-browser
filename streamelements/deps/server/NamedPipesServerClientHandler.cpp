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

	m_callback_thread = std::thread([this]() {
		CallbackThreadProc();
	});
}

NamedPipesServerClientHandler::~NamedPipesServerClientHandler()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	Disconnect();

	if (m_thread.joinable()) {
		m_thread.join();
	}

	if (m_callback_thread.joinable()) {
		m_callback_thread.join();
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
	if (!IsConnected()) {
		return false;
	}

	m_writeQueue.enqueue(std::vector<char>(buffer, buffer + length));

	return true;
}

void NamedPipesServerClientHandler::CallbackThreadProc()
{
	while (IsConnected()) {
		std::vector<char> incoming_message;

		if (m_readQueue.try_dequeue(incoming_message)) {
			m_msgHandler(incoming_message.data(), incoming_message.size());
		}
		else Sleep(25);
	}
}

void NamedPipesServerClientHandler::ThreadProc()
{
	while (IsConnected()) {
		bool had_activity = false;
		bool continue_reading = true;

		while (IsConnected() && continue_reading)
		{
			continue_reading = false;

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
					continue_reading = true;

					buffer[bytesLeftThisMessage] = 0;

					m_readQueue.enqueue(std::vector<char>(buffer, buffer + bytesLeftThisMessage));

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

		std::vector<char> message_to_write;
		if (IsConnected() && m_writeQueue.try_dequeue(message_to_write)) {
			bool fWriteSuccess = TRUE == WriteFile(
				m_hPipe,
				message_to_write.data(),
				message_to_write.size(),
				NULL,
				NULL);

			if (!fWriteSuccess) {
				Disconnect();
			}

			had_activity = true;
		}
	
		if (!had_activity) {
			Sleep(25);
		}
	}
}
