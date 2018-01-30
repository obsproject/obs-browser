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

// TODO: Remove
#include "shared/browser-client.hpp"

namespace Ui {
	class WCUIBrowserDialog;
}

class WCUIBrowserDialog:
	public QDialog,
	public CefClient
{
	Q_OBJECT

private:
	const int BROWSER_HANDLE_NONE = -1;

public:
	explicit WCUIBrowserDialog(
		QWidget* parent,
		std::string obs_module_path,
		std::string cache_path);

	~WCUIBrowserDialog();

public:
	void ShowModal();

private:
	static void* InitBrowserThreadEntryPoint(void* arg);
	void InitBrowser();

private:
	std::string m_obs_module_path;
	std::string m_cache_path;

	Ui::WCUIBrowserDialog* ui;

	cef_window_handle_t m_window_handle;
	int m_browser_handle = BROWSER_HANDLE_NONE;

public: // CefClient implementation

	// Called when a new message is received from a different process. Return true
	// if the message was handled or false otherwise. Do not keep a reference to
	// or attempt to access the message outside of this callback.
	///
	/*--cef()--*/
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message);

private:

	// To be called ONLY by OnProcessMessageReceived
	//
	void OnProcessMessageReceivedSendExecuteCallbackMessage(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefValue> callback_arg);

	// To be called ONLY by OnProcessMessageReceived
	//
	void OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message,
		obs_encoder_type encoder_type);

public:
	IMPLEMENT_REFCOUNTING(WCUIBrowserDialog);
};

#endif // WCUI_BROWSER_DIALOG_H
