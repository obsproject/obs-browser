#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"

#include "cef-headers.hpp"
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include <QUuid>

#include <algorithm>    // std::sort

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
	blog(LOG_INFO, "obs-browser: central widget: loading url: %s", url);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	StreamElementsBrowserWidget* widget = new StreamElementsBrowserWidget(nullptr, url, executeJavaScriptCodeOnLoad, "center", "");

	PushCentralWidget(widget);
}

bool StreamElementsBrowserWidgetManager::DestroyCurrentCentralBrowserWidget()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return DestroyCurrentCentralWidget();
}

std::string StreamElementsBrowserWidgetManager::AddDockBrowserWidget(CefRefPtr<CefValue> input, std::string requestId)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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

				//widget->setSizePolicy(sizePolicy);
				widget->widget()->setSizePolicy(sizePolicy);

				//widget->setMinimumSize(requestWidth, requestHeight);
				widget->widget()->setMinimumSize(requestWidth, requestHeight);

				QApplication::sendPostedEvents();

				//widget->setMinimumSize(minWidth, minHeight);
				widget->widget()->setMinimumSize(minWidth, minHeight);

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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	while (m_browserWidgets.size()) {
		RemoveDockWidget(m_browserWidgets.begin()->first.c_str());
	}
}

bool StreamElementsBrowserWidgetManager::RemoveDockWidget(const char* const id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (StreamElementsWidgetManager::RemoveDockWidget(id)) {
		if (m_browserWidgets.count(id)) {
			m_browserWidgets.erase(id);
		}
	}
}

void StreamElementsBrowserWidgetManager::GetDockBrowserWidgetIdentifiers(std::vector<std::string>& result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return GetDockWidgetIdentifiers(result);
}

StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* StreamElementsBrowserWidgetManager::GetDockBrowserWidgetInfo(const char* const id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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

			QDockWidget* widget = GetDockWidget(id.c_str());

			QSize minSize = widget->widget()->minimumSize();
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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!input.get()) {
		return;
	}

	CefRefPtr<CefDictionaryValue> rootDictionary =
		input->GetDictionary();

	CefDictionaryValue::KeyList rootDictionaryKeys;

	if (!rootDictionary->GetKeys(rootDictionaryKeys)) {
		return;
	}

	// 1. Build maps:
	//	area -> start coord -> vector of ids
	//	id -> secondary start coord
	//	id -> minimum size
	//
	typedef std::vector<std::string> id_arr_t;
	typedef std::map<int, id_arr_t> start_to_ids_map_t;
	typedef std::map<std::string, start_to_ids_map_t> docks_map_t;

	docks_map_t docksMap;
	std::map<std::string, int> secondaryStartMap;

	for (auto id : rootDictionaryKeys) {
		CefRefPtr<CefValue> widgetValue = rootDictionary->GetValue(id);

		if (widgetValue->GetDictionary()->HasKey("dockingArea")) {
			std::string area = widgetValue->GetDictionary()->GetString("dockingArea").ToString();

			int start = -1;
			int secondary = -1;

			if (widgetValue->GetDictionary()->HasKey("left") && widgetValue->GetDictionary()->HasKey("top")) {
				if ((area == "left" || area == "right")) {
					start = widgetValue->GetDictionary()->GetInt("top");
					secondary = widgetValue->GetDictionary()->GetInt("left");
				}
				else if ((area == "top" || area == "bottom")) {
					start = widgetValue->GetDictionary()->GetInt("left");
					secondary = widgetValue->GetDictionary()->GetInt("top");
				}
			}

			docksMap[area][start].emplace_back(id.ToString());
			secondaryStartMap[id.ToString()] = secondary;
		}
	}

	// 2. For each area
	//
	for (docks_map_t::iterator areaPair = docksMap.begin(); areaPair != docksMap.end(); ++areaPair) {
		std::string area = areaPair->first;

		for (start_to_ids_map_t::iterator startPair = areaPair->second.begin(); startPair != areaPair->second.end(); ++startPair) {
			int start = startPair->first;
			id_arr_t dockIds = startPair->second;

			// 3. Sort dock IDs by secondary start coord
			std::sort(dockIds.begin(), dockIds.end(), [&](std::string i, std::string j) -> bool {
				return secondaryStartMap[i] < secondaryStartMap[j];
			});

			std::map<std::string, QSize> idToMinSizeMap;

			// 4. For each dock Id
			//
			for (int i = 0; i < dockIds.size(); ++i) {
				CefRefPtr<CefValue> widgetValue = rootDictionary->GetValue(dockIds[i]);

				// 5. Create docking widget
				//
				dockIds[i] = AddDockBrowserWidget(widgetValue, dockIds[i]);

				/*
				QDockWidget* prev = i > 0 ? GetDockWidget(dockIds[i - 1].c_str()) : nullptr;
				QDockWidget* curr = GetDockWidget(dockIds[i].c_str());

				idToMinSizeMap[dockIds[i]] = curr->widget()->minimumSize();

				if (prev && curr) {
					// 6. If it's not the first: split dock widgets which should occupy the
					//    same space, and set previous minimum size
					//
					if (area == "left" || area == "right") {
						mainWindow()->splitDockWidget(prev, curr, Qt::Horizontal);

					}
					else if (area == "top" || area == "bottom") {
						mainWindow()->splitDockWidget(prev, curr, Qt::Vertical);
					}

					QApplication::sendPostedEvents();
					prev->setMinimumSize(idToMinSizeMap[dockIds[i - 1]]);
					prev->widget()->setMinimumSize(idToMinSizeMap[dockIds[i - 1]]);
					QApplication::sendPostedEvents();
				}
				*/
			}
		}

	}
}

void StreamElementsBrowserWidgetManager::ShowNotificationBar(
	const char* const url,
	const int height,
	const char* const executeJavaScriptCodeOnLoad)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_notificationBarToolBar) {
		m_notificationBarToolBar->setVisible(false);

		QApplication::sendPostedEvents();

		mainWindow()->removeToolBar(m_notificationBarToolBar);

		m_notificationBarToolBar->deleteLater();

		m_notificationBarToolBar = nullptr;
		m_notificationBarBrowserWidget = nullptr;
	}
}

void StreamElementsBrowserWidgetManager::SerializeNotificationBar(CefRefPtr<CefValue>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

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
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefValue> root = CefValue::Create();

	SerializeNotificationBar(root);

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	output = jsonString.ToString();
}

void StreamElementsBrowserWidgetManager::DeserializeNotificationBar(std::string& input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Convert JSON string to CefValue
	CefRefPtr<CefValue> root =
		CefParseJSON(CefString(input), JSON_PARSER_ALLOW_TRAILING_COMMAS);

	DeserializeNotificationBar(root);
}
