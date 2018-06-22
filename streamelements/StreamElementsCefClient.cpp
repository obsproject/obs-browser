#include "StreamElementsCefClient.hpp"
#include "StreamElementsUtils.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include <obs-frontend-api.h>
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON
#include <regex>
#include <sstream>

#include <QWindow>
#include <QIcon>

using namespace json11;

/* ========================================================================= */

#define CEF_REQUIRE_UI_THREAD()       DCHECK(CefCurrentlyOn(TID_UI));
#define CEF_REQUIRE_IO_THREAD()       DCHECK(CefCurrentlyOn(TID_IO));
#define CEF_REQUIRE_FILE_THREAD()     DCHECK(CefCurrentlyOn(TID_FILE));
#define CEF_REQUIRE_RENDERER_THREAD() DCHECK(CefCurrentlyOn(TID_RENDERER));

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

#include <QFile>
#include <QTextStream>
#include <QString>

static std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
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
	cef_window_handle_t handle = browser->GetHost()->GetWindowHandle();

#ifdef WIN32
	::SetWindowTextW(handle, title.ToWString().c_str());
#endif
}

void StreamElementsCefClient::OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
	const std::vector<CefString>& icon_urls)
{
	// TODO: Implement
}
