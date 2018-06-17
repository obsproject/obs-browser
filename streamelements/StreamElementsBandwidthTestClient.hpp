#pragma once

#include "StreamElementsAsyncTaskQueue.hpp"

#include <string>
#include <vector>

#include <obs.h>
#include <util/platform.h>
#include <util/threading.h>

class StreamElementsBandwidthTestClient
{
public:
	class Server
	{
	public:
		std::string url;
		std::string streamKey;
		bool useAuth = false;
		std::string authUsername;
		std::string authPassword;

		Server() { }
		Server(const char* const url, const char* const streamKey, bool useAuth = false, const char* username = nullptr, const char* password = nullptr):
			url(url),
			streamKey(streamKey),
			useAuth(useAuth),
			authUsername(username != nullptr ? username : ""),
			authPassword(password != nullptr ? password : "")
		{ }

		Server(Server& other) :
			url(other.url),
			streamKey(other.streamKey),
			useAuth(other.useAuth),
			authUsername(other.authUsername),
			authPassword(other.authPassword)
		{ }
	};

	class Result
	{
	public:
		bool success = false;
		bool cancelled = false;
		std::string serverUrl;
		std::string streamKey;
		uint64_t bitsPerSecond = 0L;
		uint64_t connectTimeMilliseconds = 0L;

		Result() { }
		Result(Result& other):
			success(other.success),
			cancelled(other.cancelled),
			serverUrl(other.serverUrl),
			streamKey(other.streamKey),
			bitsPerSecond(other.bitsPerSecond),
			connectTimeMilliseconds(other.connectTimeMilliseconds) { }
	};

private:
	// ignore first WARMUP_DURATION_MS due to possible buffering skewing
	// the result
	const unsigned long WARMUP_DURATION_MS = 2500L;

private:
	enum state_enum {
		Running = 0,
		Stopped = 1,
		Cancelled = 2
	};

	os_event_t* m_event_state_changed;
	os_event_t* m_event_async_done;
	state_enum m_state = Stopped;
	bool m_async_busy = false;

private:
	void wait_state_changed() { os_event_wait(m_event_state_changed); }
	void wait_state_changed(unsigned long milliseconds) { os_event_timedwait(m_event_state_changed, milliseconds); }
	void signal_state_changed() { os_event_signal(m_event_state_changed); }
	void set_state(const state_enum new_state) { m_state = new_state; signal_state_changed(); }

public:
	typedef void(*TestServerBitsPerSecondAsyncCallback)(Result*, void*);
	typedef void(*TestMultipleServersBitsPerSecondAsyncCallback)(std::vector<Result>*, void*);

	StreamElementsBandwidthTestClient();
	virtual ~StreamElementsBandwidthTestClient();

	void TestServerBitsPerSecondAsync(
		const char* const serverUrl,
		const char* const streamKey,
		const int maxBitrateBitsPerSecond,
		const char* const bindToIP,
		const int durationSeconds,
		const bool useAuth,
		const char* const authUsername,
		const char* const authPassword,
		const TestServerBitsPerSecondAsyncCallback callback,
		void* const data);

	void TestMultipleServersBitsPerSecondAsync(
		std::vector<Server> servers,
		const int maxBitrateBitsPerSecond,
		const char* const bindToIP,
		const int durationSeconds,
		const TestMultipleServersBitsPerSecondAsyncCallback progress_callback,
		const TestMultipleServersBitsPerSecondAsyncCallback callback,
		void* const data);

	void CancelAll();

private:
	Result TestServerBitsPerSecond(
		const char* serverUrl,
		const char* streamKey,
		const int maxBitrateBitsPerSecond,
		const char* bindToIP = nullptr,
		const int durationSeconds = 10,
		const bool useAuth = false,
		const char* const authUsername = nullptr,
		const char* const authPassword = nullptr);
};
