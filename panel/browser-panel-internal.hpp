#pragma once

#include <QTimer>
#include <QPointer>
#include "browser-panel.hpp"
#include "cef-headers.hpp"

#include <vector>
#include <mutex>

struct PopupCallbackInfo {
	std::string       url;
	QPointer<QObject> obj;
	const char        *method;
};

extern std::mutex                     popup_callbacks_mutex;
extern std::vector<PopupCallbackInfo> popup_callbacks;

/* ------------------------------------------------------------------------- */

class QCefRequestContextHandler : public CefRequestContextHandler {
	CefRefPtr<CefCookieManager> cm;

public:
	inline QCefRequestContextHandler(CefRefPtr<CefCookieManager> cm_)
		: cm(cm_)
	{}

	virtual CefRefPtr<CefCookieManager> GetCookieManager() override;

	IMPLEMENT_REFCOUNTING(QCefRequestContextHandler);
};

/* ------------------------------------------------------------------------- */

class QCefWidgetInternal : public QCefWidget {
	Q_OBJECT

public:
	QCefWidgetInternal(
			QWidget *parent,
			const std::string &url,
			CefRefPtr<CefRequestContextHandler> rch);
	~QCefWidgetInternal();

	CefRefPtr<CefBrowser> cefBrowser;
	std::string url;
	std::string script;
	CefRefPtr<CefRequestContextHandler> rch;
	QTimer timer;

	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;
	virtual QPaintEngine *paintEngine() const override;

	virtual void setURL(const std::string &url) override;
	virtual void setStartupScript(const std::string &script) override;

public slots:
	void Init();
};
