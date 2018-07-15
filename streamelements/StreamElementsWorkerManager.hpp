#pragma once

#include "StreamElementsUtils.hpp"
#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsBrowserWidget.hpp"

#include "cef-headers.hpp"

#include <string>
#include <map>
#include <mutex>

class StreamElementsWorkerManager :
	public StreamElementsObsAppMonitor
{
private:
	class StreamElementsWorker;

public:
	StreamElementsWorkerManager();
	~StreamElementsWorkerManager();

public:
	void RemoveAll();
	std::string Add(std::string requestedId, std::string content, std::string url);
	void Remove(std::string id);
	std::string GetContent(std::string id);
	void GetIdentifiers(std::vector<std::string>& result);

public:
	void Serialize(CefRefPtr<CefValue>& output);
	void Deserialize(CefRefPtr<CefValue>& input);

	bool SerializeOne(std::string id, CefRefPtr<CefValue>& output);
	std::string DeserializeOne(CefRefPtr<CefValue>& input);

protected:
	virtual void OnObsExit();

private:
	std::recursive_mutex m_mutex;

	std::map<std::string, StreamElementsWorker*> m_items;
};
