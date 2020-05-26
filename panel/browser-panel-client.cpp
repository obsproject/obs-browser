#include "browser-panel-client.hpp"
#include <util/dstr.h>

#include <QUrl>
#include <QDesktopServices>

#include <obs-module.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* CefClient */
CefRefPtr<CefLoadHandler> QCefBrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> QCefBrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefRequestHandler> QCefBrowserClient::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> QCefBrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefKeyboardHandler> QCefBrowserClient::GetKeyboardHandler()
{
	return this;
}

/* CefDisplayHandler */
void QCefBrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
				      const CefString &title)
{
	if (widget && widget->cefBrowser->IsSame(browser)) {
		std::string str_title = title;
		QString qt_title = QString::fromUtf8(str_title.c_str());
		QMetaObject::invokeMethod(widget, "titleChanged",
					  Q_ARG(QString, qt_title));
	} else { /* handle popup title */
#ifdef _WIN32
		std::wstring str_title = title;
		HWND hwnd = browser->GetHost()->GetWindowHandle();
		SetWindowTextW(hwnd, str_title.c_str());
#endif
	}
}

/* CefRequestHandler */
bool QCefBrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				       CefRefPtr<CefFrame>,
				       CefRefPtr<CefRequest> request, bool,
				       bool)
{
	std::string str_url = request->GetURL();

	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	for (size_t i = forced_popups.size(); i > 0; i--) {
		PopupWhitelistInfo &info = forced_popups[i - 1];

		if (!info.obj) {
			forced_popups.erase(forced_popups.begin() + (i - 1));
			continue;
		}

		if (astrcmpi(info.url.c_str(), str_url.c_str()) == 0) {
			/* Open tab popup URLs in user's actual browser */
			QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
			QDesktopServices::openUrl(url);
			browser->GoBack();
			return true;
		}
	}

	if (widget) {
		QString qt_url = QString::fromUtf8(str_url.c_str());
		QMetaObject::invokeMethod(widget, "urlChanged",
					  Q_ARG(QString, qt_url));
	}
	return false;
}

bool QCefBrowserClient::OnOpenURLFromTab(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	CefRequestHandler::WindowOpenDisposition, bool)
{
	std::string str_url = target_url;

	/* Open tab popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

void QCefBrowserClient::OnLoadError(CefRefPtr<CefBrowser> browser,
				    CefRefPtr<CefFrame> frame,
				    CefLoadHandler::ErrorCode errorCode,
				    const CefString &errorText,
				    const CefString &failedUrl)
{
	if (errorCode == ERR_ABORTED)
		return;

	struct dstr html;
	char *path = obs_module_file("error.html");
	char *errorPage = os_quick_read_utf8_file(path);

	dstr_init_copy(&html, errorPage);

	dstr_replace(&html, "%%ERROR_URL%%", failedUrl.ToString().c_str());

	dstr_replace(&html, "Error.Title", obs_module_text("Error.Title"));
	dstr_replace(&html, "Error.Description",
		     obs_module_text("Error.Description"));
	dstr_replace(&html, "Error.Retry", obs_module_text("Error.Retry"));
	const char *translError;
	std::string errorKey = "ErrorCode." + errorText.ToString();
	if (obs_module_get_string(errorKey.c_str(),
				  (const char **)&translError)) {
		dstr_replace(&html, "%%ERROR_CODE%%", translError);
	} else {
		dstr_replace(&html, "%%ERROR_CODE%%",
			     errorText.ToString().c_str());
	}

	frame->LoadURL(
		"data:text/html;base64," +
		CefURIEncode(CefBase64Encode(html.array, html.len), false)
			.ToString());

	dstr_free(&html);
	bfree(path);
	bfree(errorPage);
}

/* CefLifeSpanHandler */
bool QCefBrowserClient::OnBeforePopup(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	const CefString &, CefLifeSpanHandler::WindowOpenDisposition, bool,
	const CefPopupFeatures &, CefWindowInfo &windowInfo,
	CefRefPtr<CefClient> &, CefBrowserSettings &,
#if CHROME_VERSION_BUILD >= 3770
	CefRefPtr<CefDictionaryValue> &,
#endif
	bool *)
{
	if (allowAllPopups) {
#ifdef _WIN32
		HWND hwnd = (HWND)widget->effectiveWinId();
		windowInfo.parent_window = hwnd;
#endif
		return false;
	}

	std::string str_url = target_url;

	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	for (size_t i = popup_whitelist.size(); i > 0; i--) {
		PopupWhitelistInfo &info = popup_whitelist[i - 1];

		if (!info.obj) {
			popup_whitelist.erase(popup_whitelist.begin() +
					      (i - 1));
			continue;
		}

		if (astrcmpi(info.url.c_str(), str_url.c_str()) == 0) {
#ifdef _WIN32
			HWND hwnd = (HWND)widget->effectiveWinId();
			windowInfo.parent_window = hwnd;
#endif
			return false;
		}
	}

	/* Open popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

void QCefBrowserClient::OnLoadEnd(CefRefPtr<CefBrowser>,
				  CefRefPtr<CefFrame> frame, int)
{
	if (frame->IsMain() && !script.empty())
		frame->ExecuteJavaScript(script, CefString(), 0);
}

bool QCefBrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				      const CefKeyEvent &event, CefEventHandle,
				      bool *)
{
#ifdef _WIN32
	if (event.type != KEYEVENT_RAWKEYDOWN)
		return false;

	if (event.windows_key_code == 'R' &&
	    (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
		browser->ReloadIgnoreCache();
		return true;
	}
#endif
	return false;
}
