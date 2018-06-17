#pragma once

#include "StreamElementsWidgetManager.hpp"
#include "StreamElementsBrowserWidget.hpp"

class StreamElementsBrowserWidgetManager :
	protected StreamElementsWidgetManager
{
public:
	class DockBrowserWidgetInfo :
		public DockWidgetInfo
	{
	public:
		DockBrowserWidgetInfo()
		{
		}

		DockBrowserWidgetInfo(DockWidgetInfo& other): DockWidgetInfo(other)
		{
		}

		DockBrowserWidgetInfo(DockBrowserWidgetInfo& other) : DockWidgetInfo(other)
		{
			m_url = other.m_url;
			m_executeJavaScriptOnLoad = other.m_executeJavaScriptOnLoad;
		}

	public:
		std::string m_url;
		std::string m_executeJavaScriptOnLoad;
	};

public:
	StreamElementsBrowserWidgetManager(QMainWindow* parent);
	virtual ~StreamElementsBrowserWidgetManager();

	/********************/
	/* Central widget   */
	/********************/

	void PushCentralBrowserWidget(
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad);

	bool DestroyCurrentCentralBrowserWidget();

	/********************/
	/* Dockable widgets */
	/********************/

	std::string AddDockBrowserWidget(CefRefPtr<CefValue> input, std::string requestId = "");

	bool AddDockBrowserWidget(
		const char* const id,
		const char* const title,
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad,
		const Qt::DockWidgetArea area,
		const Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
		const QDockWidget::DockWidgetFeatures features = QDockWidget::AllDockWidgetFeatures);

	virtual bool RemoveDockWidget(const char* const id) override;

	void RemoveAllDockWidgets();

	void GetDockBrowserWidgetIdentifiers(std::vector<std::string>& result);

	DockBrowserWidgetInfo* GetDockBrowserWidgetInfo(const char* const id);

	QDockWidget* GetDockWidget(const char* const id) {
		return StreamElementsWidgetManager::GetDockWidget(id);
	}

	virtual void SerializeDockingWidgets(CefRefPtr<CefValue>& output) override;
	virtual void DeserializeDockingWidgets(CefRefPtr<CefValue>& input) override;

	void SerializeDockingWidgets(std::string& output) { StreamElementsWidgetManager::SerializeDockingWidgets(output); }
	void DeserializeDockingWidgets(std::string& input) { StreamElementsWidgetManager::DeserializeDockingWidgets(input); }

	/********************/
	/* Notification bar */
	/********************/

	void ShowNotificationBar(
		const char* const url,
		const int height,
		const char* const executeJavaScriptCodeOnLoad);

	void HideNotificationBar();

	virtual void SerializeNotificationBar(CefRefPtr<CefValue>& output);
	virtual void DeserializeNotificationBar(CefRefPtr<CefValue>& input);

	void SerializeNotificationBar(std::string& output);
	void DeserializeNotificationBar(std::string& input);

private:
	std::map<std::string, StreamElementsBrowserWidget*> m_browserWidgets;

	QToolBar* m_notificationBarToolBar;
	StreamElementsBrowserWidget* m_notificationBarBrowserWidget;
};
