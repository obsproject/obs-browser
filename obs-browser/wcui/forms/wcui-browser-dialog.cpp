#include "wcui-browser-dialog.hpp"
#include "ui_wcui-browser-dialog.h"
#include "browser-manager.hpp"

#include <QGuiApplication>

WCUIBrowserDialog::WCUIBrowserDialog(QWidget* parent, std::string obs_module_path, std::string cache_path) :
	QDialog(parent, Qt::Dialog),
	ui(new Ui::WCUIBrowserDialog),
	m_obs_module_path(obs_module_path),
	m_cache_path(cache_path)
{
	pthread_mutex_init(&m_dispatchLock, nullptr);
	os_event_init(&m_dispatchEvent, OS_EVENT_TYPE_AUTO);
	os_event_init(&m_startupEvent, OS_EVENT_TYPE_MANUAL);

	ui->setupUi(this);
}

WCUIBrowserDialog::~WCUIBrowserDialog()
{
	delete ui;
	ui = NULL;

	os_event_destroy(m_dispatchEvent);
	m_dispatchEvent = NULL;

	os_event_destroy(m_startupEvent);
	m_startupEvent = NULL;

	pthread_mutex_destroy(&m_dispatchLock);
	m_dispatchLock = NULL;
}

void WCUIBrowserDialog::ToggleShowHide()
{
	if (!isVisible())
	{
		setVisible(true);

		InitBrowser();
	}
	else
	{
		setVisible(false);
	}
}

std::string WCUIBrowserDialog::GetCefBootstrapProcessPath()
{
	std::string parentPath(
		m_obs_module_path.substr(0,
		m_obs_module_path.find_last_of('/') + 1));

#ifdef _WIN32	
	return parentPath + "/cef-bootstrap.exe";
#else
	return parentPath + "/cef-bootstrap";
#endif
}

// Initialize CEF
void WCUIBrowserDialog::InitBrowser()
{
	CefWindowInfo window_info;
	CefRefPtr<CefClient> client;
	CefString url = "http://www.google.com";
	CefBrowserSettings settings;

	RECT clientRect;
	clientRect.left = 0;
	clientRect.top = 0;
	clientRect.right = 400;
	clientRect.bottom = 300;

	//window_info.SetAsChild(parentWindowHandle, clientRect);
	window_info.SetAsPopup(NULL, "Test");

	// Explore this to set as child:
	// http://magpcss.org/ceforum/viewtopic.php?f=6&t=13938&start=20

	// handle to be destroyed with DestroyBrowser() later
	int handle = BrowserManager::Instance()->CreateBrowser(window_info, client, url, settings, nullptr);

}
