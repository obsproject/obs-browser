#include "StreamElementsMenuManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "../cef-headers.hpp"

#include <QObjectList>
#include <QDesktopServices>
#include <QUrl>

#include <algorithm>

StreamElementsMenuManager::StreamElementsMenuManager(QMainWindow* parent):
	m_mainWindow(parent)
{
	m_menu = new QMenu("St&reamElements");

	mainWindow()->menuBar()->addMenu(m_menu);
}

StreamElementsMenuManager::~StreamElementsMenuManager()
{
	m_menu->menuAction()->setVisible(false);
	m_menu = nullptr;
}

void StreamElementsMenuManager::Update()
{
	SYNC_ACCESS();

	if (!m_menu) return;

	m_menu->clear();

	auto addURL = [this](QString title, QString url) {
		QAction* menu_action = new QAction(title);
		m_menu->addAction(menu_action);

		menu_action->connect(
			menu_action,
			&QAction::triggered,
			[this, url]
			{
				QUrl navigate_url = QUrl(url, QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});
	};

	QAction* logout_action = new QAction(obs_module_text("StreamElements.Action.ResetState"));
	m_menu->addAction(logout_action);
	logout_action->connect(
		logout_action,
		&QAction::triggered,
		[this]
		{
			StreamElementsGlobalStateManager::GetInstance()->Reset();
		});


	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Action.Bot"), obs_module_text("StreamElements.Action.Bot.URL"));
	addURL(obs_module_text("StreamElements.Action.Tipping"), obs_module_text("StreamElements.Action.Tipping.URL"));
	addURL(obs_module_text("StreamElements.Action.Overlays"), obs_module_text("StreamElements.Action.Overlays.URL"));
	addURL(obs_module_text("StreamElements.Action.GroundControl"), obs_module_text("StreamElements.Action.GroundControl.URL"));
	m_menu->addSeparator();
	{
		std::vector<std::string> widgetIds;
		StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->GetDockBrowserWidgetIdentifiers(widgetIds);

		std::vector<StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo*> widgets;
		for (auto id : widgetIds) {
			widgets.emplace_back(
				StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->GetDockBrowserWidgetInfo(
					id.c_str()));
		}

		std::sort(
			widgets.begin(),
			widgets.end(),
			[](StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* a, StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo* b)
		{
			return a->m_title < b->m_title;
		});

		for (auto widget : widgets) {
			// widget->m_visible
			QAction* widget_action = new QAction(QString(widget->m_title.c_str()));
			m_menu->addAction(widget_action);

			std::string id = widget->m_id;
			bool isVisible = widget->m_visible;

			widget_action->setCheckable(true);
			widget_action->setChecked(isVisible);

			QObject::connect(
				widget_action,
				&QAction::triggered,
				[this, id, isVisible, widget_action]
			{
				QDockWidget* dock =
					StreamElementsGlobalStateManager::GetInstance()->GetWidgetManager()->GetDockWidget(id.c_str());

				if (dock) {
					dock->setVisible(!isVisible);

					Update();
				}
			});
		}

		for (auto widget : widgets) {
			delete widget;
		}
	}
	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Action.Import"), obs_module_text("StreamElements.Action.Import.URL"));

	/*
	QAction* start_onboarding_ui_action = new QAction(obs_module_text("StreamElements.Action.StartOnBoardingUI"));
	m_menu->addAction(start_onboarding_ui_action);
	start_onboarding_ui_action->connect(
		start_onboarding_ui_action,
		&QAction::triggered,
		[this]
	{
		StreamElementsGlobalStateManager::GetInstance()->StartOnBoardingUI();
	});
	*/

	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Action.Uninstall"), obs_module_text("StreamElements.Action.Uninstall.URL"));
	m_menu->addSeparator();
	addURL(obs_module_text("StreamElements.Action.LiveSupport"), obs_module_text("StreamElements.Action.LiveSupport.URL"));
}
