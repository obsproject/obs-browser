#include "StreamElementsWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <cassert>
#include <mutex>

#include <QApplication>

StreamElementsWidgetManager::StreamElementsWidgetManager(QMainWindow* parent) :
	m_parent(parent),
	m_nativeCentralWidget(nullptr)
{
	assert(parent);
}

StreamElementsWidgetManager::~StreamElementsWidgetManager()
{
}

//
// The QApplication::sendPostedEvents() and setMinimumSize() black
// magic below is a very unpleasant hack to ensure that the central
// widget remains the same size as the previously visible central
// widget.
//
// This is done by first measuring the size() of the current
// central widget, then removing the current central widget and
// replacing it with a new one, setting the new widget MINIMUM
// size to the previously visible central widget width & height
// and draining the Qt message queue with a call to
// QApplication::sendPostedEvents().
// Then we reset the new central widget minimum size to 0x0.
//
void StreamElementsWidgetManager::PushCentralWidget(QWidget* widget)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QApplication::sendPostedEvents();
	QSize prevSize = mainWindow()->centralWidget()->size();

	widget->setMinimumSize(prevSize);

	m_nativeCentralWidget = m_parent->takeCentralWidget();

	m_parent->setCentralWidget(widget);

	// Drain event queue
	QApplication::sendPostedEvents();

	widget->setMinimumSize(0, 0);
}

bool StreamElementsWidgetManager::DestroyCurrentCentralWidget()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!!m_nativeCentralWidget) {
		SaveDockWidgetsGeometry();

		QApplication::sendPostedEvents();
		QSize currSize = mainWindow()->centralWidget()->size();

		m_parent->setCentralWidget(m_nativeCentralWidget);

		m_nativeCentralWidget = nullptr;

		mainWindow()->centralWidget()->setMinimumSize(currSize);

		// Drain event queue
		QApplication::sendPostedEvents();

		mainWindow()->centralWidget()->setMinimumSize(0, 0);

		RestoreDockWidgetsGeometry();
	}

	return nullptr;
}

bool StreamElementsWidgetManager::HasCentralWidget()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return !!m_nativeCentralWidget;
}

void StreamElementsWidgetManager::OnObsExit()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Empty stack
	while (DestroyCurrentCentralWidget()) {
	}
}

bool StreamElementsWidgetManager::AddDockWidget(
	const char* const id,
	const char* const title,
	QWidget* const widget,
	const Qt::DockWidgetArea area,
	const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	assert(id);
	assert(title);
	assert(widget);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = new QDockWidget(title, m_parent);

	dock->setObjectName(QString(id));

	dock->setAllowedAreas(allowedAreas);
	dock->setFeatures(features);
	dock->setWidget(widget);

	m_parent->addDockWidget(area, dock);

	m_dockWidgets[id] = dock;
	m_dockWidgetAreas[id] = area;

	if (area == Qt::NoDockWidgetArea) {
		dock->setFloating(false);
		QApplication::sendPostedEvents();
		dock->setFloating(true);
		QApplication::sendPostedEvents();
	}

	std::string savedId = id;

	QObject::connect(dock, &QDockWidget::dockLocationChanged, [savedId, dock, this](Qt::DockWidgetArea area) {
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		if (!m_dockWidgets.count(savedId)) {
			return;
		}

		m_dockWidgetAreas[savedId] = area;

		QtPostTask([](void*) -> void {
			StreamElementsGlobalStateManager::GetInstance()->PersistState();
		}, nullptr);
	});

	QObject::connect(dock, &QDockWidget::visibilityChanged, [this]() {
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		QtPostTask([](void*) -> void {
			StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
			StreamElementsGlobalStateManager::GetInstance()->PersistState();
		}, nullptr);
	});

	return true;
}

bool StreamElementsWidgetManager::ToggleWidgetFloatingStateById(const char* const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = m_dockWidgets[id];

	dock->setFloating(!dock->isFloating());

	return true;
}

bool StreamElementsWidgetManager::SetWidgetDimensionsById(const char* const id, const int width, const int height)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = m_dockWidgets[id];

	if (!dock->isFloating() || !dock->window()) {
		return false;
	}

	QApplication::sendPostedEvents();

	QSize prevMin = dock->window()->minimumSize();
	QSize prevMax = dock->window()->maximumSize();

	if (width >= 0) {
		dock->window()->setMinimumWidth(width);
		dock->window()->setMaximumWidth(width);
	}

	if (height >= 0) {
		dock->window()->setMinimumHeight(height);
		dock->window()->setMaximumHeight(height);
	}

	QApplication::sendPostedEvents();

	dock->window()->setMinimumSize(prevMin);
	dock->window()->setMaximumSize(prevMax);

	return true;
}

bool StreamElementsWidgetManager::SetWidgetPositionById(const char* const id, const int left, const int top)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = m_dockWidgets[id];

	if (!dock->isFloating() || !dock->window()) {
		return false;
	}

	QPoint pos = dock->window()->pos();

	if (left >= 0) {
		pos.setX(left);
	}

	if (top >= 0) {
		pos.setY(top);
	}

	dock->window()->move(pos);

	return true;
}

bool StreamElementsWidgetManager::RemoveDockWidget(const char* const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return false;
	}

	QDockWidget* dock = m_dockWidgets[id];

	m_dockWidgets.erase(id);
	m_dockWidgetAreas.erase(id);

	m_parent->removeDockWidget(dock);

	dock->deleteLater();

	return true;
}

void StreamElementsWidgetManager::GetDockWidgetIdentifiers(std::vector<std::string>& result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	for (auto imap : m_dockWidgets) {
		result.push_back(imap.first);
	}
}

QDockWidget* StreamElementsWidgetManager::GetDockWidget(const char* const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_dockWidgets.count(id)) {
		return nullptr;
	}

	return m_dockWidgets[id];
}

StreamElementsWidgetManager::DockWidgetInfo* StreamElementsWidgetManager::GetDockWidgetInfo(const char* const id)
{
	assert(id);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QDockWidget* dockWidget = GetDockWidget(id);

	if (!dockWidget) {
		return nullptr;
	}

	StreamElementsWidgetManager::DockWidgetInfo* result = new StreamElementsWidgetManager::DockWidgetInfo();

	result->m_widget = dockWidget->widget();

	result->m_id = id;
	result->m_title = dockWidget->windowTitle().toStdString();
	result->m_visible = dockWidget->isVisible();

	if (dockWidget->isFloating()) {
		result->m_dockingArea = "floating";
	} else {
		switch (m_dockWidgetAreas[id])
		{
		case Qt::LeftDockWidgetArea:
			result->m_dockingArea = "left";
			break;
		case Qt::RightDockWidgetArea:
			result->m_dockingArea = "right";
			break;
		case Qt::TopDockWidgetArea:
			result->m_dockingArea = "top";
			break;
		case Qt::BottomDockWidgetArea:
			result->m_dockingArea = "bottom";
			break;
		//case Qt::NoDockWidgetArea:
		default:
			result->m_dockingArea = "floating";
			break;
		}
	}

	return result;
}

void StreamElementsWidgetManager::SerializeDockingWidgets(std::string& output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefValue> root = CefValue::Create();

	SerializeDockingWidgets(root);

	// Convert data to JSON
	CefString jsonString =
		CefWriteJSON(root, JSON_WRITER_DEFAULT);

	output = jsonString.ToString();
}

void StreamElementsWidgetManager::DeserializeDockingWidgets(std::string& input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Convert JSON string to CefValue
	CefRefPtr<CefValue> root =
		CefParseJSON(CefString(input), JSON_PARSER_ALLOW_TRAILING_COMMAS);

	DeserializeDockingWidgets(root);
}

void StreamElementsWidgetManager::SaveDockWidgetsGeometry()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	m_dockWidgetSavedMinSize.clear();

	QApplication::sendPostedEvents();

	for (auto iter : m_dockWidgets) {
		m_dockWidgetSavedMinSize[iter.first] = iter.second->size();
	}
}

void StreamElementsWidgetManager::RestoreDockWidgetsGeometry()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QApplication::sendPostedEvents();

	std::map<std::string, QSize> maxSize;

	for (auto iter : m_dockWidgetSavedMinSize) {
		if (m_dockWidgets.count(iter.first)) {
			maxSize[iter.first] = m_dockWidgets[iter.first]->maximumSize();

			m_dockWidgets[iter.first]->setMinimumSize(iter.second);
			m_dockWidgets[iter.first]->setMaximumSize(iter.second);
		}
	}

	QApplication::sendPostedEvents();

	for (auto iter : m_dockWidgetSavedMinSize) {
		if (m_dockWidgets.count(iter.first)) {
			m_dockWidgets[iter.first]->setMinimumSize(QSize(0, 0));
			m_dockWidgets[iter.first]->setMaximumSize(maxSize[iter.first]);
		}
	}

	QApplication::sendPostedEvents();
}
