#include "browser-panel-client.hpp"

#include <QUrl>
#include <QDesktopServices>

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


/* CefDisplayHandler */
void QCefBrowserClient::OnTitleChange(
		CefRefPtr<CefBrowser>,
		const CefString &title)
{
	if (widget) {
		std::string str_title = title;
		QString qt_title = QString::fromUtf8(str_title.c_str());
		QMetaObject::invokeMethod(widget, "titleChanged",
				Q_ARG(QString, qt_title));
	}
}

/* CefRequestHandler */
bool QCefBrowserClient::OnBeforeBrowse(
		CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame>,
		CefRefPtr<CefRequest> request,
		bool,
		bool)
{
	if (widget) {
		std::string str_url = request->GetURL();
		QString qt_url = QString::fromUtf8(str_url.c_str());
		QMetaObject::invokeMethod(widget, "urlChanged",
				Q_ARG(QString, qt_url));
	}
	return false;
}

/* CefLifeSpanHandler */
bool QCefBrowserClient::OnBeforePopup(
		CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame>,
		const CefString &target_url,
		const CefString &,
		CefLifeSpanHandler::WindowOpenDisposition,
		bool,
		const CefPopupFeatures &,
		CefWindowInfo &,
		CefRefPtr<CefClient> &,
		CefBrowserSettings &,
		bool *)
{
	/* Open popup URLs in user's actual browser */
	std::string str_url = target_url;
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

void QCefBrowserClient::OnLoadEnd(
		CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame> frame,
		int)
{
	if (frame->IsMain() && !script.empty())
		frame->ExecuteJavaScript(script, CefString(), 0);
}
