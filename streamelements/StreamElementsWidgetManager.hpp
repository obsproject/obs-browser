#pragma once

#include <QWidget>
#include <QDockWidget>
#include <QMainWindow>
#include <stack>
#include <map>
#include <mutex>

#include "../cef-headers.hpp"

#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsUtils.hpp"

class StreamElementsWidgetManager :
	public StreamElementsObsAppMonitor
{
public:
	class DockWidgetInfo
	{
	public:
		std::string m_id;
		std::string m_title;
		bool m_visible;
		std::string m_dockingArea;

	public:
		DockWidgetInfo()
		{ }

		DockWidgetInfo(DockWidgetInfo& other)
		{
			m_id = other.m_id;
			m_title = other.m_title;
			m_visible = other.m_visible;
			m_dockingArea = other.m_dockingArea;

			m_widget = other.m_widget;
		}

	private:
		QWidget* m_widget;

		friend class StreamElementsWidgetManager;

	public:
		QWidget* GetWidget() { return m_widget; }
	};

public:
	StreamElementsWidgetManager(QMainWindow* parent);
	~StreamElementsWidgetManager();

	/* central widget */

	void PushCentralWidget(QWidget* widget);
	bool DestroyCurrentCentralWidget();

	/* dockable widgets */

	bool AddDockWidget(
		const char* const id,
		const char* const title,
		QWidget* const widget,
		const Qt::DockWidgetArea area,
		const Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
		const QDockWidget::DockWidgetFeatures features = QDockWidget::AllDockWidgetFeatures);

	virtual bool RemoveDockWidget(const char* const id);

	void GetDockWidgetIdentifiers(std::vector<std::string>& result);

	QDockWidget* GetDockWidget(const char* const id);

	DockWidgetInfo* GetDockWidgetInfo(const char* const id);

	virtual void SerializeDockingWidgets(CefRefPtr<CefValue>& output) = 0;
	virtual void DeserializeDockingWidgets(CefRefPtr<CefValue>& input) = 0;

	void SerializeDockingWidgets(std::string& output);
	void DeserializeDockingWidgets(std::string& input);

protected:
	QMainWindow* mainWindow() { return m_parent; }

private:
	QMainWindow* m_parent;

	std::stack<QWidget*> m_centralWidgetStack;

	std::map<std::string, QDockWidget*> m_dockWidgets;
	std::map<std::string, Qt::DockWidgetArea> m_dockWidgetAreas;

	std::map<std::string, QSize> m_dockWidgetSavedMinSize;

protected:
	std::recursive_mutex m_mutex;

protected:
	void SaveDockWidgetsGeometry();
	void RestoreDockWidgetsGeometry();


protected:
	virtual void OnObsExit();
};
