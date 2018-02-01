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

#include "shared/browser-client.hpp"

#include "../wcui-async-task-queue.hpp"

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
	void InitBrowser();

private:
	std::string m_obs_module_path;
	std::string m_cache_path;

	Ui::WCUIBrowserDialog* ui;

	cef_window_handle_t m_window_handle;
	int m_browser_handle = BROWSER_HANDLE_NONE;

	WCUIAsyncTaskQueue m_task_queue;

private slots: // OBS operations

	size_t ObsScenesGetCount();
	void ObsRemoveFirstScenes(size_t removeCount = 1);

	void ObsAddScene(
		const char* name,
		bool setCurrent = true);

	void ObsAddSource(
		obs_source_t* parentScene,
		const char* sourceId,
		const char* sourceName,
		obs_data_t* sourceSettings = NULL,
		obs_data_t* sourceHotkeyData = NULL,
		bool preferExistingSource = false,
		obs_source_t** output_source = NULL,
		obs_sceneitem_t** output_sceneitem = NULL);

	void ObsAddSourceBrowser(
		obs_source_t* parentScene,
		const char* name,
		const long long width,
		const long long height,
		const long long fps,
		const char* url,
		const bool shutdownWhenInactive = true,
		const char* css = "");

	void ObsAddSourceVideoCapture(
		obs_source_t* parentScene,
		const char* name,
		const int x,
		const int y,
		const int maxWidth,
		const int maxHeight);

	void ObsAddSourceGame(
		obs_source_t* parentScene,
		const char* name,
		bool multiGpuCompatibility = true,
		bool allowTransparency = false,
		bool limitFramerate = false,
		bool captureCursor = false,
		bool antiCheatHook = true,
		bool captureOverlays = true);

	void ObsSetProfileOutputConfiguration();

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
