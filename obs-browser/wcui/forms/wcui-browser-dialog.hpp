#ifndef WCUI_BROWSER_DIALOG_H
#define WCUI_BROWSER_DIALOG_H

#include <obs-frontend-api.h>

#include <QDialog>
#include <QWidget>

#include <util/platform.h>
#include <util/threading.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>

#include <pthread.h>

#include <functional>

namespace Ui {
	class WCUIBrowserDialog;
}

class WCUIBrowserDialog : public QDialog
{
	Q_OBJECT

public:
	explicit WCUIBrowserDialog(
		QWidget* parent,
		std::string obs_module_path,
		std::string cache_path);

	~WCUIBrowserDialog();
	void ToggleShowHide();

	/*
	private Q_SLOTS:
	void AuthCheckboxChanged();
	void FormAccepted();
	*/

private:
	std::string GetCefBootstrapProcessPath();

	void InitBrowser();

private:
	void PushEvent(std::function<void()> event);
	static void* BrowserManagerThreadEntry(void* threadArgument);
	void BrowserManagerEntry();

private:
	std::string m_obs_module_path;
	std::string m_cache_path;

	bool m_threadAlive;
	os_event_t *m_dispatchEvent;
	os_event_t *m_startupEvent;
	pthread_t m_managerThread;
	pthread_mutex_t m_dispatchLock;
	std::vector<std::function<void()>> m_queue;

	Ui::WCUIBrowserDialog* ui;
};

#endif // WCUI_BROWSER_DIALOG_H
