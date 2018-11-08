#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "cef-headers.hpp"
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#include <QUuid>
#include <QMainWindow>
#include <QSpacerItem>

#include <algorithm>    // std::sort
#include <functional>
#include <regex>

#include <obs-module.h>

static const bool ENABLE_CUSTOM_DOCK_WIDGET_TITLE_BAR = true;

class LocalTitleWidget : public QWidget
{
private:
	void updateStyleSheet()
	{
		std::string css =
			//GetAppStyleSheetSelectorContent("QWidget") +
			//GetAppStyleSheetSelectorContent("QDockWidget") +
			GetAppStyleSheetSelectorContent("QDockWidget::title") +
			"";

		if (this->styleSheet() != css.c_str()) {
			this->setStyleSheet(css.c_str());
		}
	}

public:
	LocalTitleWidget(QWidget* parent) : QWidget(parent)
	{
		updateStyleSheet();
	}

	virtual void changeEvent(QEvent* event) override
	{
		if (event->type() == QEvent::StyleChange)
		{
			// Style has been changed.
			updateStyleSheet();
		}

		QWidget::changeEvent(event);
	}

	/*
	virtual void paintEvent(QPaintEvent*) override
	{
		QStyleOptionTitleBar opt;
		//QStyleOption opt;
		opt.init(this);
		QPainter p(this);

		style()->drawControl(QStyle::CE_DockWidgetTitle, &opt, &p, this);
		//style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
	}
	*/

	static inline int perp(bool vertical, const QSize &size)
	{
		return vertical ? size.width() : size.height();
	}

	int titleHeight() const
	{
		QDockWidget *q = qobject_cast<QDockWidget*>(parentWidget());

		QSize items(0, 0);
		for (int i = 0; i < layout()->count(); ++i) {
			QSize curr = layout()->itemAt(i)->widget()->sizeHint();

			if (i == 0) {
				items = curr;
			}
			else {
				items = QSize(qMax(curr.width(), items.width()), qMax(curr.height(), items.height()));
			}
		}

		int buttonHeight = qMax(
			items.height(),
			style()->pixelMetric(QStyle::PM_SmallIconSize, 0, q) +
			q->style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, 0, q)
		);


		QFontMetrics titleFontMetrics = q->fontMetrics();
		int mw =
			q->style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, 0, q) * 2 +
			//q->style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, 0, q) +
			q->style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth, 0, q) +
			0;

		return qMax(buttonHeight, titleFontMetrics.height() + mw);
	}

	virtual QSize minimumSizeHint() const override
	{
		return sizeHint();
	}

	virtual QSize sizeHint() const override
	{
		ensurePolished();
		QStyleOptionTitleBar opt;
		opt.init(this);
		//int marginSize = style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, &opt, this);
		//int frameSize = style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth, &opt, this);
		//int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, &opt, this);

		int titleSize = titleHeight();

		//QDockWidget *w = qobject_cast<QDockWidget*>(parentWidget());

		QSize result = QSize(titleSize, titleSize);

		return result;
	}
};

class LocalTitleLabel : public QLabel
{
private:
	void updateStyleSheet()
	{
		std::string css =
			//GetAppStyleSheetSelectorContent("QWidget") +
			//GetAppStyleSheetSelectorContent("QDockWidget") +
			//GetAppStyleSheetSelectorContent("QDockWidget::title") +
			"";

		if (this->styleSheet() != css.c_str()) {
			this->setStyleSheet(css.c_str());
		}
	}

public:
	LocalTitleLabel(QString label) :
		QLabel(label)
	{
	}

protected:
	virtual void changeEvent(QEvent* event) override
	{
		if (event->type() == QEvent::StyleChange || event->type() == QEvent::ScreenChangeInternal)
		{
			// Style has been changed.
			updateStyleSheet();
		}

		update();

		QLabel::changeEvent(event);
	}
};

class LocalToolButton : public QToolButton
{
private:
	mutable int m_iconSize = -1;

public:
	LocalToolButton()
	{
		//setToolButtonStyle(Qt::ToolButtonTextOnly);
		//setFocusPolicy(Qt::NoFocus);

		//updateStyleSheet(true);
	}

	virtual void changeEvent(QEvent* event) override
	{
		if (event->type() == QEvent::StyleChange || event->type() == QEvent::ScreenChangeInternal)
		{
			// Style has been changed.
			//updateStyleSheet();
		}

		m_iconSize = -1;
		update();

		QToolButton::changeEvent(event);
	}

	virtual void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		QStyleOptionToolButton opt;
		opt.initFrom(this);
		opt.state |= QStyle::State_AutoRaise;
		if (style()->styleHint(QStyle::SH_DockWidget_ButtonsHaveFrame, 0, this))
		{
			if (isEnabled() && underMouse() && !isChecked() && !isDown())
				opt.state |= QStyle::State_Raised;
			if (isChecked())
				opt.state |= QStyle::State_On;
			if (isDown())
				opt.state |= QStyle::State_Sunken;
			style()->drawPrimitive(QStyle::PE_PanelButtonTool, &opt, &p, this);
		}

		opt.icon = icon();
		opt.subControls = 0;
		opt.activeSubControls = 0;
		opt.features = QStyleOptionToolButton::None;
		opt.arrowType = Qt::NoArrow;
		opt.iconSize = dockButtonIconSize(&opt);
		style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &p, this);
	}

	static inline bool isWindowsStyle(const QStyle *style)
	{
		// Note: QStyleSheetStyle inherits QWindowsStyle
		const QStyle *effectiveStyle = style;	

		//if (style->inherits("QStyleSheetStyle"))
		//	effectiveStyle = static_cast<const QStyleSheetStyle *>(style)->baseStyle();

		return effectiveStyle->inherits("QWindowsStyle");
	}

	QSize dockButtonIconSize(QStyleOption* styleOption) const
	{
		if (m_iconSize < 0) {
			m_iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, styleOption, this);

			// Dock Widget title buttons on Windows where historically limited to size 10
			// (from small icon size 16) since only a 10x10 XPM was provided.
			// Adding larger pixmaps to the icons thus caused the icons to grow; limit
			// this to qpiScaled(10) here.
			if (isWindowsStyle(style())) {
				m_iconSize = qMin((10 * logicalDpiX()) / 96, m_iconSize);
			}
		}

		return QSize(m_iconSize, m_iconSize);
	}

	virtual QSize sizeHint() const override
	{
		ensurePolished();
		int size = 2 * style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, 0, this);
		if (!icon().isNull()) {
			const QSize sz = icon().actualSize(dockButtonIconSize(nullptr));
			size += qMax(sz.width(), sz.height());
		}
		return QSize(size, size);
	}

	virtual void enterEvent(QEvent *event) override
	{
		if (isEnabled()) update();
		QAbstractButton::enterEvent(event);
	}

	virtual void leaveEvent(QEvent *event) override
	{
		if (isEnabled()) update();
		QAbstractButton::leaveEvent(event);
	}

};

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
	if (!url) {
		return;
	}

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

	const bool enableBackForwardNavigation = false;

	QMainWindow* main = new QMainWindow(nullptr);

	//QAction* floatAction = new QAction("🗗");
	//QAction* closeAction = new QAction("×");
	//QAction* backAction = new QAction("❮");
	//QAction* forwardAction = new QAction("❯");
	//QAction* reloadAction = new QAction("☀");

	QAction* backAction = new QAction(qApp->style()->standardIcon(QStyle::SP_ArrowLeft), "");
	QAction* forwardAction = new QAction(qApp->style()->standardIcon(QStyle::SP_ArrowRight), "");
	//QAction* reloadAction = new QAction(qApp->style()->standardIcon(QStyle::SP_BrowserReload), "");
	QAction* reloadAction = new QAction(QIcon(QPixmap(":/images/toolbar/reload.png")), "");
	QAction* floatAction = new QAction(qApp->style()->standardIcon(QStyle::SP_TitleBarNormalButton), "");
	QAction* closeAction = new QAction(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton), "");	

	QFont font;
	font.setStyleStrategy(QFont::PreferAntialias);

	backAction->setFont(font);
	forwardAction->setFont(font);
	reloadAction->setFont(font);
	floatAction->setFont(font);
	closeAction->setFont(font);

	backAction->setEnabled(false);
	forwardAction->setEnabled(false);

	StreamElementsBrowserWidget* widget = new StreamElementsBrowserWidget(
		nullptr, url, executeJavaScriptCodeOnLoad, DockWidgetAreaToString(area).c_str(), id);

	widget->connect(
		widget,
		&StreamElementsBrowserWidget::browserStateChanged,
		[this, widget, backAction, forwardAction]() {
		backAction->setEnabled(widget->BrowserHistoryCanGoBack());
		forwardAction->setEnabled(widget->BrowserHistoryCanGoForward());
	});

	main->setCentralWidget(widget);

	reloadAction->connect(
		reloadAction,
		&QAction::triggered,
		[widget] {
		widget->BrowserReload(true);
	});

	backAction->connect(
		backAction,
		&QAction::triggered,
		[widget] {
		widget->BrowserHistoryGoBack();
	});

	forwardAction->connect(
		backAction,
		&QAction::triggered,
		[widget] {
		widget->BrowserHistoryGoForward();
	});

	if (AddDockWidget(id, title, main, area, allowedAreas, features)) {
		m_browserWidgets[id] = widget;

		QDockWidget* dock = GetDockWidget(id);

		if (ENABLE_CUSTOM_DOCK_WIDGET_TITLE_BAR) {
			QWidget* titleWidget = new LocalTitleWidget(dock);

			titleWidget->setLayout(new QHBoxLayout());
			titleWidget->layout()->setMargin(2);

			dock->setTitleBarWidget(titleWidget);

			//QString buttonStyle = "QToolButton { border: none; padding: 4px; font-weight: bold; } QToolButton:!hover { background-color: transparent; }";
			//QString labelStyle = "QLabel { background-color: transparent; padding: 2px; }";

			auto createButton = [&](QAction* action, const char* toolTipText) {
				auto result = new LocalToolButton();

				result->setDefaultAction(action);

				result->setToolTip(toolTipText);
				action->setToolTip(toolTipText);

				return result;
			};

			auto backButton = createButton(backAction, obs_module_text("StreamElements.Action.BrowserBack.Tooltip"));
			auto forwardButton = createButton(forwardAction, obs_module_text("StreamElements.Action.BrowserForward.Tooltip"));
			auto reloadButton = createButton(reloadAction, obs_module_text("StreamElements.Action.BrowserReload.Tooltip"));
			auto floatButton = createButton(floatAction, obs_module_text("StreamElements.Action.DockToggleFloat.Tooltip"));
			auto closeButton = createButton(closeAction, obs_module_text("StreamElements.Action.DockHide.Tooltip"));

			floatAction->connect(
				floatAction,
				&QAction::triggered,
				[dock] {
				dock->setFloating(!dock->isFloating());
			});

			closeAction->connect(
				closeAction,
				&QAction::triggered,
				[dock] {
				dock->setVisible(false);

				StreamElementsGlobalStateManager::GetInstance()->
					GetAnalyticsEventsManager()->trackDockWidgetEvent(dock, "Hide", json11::Json::object{ { "actionSource", "Docking widget title bar" } });
			});

			auto windowTitle = new LocalTitleLabel(title);
			windowTitle->setAlignment(Qt::AlignCenter);
			//windowTitle->setStyleSheet(labelStyle);
			//windowTitle->setFont(font);

			if (enableBackForwardNavigation) {
				titleWidget->layout()->addWidget(backButton);
				titleWidget->layout()->addWidget(forwardButton);
			}

			titleWidget->layout()->addWidget(reloadButton);
			titleWidget->layout()->addWidget(windowTitle);
			titleWidget->layout()->addWidget(floatButton);
			titleWidget->layout()->addWidget(closeButton);

			backButton->connect(
				backButton,
				&QToolButton::click,
				[this, widget]
			{
				widget->BrowserHistoryGoBack();
			});

			forwardButton->connect(
				forwardButton,
				&QToolButton::click,
				[this, widget]
			{
				widget->BrowserHistoryGoForward();
			});

			reloadButton->connect(
				reloadButton,
				&QToolButton::click,
				[this, widget]
			{
				widget->BrowserReload(true);
			});

			dock->connect(
				dock,
				&QDockWidget::windowTitleChanged,
				[windowTitle](const QString value) {
				windowTitle->setText(value);
			});
		}

		return true;
	} else {
		return false;
	}
}

bool StreamElementsBrowserWidgetManager::ToggleWidgetFloatingStateById(const char* const id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return StreamElementsWidgetManager::ToggleWidgetFloatingStateById(id);
}

bool StreamElementsBrowserWidgetManager::SetWidgetDimensionsById(const char* const id, const int width, const int height)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return StreamElementsWidgetManager::SetWidgetDimensionsById(id, width, height);
}

bool StreamElementsBrowserWidgetManager::SetWidgetPositionById(const char* const id, const int left, const int top)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return StreamElementsWidgetManager::SetWidgetPositionById(id, left, top);
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

			return true;
		}
	}

	return false;
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

	result->m_url = m_browserWidgets[id]->GetStartUrl();

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

			docksMap[area][start].push_back(id.ToString());
			secondaryStartMap[id.ToString()] = secondary;
		}
	}

	// 2. For each area
	//
	for (docks_map_t::iterator areaPair = docksMap.begin(); areaPair != docksMap.end(); ++areaPair) {
		std::string area = areaPair->first;

		for (start_to_ids_map_t::iterator startPair = areaPair->second.begin(); startPair != areaPair->second.end(); ++startPair) {
			//int start = startPair->first;
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

bool StreamElementsBrowserWidgetManager::HasNotificationBar()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return !!m_notificationBarToolBar;
}

void StreamElementsBrowserWidgetManager::SerializeNotificationBar(CefRefPtr<CefValue>& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_notificationBarToolBar) {
		CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
		output->SetDictionary(rootDictionary);

		rootDictionary->SetString("url", m_notificationBarBrowserWidget->GetStartUrl());
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
