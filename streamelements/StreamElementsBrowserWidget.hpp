#pragma once

#include <QWidget>
#include <QHideEvent>

#include <util/platform.h>
#include <util/threading.h>
#include <include/cef_base.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>
#include <include/base/cef_bind.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/base/cef_lock.h>

#include <pthread.h>
#include <functional>
#include <mutex>

#include "../browser-client.hpp"

#include "StreamElementsAsyncTaskQueue.hpp"
#include "StreamElementsCefClient.hpp"

#include <QtWidgets>

class StreamElementsBrowserWidget:
	public QWidget

{
	Q_OBJECT

private:
	std::string m_url;
	std::string m_executeJavaScriptCodeOnLoad;
	std::string m_pendingLocationArea;
	std::string m_pendingId;

	QSize m_sizeHint;

public:
	StreamElementsBrowserWidget(
		QWidget* parent,
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad,
		const char* const locationArea,
		const char* const id);
	~StreamElementsBrowserWidget();

	void setSizeHint(QSize& size)
	{
		m_sizeHint = size;
	}

	void setSizeHint(const int w, const int h)
	{
		m_sizeHint = QSize(w, h);
	}

public:
	std::string GetCurrentUrl();
	std::string GetExecuteJavaScriptCodeOnLoad();

private:
	///
	// Browser initialization
	//
	// Create browser or navigate back to home page (obs-browser-wcui-browser-dialog.html)
	//
	void InitBrowserAsync();
	void CefUIThreadExecute(std::function<void()> func, bool async);

private slots:
	void InitBrowserAsyncInternal();

private:
	StreamElementsAsyncTaskQueue m_task_queue;
	cef_window_handle_t m_window_handle;
	CefRefPtr<CefBrowser> m_cef_browser;

protected:
	virtual void showEvent(QShowEvent* showEvent) override
	{
		QWidget::showEvent(showEvent);

		InitBrowserAsync();

		ShowBrowser();
	}

	virtual void hideEvent(QHideEvent *hideEvent) override
	{
		QWidget::hideEvent(hideEvent);

		HideBrowser();
	}

	virtual void resizeEvent(QResizeEvent* event) override
	{
		QWidget::resizeEvent(event);

		UpdateBrowserSize();
	}

	virtual void changeEvent(QEvent* event) override
	{
		QWidget::changeEvent(event);

		if (event->type() == QEvent::ParentChange) {
			if (!parent()) {
				HideBrowser();
			}
		}
	}

private:
	void UpdateBrowserSize()
	{
		if (!!m_cef_browser.get()) {
			cef_window_handle_t hWnd = m_cef_browser->GetHost()->GetWindowHandle();

#ifdef WIN32
			::SetWindowPos(hWnd, HWND_TOP, 0, 0, width(), height(), SWP_DRAWFRAME | SWP_SHOWWINDOW);
#endif
		}
	}

	void HideBrowser()
	{
		if (m_cef_browser.get() != NULL) {
			::ShowWindow(
				m_cef_browser->GetHost()->GetWindowHandle(),
				SW_HIDE);
		}
	}

	void ShowBrowser()
	{
		if (m_cef_browser.get() != NULL) {
			::ShowWindow(
				m_cef_browser->GetHost()->GetWindowHandle(),
				SW_SHOW);
		}
	}

	void DestroyBrowser()
	{
		std::lock_guard<std::mutex> guard(m_create_destroy_mutex);

		if (m_cef_browser.get() != NULL) {
			HideBrowser();

			// Detach browser to prevent WM_CLOSE event from being sent
			// from CEF to the parent window.
			::SetParent(
				m_cef_browser->GetHost()->GetWindowHandle(),
				0L);

			m_cef_browser->GetHost()->CloseBrowser(true);
			m_cef_browser = NULL;
		}
	}




private:
	std::mutex m_create_destroy_mutex;

public:
//	IMPLEMENT_REFCOUNTING(StreamElementsBrowserWidget)
};

