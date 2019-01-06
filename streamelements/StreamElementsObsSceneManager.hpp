#pragma once

#include "cef-headers.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QApplication>

#include <mutex>

class StreamElementsObsSceneManager
{
public:
	StreamElementsObsSceneManager(QMainWindow* parent);
	virtual ~StreamElementsObsSceneManager();

public:
	void DeserializeObsBrowserSource(
		CefRefPtr<CefValue>& input,
		CefRefPtr<CefValue>& output);

	void SerializeObsCurrentSceneItems(
		CefRefPtr<CefValue>& output);

protected:
	QMainWindow* mainWindow() { return m_parent; }

	void ObsAddSourceInternal(
		obs_source_t* parentScene,
		const char* sourceId,
		const char* sourceName,
		obs_data_t* sourceSettings,
		obs_data_t* sourceHotkeyData,
		bool preferExistingSource,
		obs_source_t** output_source,
		obs_sceneitem_t** output_sceneitem);

	std::string ObsGetUniqueSourceName(std::string name);

protected:
	std::recursive_mutex m_mutex;

private:
	QMainWindow* m_parent;
};
