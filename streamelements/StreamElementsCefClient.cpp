#include "StreamElementsCefClient.hpp"
#include "StreamElementsUtils.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include <obs-frontend-api.h>
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON
#include <include/cef_urlrequest.h>	// CefURLRequestClient
#include <regex>
#include <sstream>

#include <QWindow>
#include <QIcon>
#include <QWidget>
#include <QFile>

static class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override { task(); }

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

static bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI, CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

/* ========================================================================= */

static bool SetWindowIconFromBuffer(cef_window_handle_t windowHandle, void* buffer, size_t buffer_len)
{
	size_t offset = ::LookupIconIdFromDirectoryEx((PBYTE)buffer, TRUE, 0, 0, LR_DEFAULTCOLOR);

	if (offset) {
		size_t size = buffer_len - offset;

		HICON hIcon = ::CreateIconFromResourceEx((PBYTE)buffer + offset, size, TRUE, 0x00030000, 0, 0, LR_SHARED);

		if (hIcon) {
			::SendMessage(windowHandle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			::SendMessage(windowHandle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

			return true;
		}
	}

	return false;
}

static bool SetWindowIconFromResource(cef_window_handle_t windowHandle, QString& resource)
{
	QFile file(resource);

	if (file.open(QIODevice::ReadOnly)) {
		QByteArray data = file.readAll();

		return SetWindowIconFromBuffer(windowHandle, data.begin(), data.size());
	}

	return false;
}

static bool SetWindowDefaultIcon(cef_window_handle_t windowHandle)
{
	return SetWindowIconFromResource(windowHandle, QString(":/images/icon.ico"));
}

/* ========================================================================= */

using namespace json11;

/* ========================================================================= */

#define CEF_REQUIRE_UI_THREAD()       DCHECK(CefCurrentlyOn(TID_UI));
#define CEF_REQUIRE_IO_THREAD()       DCHECK(CefCurrentlyOn(TID_IO));
#define CEF_REQUIRE_FILE_THREAD()     DCHECK(CefCurrentlyOn(TID_FILE));
#define CEF_REQUIRE_RENDERER_THREAD() DCHECK(CefCurrentlyOn(TID_RENDERER));

/* ========================================================================= */

StreamElementsCefClient::StreamElementsCefClient(std::string executeJavaScriptCodeOnLoad, CefRefPtr<StreamElementsBrowserMessageHandler> messageHandler) :
	m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad),
	m_messageHandler(messageHandler)
{
}

StreamElementsCefClient::~StreamElementsCefClient()
{
}

/* ========================================================================= */

void StreamElementsCefClient::OnLoadEnd(
	CefRefPtr<CefBrowser> /*browser*/,
	CefRefPtr<CefFrame> frame,
	int /*httpStatusCode*/)
{
	if (m_executeJavaScriptCodeOnLoad.empty() || !frame->IsMain()) {
		return;
	}

	frame->ExecuteJavaScript(
		CefString(m_executeJavaScriptCodeOnLoad),
		frame->GetURL(),
		0);
}

void StreamElementsCefClient::OnLoadError(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	ErrorCode errorCode,
	const CefString& errorText,
	const CefString& failedUrl)
{
	/*if (errorCode == ERR_ABORTED) {
		// Don't display an error for downloaded files.
		return;
	}*/

	std::string htmlString = LoadResourceString(":/html/error.html");

	if (!htmlString.size()) {
		// Default
		htmlString = "<html><body><h1>error page</h1><p>${error.code}</p><p>${error.url}</p></body></html>";
	}

	std::stringstream error;
	if (errorText.size()) {
		error << errorText.ToString();
	}
	else {
		error << "UNKNOWN" << " (" << (int)errorCode << ")";
	}

	htmlString = std::regex_replace(htmlString, std::regex("\\$\\{error.code\\}"), error.str());
	htmlString = std::regex_replace(htmlString, std::regex("\\$\\{error.text\\}"), error.str());
	htmlString = std::regex_replace(htmlString, std::regex("\\$\\{error.url\\}"), failedUrl.ToString());

	frame->GetBrowser()->GetMainFrame()->LoadStringW(htmlString, failedUrl);
}

/* ========================================================================= */

bool StreamElementsCefClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();
	Json json;

	if (name == "getCurrentScene") {
		json = Json::object{};
	}
	else if (name == "getStatus") {
		json = Json::object{
			{ "recording", obs_frontend_recording_active() },
			{ "streaming", obs_frontend_streaming_active() },
			{ "replaybuffer", obs_frontend_replay_buffer_active() }
		};

	}
	else if (m_messageHandler.get() && m_messageHandler->OnProcessMessageReceived(browser, source_process, message)) {
		return true;
	}
	else {

		return false;
	}

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("executeCallback");

	CefRefPtr<CefListValue> args = msg->GetArgumentList();
	args->SetInt(0, message->GetArgumentList()->GetInt(0));
	args->SetString(1, json.dump());

	browser->SendProcessMessage(PID_RENDERER, msg);

	return true;
}

void StreamElementsCefClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
	const CefString& title)
{
	struct local_context {
		cef_window_handle_t handle;
		CefString title;
	};

	local_context* context = new local_context();
	
	context->handle = browser->GetHost()->GetWindowHandle();;
	context->title = title;

	QtPostTask([](void* data) {
		local_context* context = (local_context*)data;

		QWindow* win = QWindow::fromWinId((WId)context->handle);
		win->setTitle(QString(context->title.ToString().c_str()));

		delete win;

		delete context;
	},
	context);
}

void StreamElementsCefClient::OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
	const std::vector<CefString>& icon_urls)
{
}

void StreamElementsCefClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	SetWindowDefaultIcon(
		browser->GetHost()->GetWindowHandle());
}
