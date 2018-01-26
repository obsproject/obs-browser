#include "wcui-browser-dialog.hpp"
#include "ui_wcui-browser-dialog.h"
#include "browser-manager.hpp"

WCUIBrowserDialog::WCUIBrowserDialog(QWidget* parent, std::string obs_module_path, std::string cache_path) :
	QDialog(parent, Qt::Dialog),
	ui(new Ui::WCUIBrowserDialog),
	m_obs_module_path(obs_module_path),
	m_cache_path(cache_path)
{
	ui->setupUi(this);
}

WCUIBrowserDialog::~WCUIBrowserDialog()
{
	DestroyBrowser();

	delete ui;
}

void WCUIBrowserDialog::ToggleShowHide()
{
	if (!isVisible())
	{
		setVisible(true);

		InitBrowser();
	}
	else
	{
		DestroyBrowser();

		setVisible(false);
	}
}

std::string WCUIBrowserDialog::GetCefBootstrapProcessPath()
{
	std::string parentPath(
		m_obs_module_path.substr(0,
		m_obs_module_path.find_last_of('/') + 1));

#ifdef _WIN32	
	return parentPath + "/cef-bootstrap.exe";
#else
	return parentPath + "/cef-bootstrap";
#endif
}

// Initialize CEF
void WCUIBrowserDialog::InitBrowser()
{
}


// Shutdown CEF
void WCUIBrowserDialog::DestroyBrowser()
{

}
