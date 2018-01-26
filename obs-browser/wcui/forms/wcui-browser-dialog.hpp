#ifndef WCUI_BROWSER_DIALOG_H
#define WCUI_BROWSER_DIALOG_H

#include <obs-frontend-api.h>

#include <QDialog>
#include <QWidget>

#include <util/platform.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>

namespace Ui {
	class WCUIBrowserDialog;
}

class WCUIBrowserDialog : public QDialog
{
	Q_OBJECT

public:
	explicit WCUIBrowserDialog(
		QWidget* parent,
		std::string obs_module_path,
		std::string cache_path);

	~WCUIBrowserDialog();
	void ToggleShowHide();

	/*
	private Q_SLOTS:
	void AuthCheckboxChanged();
	void FormAccepted();
	*/

private:
	std::string GetCefBootstrapProcessPath();

	void InitBrowser();
	void DestroyBrowser();

private:
	std::string m_obs_module_path;
	std::string m_cache_path;

	Ui::WCUIBrowserDialog* ui;
};

#endif // WCUI_BROWSER_DIALOG_H
