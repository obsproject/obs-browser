#include "browser-panel-client.hpp"
#include <util/dstr.h>

#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QMenu>
#include <QThread>

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

CefRefPtr<CefContextMenuHandler> QCefBrowserClient::GetContextMenuHandler()
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
	UNUSED_PARAMETER(browser);
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
#else
		UNUSED_PARAMETER(windowInfo);
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

void QCefBrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser>,
					    CefRefPtr<CefFrame>,
					    CefRefPtr<CefContextMenuParams>,
					    CefRefPtr<CefMenuModel> model)
{
	if (model->IsVisible(MENU_ID_BACK) &&
	    (!model->IsVisible(MENU_ID_RELOAD) &&
	     !model->IsVisible(MENU_ID_RELOAD_NOCACHE))) {
		model->InsertItemAt(
			2, MENU_ID_RELOAD_NOCACHE,
			QObject::tr("RefreshBrowser").toUtf8().constData());
	}
	if (model->IsVisible(MENU_ID_PRINT)) {
		model->Remove(MENU_ID_PRINT);
	}
}

#if defined(_WIN32)
bool QCefBrowserClient::RunContextMenu(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
	CefRefPtr<CefContextMenuParams>, CefRefPtr<CefMenuModel> model,
	CefRefPtr<CefRunContextMenuCallback> callback)
{
	std::vector<std::tuple<std::string, int, bool, int>> menu_items;
	menu_items.reserve(model->GetCount());
	for (int i = 0; i < model->GetCount(); i++) {
		menu_items.push_back(
			{model->GetLabelAt(i), model->GetCommandIdAt(i),
			 model->IsEnabledAt(i), model->GetTypeAt(i)});
	}

	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(),
		[menu_items, callback]() {
			QMenu contextMenu;
			std::string name;
			int command_id;
			bool enabled;
			int type_id;

			for (int i = 0; i < menu_items.size(); i++) {
				std::tie(name, command_id, enabled, type_id) =
					menu_items[i];
				switch (type_id) {
				case MENUITEMTYPE_COMMAND: {
					QAction *item =
						new QAction(name.c_str());
					item->setEnabled(enabled);
					item->setProperty("cmd_id", command_id);
					contextMenu.addAction(item);
				} break;
				case MENUITEMTYPE_SEPARATOR:
					contextMenu.addSeparator();
					break;
				}
			}

			QAction *action = contextMenu.exec(QCursor::pos());
			if (action) {
				QVariant cmdId = action->property("cmd_id");
				callback.get()->Continue(cmdId.toInt(),
							 EVENTFLAG_NONE);
			} else {
				callback.get()->Cancel();
			}
		});
	return true;
}
#endif

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
#else
	UNUSED_PARAMETER(browser);
	UNUSED_PARAMETER(event);
#endif
	return false;
}
