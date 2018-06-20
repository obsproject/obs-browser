#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"

#include "cef-headers.hpp"
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include <QUuid>

StreamElementsBrowserWidgetManager::StreamElementsBrowserWidgetManager(QMainWindow* parent) :
	StreamElementsWidgetManager(parent),
	m_notificationBarToolBar(nullptr)
{
}

StreamElementsBrowserWidgetManager::~StreamElementsBrowserWidgetManager()
{
}

void StreamElementsBrowserWidgetManager::PushCentralBrowserWidget(
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad)
{
	StreamElementsBrowserWidget* widget = new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad, "center", "");

	PushCentralWidget(widget);
}

bool StreamElementsBrowserWidgetManager::DestroyCurrentCentralBrowserWidget()
{
	return DestroyCurrentCentralWidget();
}

std::string StreamElementsBrowserWidgetManager::AddDockBrowserWidget(CefRefPtr<CefValue> input, std::string requestId)
{
	std::string id = QUuid::createUuid().toString().toStdString();

	if (requestId.size() && !m_browserWidgets.count(requestId)) {
		id = requestId;
	}

	CefRefPtr<CefDictionaryValue> widgetDictionary = input->GetDictionary();

	if (widgetDictionary.get()) {
		if (widgetDictionary->HasKey("title") && widgetDictionary->HasKey("url")) {
			std::string title;
			std::string url;
			std::string dockingAreaString = "floating";
			std::string executeJavaScriptOnLoad;
			bool visible = true;
			QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
			int requestWidth = 100;
			int requestHeight = 100;
			int minWidth = 0;
			int minHeight = 0;
			int left = -1;
			int top = -1;

			QRect rec = QApplication::desktop()->screenGeometry();

			title = widgetDictionary->GetString("title");
			url = widgetDictionary->GetString("url");

			if (widgetDictionary->HasKey("dockingArea")) {
				dockingAreaString = widgetDictionary->GetString("dockingArea");
			}

			if (widgetDictionary->HasKey("executeJavaScriptOnLoad")) {
				executeJavaScriptOnLoad = widgetDictionary->GetString("executeJavaScriptOnLoad");
			}

			if (widgetDictionary->HasKey("visible")) {
				visible = widgetDictionary->GetBool("visible");
			}

			Qt::DockWidgetArea dockingArea = Qt::NoDockWidgetArea;

			if (dockingAreaString == "left") {
				dockingArea = Qt::LeftDockWidgetArea;
				sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
			}
			else if (dockingAreaString == "right") {
				dockingArea = Qt::RightDockWidgetArea;
				sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
			}
			else if (dockingAreaString == "top") {
				dockingArea = Qt::TopDockWidgetArea;
				sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
			}
			else if (dockingAreaString == "bottom") {
				dockingArea = Qt::BottomDockWidgetArea;
				sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
			}
			else
			{
				sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding);
				sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
			}

			if (widgetDictionary->HasKey("minWidth")) {
				minWidth = widgetDictionary->GetInt("minWidth");
			}

			if (widgetDictionary->HasKey("width")) {
				requestWidth = widgetDictionary->GetInt("width");
			}

			if (widgetDictionary->HasKey("minHeight")) {
				minHeight = widgetDictionary->GetInt("minHeight");
			}

			if (widgetDictionary->HasKey("height")) {
				requestHeight = widgetDictionary->GetInt("height");
			}

			if (widgetDictionary->HasKey("left")) {
				left = widgetDictionary->GetInt("left");
			}

			if (widgetDictionary->HasKey("top")) {
				top = widgetDictionary->GetInt("top");
			}

			if (AddDockBrowserWidget(
				id.c_str(),
				title.c_str(),
				url.c_str(),
				executeJavaScriptOnLoad.c_str(),
				dockingArea)) {
				QDockWidget* widget = GetDockWidget(id.c_str());

				widget->setVisible(!visible);
				QApplication::sendPostedEvents();
				widget->setVisible(visible);
				QApplication::sendPostedEvents();

				widget->setSizePolicy(sizePolicy);
				widget->setMinimumSize(requestWidth, requestHeight);

				QApplication::sendPostedEvents();

				widget->setMinimumSize(minWidth, minHeight);

				if (left >= 0 || top >= 0) {
					widget->move(left, top);
				}

				QApplication::sendPostedEvents();
			}

			return id;
		}
	}

	return "";
}

bool StreamElementsBrowserWidgetManager::AddDockBrowserWidget(
	const char* const id,
	const char* const title,
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad,
	const Qt::DockWidgetArea area,
	const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	StreamElementsBrowserWidget* widget = new StreamElementsBrowserWidget(
		nullptr, url, executeJavaScriptCodeOnLoad, DockWidgetAreaToString(area).c_str(), id);

	if (AddDockWidget(id, title, widget, area, allowedAreas, features)) {
		m_browserWidgets[id] = widget;

		return true;
	} else {
		return false;
	}
}

void StreamElementsBrowserWidgetManager::RemoveAllDockWidgets()
{
	while (m_browserWidgets.size()) {
		RemoveDockWidget(m_browserWidgets.begin()->first.c_str());
	}
}

bool StreamElementsBrowserWidgetManager::RemoveDockWidget(const char* const id)
{
	if (m_browserWidgets.count(id)) {
		m_browserWidgets.erase(id);
	}
	return StreamElementsWidgetManager::RemoveDockWidget(id);
}

void StreamElementsBrowserWidgetManager::GetDockBrowserWidgetIdentifiers(std::vector<std::string>& result)
{
	return GetDockWidgetIdentifiers(result);
}

StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* StreamElementsBrowserWidgetManager::GetDockBrowserWidgetInfo(const char* const id)
{
	StreamElementsBrowserWidgetManager::DockWidgetInfo* baseInfo = GetDockWidgetInfo(id);

	if (!baseInfo) {
		return nullptr;
	}

	StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* result =
		new StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo(*baseInfo);

	delete baseInfo;

	result->m_url = m_browserWidgets[id]->GetCurrentUrl();

	result->m_executeJavaScriptOnLoad = m_browserWidgets[id]->GetExecuteJavaScriptCodeOnLoad();

	return result;
}

void StreamElementsBrowserWidgetManager::SerializeDockingWidgets(CefRefPtr<CefValue>& output)
{
	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	output->SetDictionary(rootDictionary);

	std::vector<std::string> widgetIdentifiers;

	GetDockBrowserWidgetIdentifiers(widgetIdentifiers);

	for (auto id : widgetIdentifiers) {
		CefRefPtr<CefValue> widgetValue = CefValue::Create();
		CefRefPtr<CefDictionaryValue> widgetDictionary = CefDictionaryValue::Create();
		widgetValue->SetDictionary(widgetDictionary);

		StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* info =
			GetDockBrowserWidgetInfo(id.c_str());

		if (info) {
			widgetDictionary->SetString("id", id);
			widgetDictionary->SetString("url", info->m_url);
			widgetDictionary->SetString("dockingArea", info->m_dockingArea);
			widgetDictionary->SetString("title", info->m_title);
			widgetDictionary->SetString("executeJavaScriptOnLoad", info->m_executeJavaScriptOnLoad);
			widgetDictionary->SetBool("visible", info->m_visible);

			QWidget* widget = GetDockWidget(id.c_str());

			QSize minSize = widget->minimumSize();
			QSize size = widget->size();

			widgetDictionary->SetInt("minWidth", minSize.width());
			widgetDictionary->SetInt("minHeight", minSize.height());

			widgetDictionary->SetInt("width", size.width());
			widgetDictionary->SetInt("height", size.height());

			widgetDictionary->SetInt("left", widget->pos().x());
			widgetDictionary->SetInt("top", widget->pos().y());
		}

		rootDictionary->SetValue(id, widgetValue);
	}
}

void StreamElementsBrowserWidgetManager::DeserializeDockingWidgets(CefRefPtr<CefValue>& input)
{
	if (!input.get()) {
		return;
	}

	CefRefPtr<CefDictionaryValue> rootDictionary =
		input->GetDictionary();

	CefDictionaryValue::KeyList rootDictionaryKeys;

	if (!rootDictionary->GetKeys(rootDictionaryKeys)) {
		return;
	}

	for (auto id : rootDictionaryKeys) {
		CefRefPtr<CefValue> widgetValue = rootDictionary->GetValue(id);

		AddDockBrowserWidget(widgetValue, id);
	}
}

void StreamElementsBrowserWidgetManager::ShowNotificationBar(
	const char* const url,
	const int height,
	const char* const executeJavaScriptCodeOnLoad)
{
	HideNotificationBar();

	m_notificationBarBrowserWidget = new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad, "notification", "");

	const Qt::ToolBarArea NOTIFICATION_BAR_AREA = Qt::TopToolBarArea;

	m_notificationBarToolBar = new QToolBar(mainWindow());
	m_notificationBarToolBar->setAutoFillBackground(true);
	m_notificationBarToolBar->setAllowedAreas(NOTIFICATION_BAR_AREA);
	m_notificationBarToolBar->setFloatable(false);
	m_notificationBarToolBar->setMovable(false);
	m_notificationBarToolBar->setMinimumHeight(height);
	m_notificationBarToolBar->setMaximumHeight(height);
	m_notificationBarToolBar->setLayout(new QVBoxLayout());
	m_notificationBarToolBar->addWidget(m_notificationBarBrowserWidget);
	mainWindow()->addToolBar(NOTIFICATION_BAR_AREA, m_notificationBarToolBar);
}

void StreamElementsBrowserWidgetManager::HideNotificationBar()
{
	if (m_notificationBarToolBar) {
		m_notificationBarToolBar->setVisible(false);

		QApplication::sendPostedEvents();

		mainWindow()->removeToolBar(m_notificationBarToolBar);

		m_notificationBarToolBar = nullptr;
	}
}

void StreamElementsBrowserWidgetManager::SerializeNotificationBar(CefRefPtr<CefValue>& output)
{
	if (m_notificationBarToolBar) {
		CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
		output->SetDictionary(rootDictionary);

		rootDictionary->SetString("url", m_notificationBarBrowserWidget->GetCurrentUrl());
		rootDictionary->SetString("executeJavaScriptOnLoad", m_notificationBarBrowserWidget->GetExecuteJavaScriptCodeOnLoad());
		rootDictionary->SetInt("height", m_notificationBarToolBar->size().height());		
	}
	else {
		output->SetNull();
	}
}

void StreamElementsBrowserWidgetManager::DeserializeNotificationBar(CefRefPtr<CefValue>& input)
{
	if (input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> rootDictionary = input->GetDictionary();

		int height = 100;
		std::string url = "about:blank";
		std::string executeJavaScriptOnLoad = "";

		if (rootDictionary->HasKey("height")) {
			height = rootDictionary->GetInt("height");
		}

		if (rootDictionary->HasKey("url")) {
			url = rootDictionary->GetString("url").ToString();
		}

		if (rootDictionary->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad = rootDictionary->GetString("executeJavaScriptOnLoad").ToString();
		}

		ShowNotificationBar(url.c_str(), height, executeJavaScriptOnLoad.c_str());
	}
}

void StreamElementsBrowserWidgetManager::SerializeNotificationBar(std::string& output)
{
	CefRefPtr<CefValue> root = CefValue::Create();

	SerializeNotificationBar(root);

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	output = jsonString.ToString();
}

void StreamElementsBrowserWidgetManager::DeserializeNotificationBar(std::string& input)
{
	// Convert JSON string to CefValue
	CefRefPtr<CefValue> root =
		CefParseJSON(CefString(input), JSON_PARSER_ALLOW_TRAILING_COMMAS);

	DeserializeNotificationBar(root);
}
