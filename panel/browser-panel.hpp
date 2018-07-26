#pragma once

#include <util/platform.h>
#include <QWidget>
#include <string>

/* ------------------------------------------------------------------------- */

class QCefWidget : public QWidget {
	Q_OBJECT

protected:
	inline QCefWidget(QWidget *parent) : QWidget(parent)
	{
	}

public:
	virtual void setURL(const std::string &url)=0;

signals:
	void titleChanged(const QString &title);
};

/* ------------------------------------------------------------------------- */

typedef QCefWidget *(*CREATE_BROWSER_WIDGET_PROC)(QWidget *parent,
		const std::string &url);

static inline CREATE_BROWSER_WIDGET_PROC obs_browser_init_panel(void)
{
	CREATE_BROWSER_WIDGET_PROC proc = nullptr;
	void *lib = os_dlopen("obs-browser");
	if (!lib) {
		return nullptr;
	}

	proc = (CREATE_BROWSER_WIDGET_PROC)os_dlsym(lib,
			"obs_browser_create_widget");
	return proc;
}
