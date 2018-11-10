#pragma once

#include <mutex>
#include <list>
#include <map>

#include "cef-headers.hpp"

class StreamElementsMessageBus
{
public:
	typedef uint32_t message_destination_filter_flags_t;

	static const message_destination_filter_flags_t DEST_ALL;
	static const message_destination_filter_flags_t DEST_UI;
	static const message_destination_filter_flags_t DEST_WORKER;
	static const message_destination_filter_flags_t DEST_BROWSER_SOURCE;

	static const char* const SOURCE_WEB;

protected:
	StreamElementsMessageBus();
	virtual ~StreamElementsMessageBus();

public:
	static StreamElementsMessageBus* GetInstance();

public:
	void AddBrowserListener(CefRefPtr<CefBrowser> browser, message_destination_filter_flags_t type);
	void RemoveBrowserListener(CefRefPtr<CefBrowser> browser);

public:
	void NotifyAllBrowserListeners(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		std::string event,
		CefRefPtr<CefValue> payload);

	void BroadcastMessageToBrowsers(
		message_destination_filter_flags_t types,
		std::string source,
		std::string sourceAddress,
		CefRefPtr<CefValue> payload)
	{
		NotifyAllBrowserListeners(
			types,
			source,
			sourceAddress,
			"hostMessageReceived",
			payload);
	}

private:
	std::recursive_mutex m_browser_list_mutex;
	//std::list<CefRefPtr<CefBrowser>> m_browser_list;
	std::map<CefRefPtr<CefBrowser>, message_destination_filter_flags_t> m_browser_list;

private:
	static StreamElementsMessageBus* s_instance;
};
