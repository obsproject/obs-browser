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
	virtual void setStartupScript(const std::string &script)=0;

signals:
	void titleChanged(const QString &title);
	void urlChanged(const QString &url);
};

/* ------------------------------------------------------------------------- */

struct QCef {
	virtual ~QCef() {}

	virtual bool init_browser(void)=0;
	virtual bool initialized(void)=0;
	virtual bool wait_for_browser_init(void)=0;

	virtual QCefWidget *create_widget(
			QWidget *parent,
			const std::string &url)=0;
};

static inline QCef *obs_browser_init_panel(void)
{
	void *lib = os_dlopen("obs-browser");
	QCef *(*create_qcef)(void) = nullptr;

	if (!lib) {
		return nullptr;
	}

	create_qcef = (decltype(create_qcef))
		os_dlsym(lib, "obs_browser_create_qcef");
	if (!create_qcef)
		return nullptr;

	return create_qcef();
}
