#include "wcui-browser-dialog.hpp"
#include "ui_wcui-browser-dialog.h"
#include "browser-manager.hpp"
#include "browser-obs-bridge-base.hpp"

#include "include/cef_parser.h"		// CefParseJSON, CefWriteJSON

#include "fmt/format.h"

#include <QWidget>
#include <QFrame>

WCUIBrowserDialog::WCUIBrowserDialog(QWidget* parent, std::string obs_module_path, std::string cache_path) :
	QDialog(parent, Qt::Dialog),
	ui(new Ui::WCUIBrowserDialog),
	m_obs_module_path(obs_module_path),
	m_cache_path(cache_path)
{
	setAttribute(Qt::WA_NativeWindow);

	ui->setupUi(this);

	// Remove help question mark from title bar
	setWindowFlags(windowFlags() &= ~Qt::WindowContextHelpButtonHint);
}

WCUIBrowserDialog::~WCUIBrowserDialog()
{
	delete ui;
}

void WCUIBrowserDialog::ShowModal()
{
	// Get window handle of CEF container frame
	auto frame = findChild<QFrame*>("frame");
	m_window_handle = (cef_window_handle_t)frame->winId();

	// Spawn CEF initialization in new thread.
	//
	// The window handle must be obtained in the QT UI thread and CEF initialization must be performed in a
	// separate thread, otherwise a dead lock occurs and everything just hangs.
	//
	pthread_t thread;
	pthread_create(&thread, nullptr, InitBrowserThreadEntryPoint, this);

	// Start modal dialog
	exec();
}

// CEF initialization thread entry point.
//
void* WCUIBrowserDialog::InitBrowserThreadEntryPoint(void* arg)
{
	WCUIBrowserDialog* self = (WCUIBrowserDialog*)arg;

	self->InitBrowser();

	return nullptr;
}

// Initialize CEF.
//
// This function is called in a separate thread by InitBrowserThreadEntryPoint() above.
//
// DO NOT call this function from the QT UI thread: it will lead to a dead lock.
//
void WCUIBrowserDialog::InitBrowser()
{
	std::string absoluteHtmlFilePath;

	// Get initial UI HTML file full path
	std::string parentPath(
		m_obs_module_path.substr(0, m_obs_module_path.find_last_of('/') + 1));

	// Launcher local HTML page path
	std::string htmlPartialPath = parentPath + "/obs-browser-wcui-browser-dialog.html";

#ifdef WIN32
	char htmlFullPath[MAX_PATH + 1];
	::GetFullPathNameA(htmlPartialPath.c_str(), MAX_PATH, htmlFullPath, NULL);

	absoluteHtmlFilePath = htmlFullPath;
#else
	char* htmlFullPath = realpath(htmlPartialPath.c_str(), NULL);

	absoluteHtmlFilePath = htmlFullPath;

	free(htmlFullPath);
#endif

	// BrowserManager installs a custom http scheme URL handler to access local files.
	//
	// We don't need this on Windows, perhaps MacOS / UNIX users need this?
	//
	CefString url = "http://absolute/" + absoluteHtmlFilePath;

	if (m_browser_handle == BROWSER_HANDLE_NONE)
	{
		// Browser has not been created yet

		// Client area rectangle
		RECT clientRect;

		clientRect.left = 0;
		clientRect.top = 0;
		clientRect.right = width();
		clientRect.bottom = height();

		// CefClient
		CefRefPtr<BrowserClient> client(new BrowserClient(NULL, NULL, new BrowserOBSBridgeBase(), this));
		
		// Window info
		CefWindowInfo window_info;

		CefBrowserSettings settings;

		settings.Reset();

		// Don't allow JavaScript to close the browser window
		settings.javascript_close_windows = STATE_DISABLED;

		window_info.SetAsChild(m_window_handle, clientRect);

		m_browser_handle = BrowserManager::Instance()->CreateBrowser(window_info, client, url, settings, nullptr);
	}
	else
	{
		// Reset URL for browser which has already been created

		BrowserManager::Instance()->LoadURL(m_browser_handle, url);
	}
}

void WCUIBrowserDialog::OnProcessMessageReceivedSendExecuteCallbackMessage(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message,
	CefRefPtr<CefValue> callback_arg)
{
	int callbackID = message->GetArgumentList()->GetInt(0);

	CefString jsonString =
		CefWriteJSON(callback_arg, JSON_WRITER_DEFAULT);

	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("executeCallback");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetInt(0, callbackID);	// Callback identifier in renderer process callbackMap

	args->SetString(1, jsonString);	// Callback argument JSON string which will be converted back to CefV8Value in
					// the renderer process
	
	browser->SendProcessMessage(source_process, msg); // source_process = PID_RENDERER
}

// Called when a new message is received from a different process. Return true
// if the message was handled or false otherwise. Do not keep a reference to
// or attempt to access the message outside of this callback.
//
/*--cef()--*/
bool WCUIBrowserDialog::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	CefString name = message->GetName();
	CefRefPtr<CefListValue> args = message->GetArgumentList();

	int argsLength = args->GetSize();

	if (name == "setupEnvironment")
	{
		CefString config_json_string = args->GetValue(0)->GetString();
		CefRefPtr<CefValue> config =
			CefParseJSON(config_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS);

		if (config->GetDictionary() != NULL)
		{
			auto input = config->GetDictionary()->GetValue("input");
			auto output = config->GetDictionary()->GetValue("output");

		}

		return true;
	}
	else if (name == "videoCodecs")
	{
		CefRefPtr<CefListValue> codec_list = CefListValue::Create();

		CefRefPtr<CefValue> root = CefValue::Create();

		root->SetList(codec_list);

		// POC callback argument array content
		for (int i = 0; i < 3; ++i) {
			CefRefPtr<CefDictionaryValue> codec = CefDictionaryValue::Create();

			std::string id = fmt::format("codec {}", i);
			std::string codec_name = fmt::format("Video Codec {}", i);
			bool isHardwareAccelerated = false;
			
			codec->SetString("id", id);
			codec->SetString("name", name);
			codec->SetBool("isHardwareAccelerated", isHardwareAccelerated);

			codec_list->SetDictionary(i, codec);
		}

		OnProcessMessageReceivedSendExecuteCallbackMessage(browser, source_process, message, root);

		return true;
	}
	else if (name == "audioCodecs")
	{

	}
	else if (name == "videoCaptureDevices")
	{

	}

	return false;
}
