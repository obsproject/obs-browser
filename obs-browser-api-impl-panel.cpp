// SPDX-FileCopyrightText: 2023 tytan652 <tytan652@tytanium.xyz>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "obs-browser-api-impl.hpp"

#include "panel/browser-panel.hpp"

#include <obs.hpp>

#include <QApplication>

extern "C" int obs_browser_qcef_version_export(void);
extern "C" QCef *obs_browser_create_qcef(void);

static void qcef_free(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	if (qcef)
		delete qcef;
}

static void qcef_init_browser(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	assert(qcef != nullptr);

	calldata_set_bool(cd, "already_initialized", qcef->init_browser());
}

static void qcef_initialized(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	assert(qcef != nullptr);

	calldata_set_bool(cd, "initialized", qcef->initialized());
}

static void qcef_wait_for_browser_init(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	assert(qcef != nullptr);

	calldata_set_bool(cd, "initialized", qcef->wait_for_browser_init());
}

static void qcef_get_cookie_path(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	const char *c_storage_path = calldata_string(cd, "storage_path");
	std::string storage_path;
	BPtr<char> path;
	assert(qcef != nullptr);

	if (c_storage_path)
		storage_path = std::string(c_storage_path);

	path = qcef->get_cookie_path(storage_path);
	calldata_set_ptr(cd, "path", bstrdup(path));
}

static void qcef_add_popup_whitelist_url(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	const char *c_url = calldata_string(cd, "url");
	auto obj = static_cast<QObject *>(calldata_ptr(cd, "qobject"));
	std::string url;
	assert(qcef != nullptr);

	if (c_url)
		url = std::string(c_url);

	qcef->add_popup_whitelist_url(url, obj);
}

static void qcef_add_force_popup_url(void *, calldata_t *cd)
{
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	const char *c_url = calldata_string(cd, "url");
	auto obj = static_cast<QObject *>(calldata_ptr(cd, "qobject"));
	std::string url;
	assert(qcef != nullptr);

	if (c_url)
		url = std::string(c_url);

	qcef->add_force_popup_url(url, obj);
}

static void qcef_cookie_manager_free(void *, calldata_t *cd)
{
	auto qcef_cookie_manager = static_cast<QCefCookieManager *>(
		calldata_ptr(cd, "qcef_cookie_manager"));
	if (qcef_cookie_manager)
		delete qcef_cookie_manager;
}

static void qcef_cookie_manager_delete_cookies(void *, calldata_t *cd)
{
	auto qcef_cookie_manager = static_cast<QCefCookieManager *>(
		calldata_ptr(cd, "qcef_cookie_manager"));
	const char *c_url = calldata_string(cd, "url");
	const char *c_name = calldata_string(cd, "name");
	std::string url, name;
	assert(qcef_cookie_manager != nullptr);

	if (c_url)
		url = std::string(c_url);
	if (c_name)
		name = std::string(c_name);

	calldata_set_bool(cd, "success",
			  qcef_cookie_manager->DeleteCookies(url, name));
}

static void qcef_cookie_manager_flush_store(void *, calldata_t *cd)
{
	auto qcef_cookie_manager = static_cast<QCefCookieManager *>(
		calldata_ptr(cd, "qcef_cookie_manager"));
	assert(qcef_cookie_manager != nullptr);

	calldata_set_bool(cd, "success", qcef_cookie_manager->FlushStore());
}

static void qcef_cookie_manager_check_for_cookie(void *, calldata_t *cd)
{
	auto qcef_cookie_manager = static_cast<QCefCookieManager *>(
		calldata_ptr(cd, "qcef_cookie_manager"));
	const char *c_site = calldata_string(cd, "site");
	const char *c_cookie = calldata_string(cd, "cookie");
	auto callback = static_cast<QCefCookieManager::cookie_exists_cb *>(
		calldata_ptr(cd, "callback"));
	std::string site, cookie;
	assert(qcef_cookie_manager != nullptr);

	if (c_site)
		site = std::string(c_site);
	if (c_cookie)
		cookie = std::string(c_cookie);

	qcef_cookie_manager->CheckForCookie(site, cookie, *callback);
}

static void qcef_create_cookie_manager(void *, calldata_t *cd)
{
	proc_handler_t *ph = proc_handler_create();
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	const char *c_storage_path = calldata_string(cd, "storage_path");
	bool persist_session_cookies =
		calldata_bool(cd, "persist_session_cookies");
	std::string storage_path;
	assert(qcef != nullptr);

	if (c_storage_path)
		storage_path = std::string(c_storage_path);

	proc_handler_add(
		ph, "void qcef_cookie_manager_free(in ptr qcef_cookie_manager)",
		&qcef_cookie_manager_free, nullptr);
	proc_handler_add(
		ph,
		"void delete_cookies(in ptr qcef_cookie_manager, in string url, in string name, out bool success)",
		&qcef_cookie_manager_delete_cookies, nullptr);
	proc_handler_add(
		ph,
		"void flush_store(in ptr qcef_cookie_manager, out bool success)",
		&qcef_cookie_manager_flush_store, nullptr);
	proc_handler_add(
		ph,
		"void check_for_cookie(in ptr qcef_cookie_manager, in string site, in string cookie, in ptr callback)",
		&qcef_cookie_manager_check_for_cookie, nullptr);

	calldata_set_ptr(cd, "ph", ph);
	calldata_set_ptr(cd, "cookie_manager",
			 qcef->create_cookie_manager(storage_path,
						     persist_session_cookies));
}

static void qcef_widget_set_url(void *, calldata_t *cd)
{
	auto qcef_widget =
		static_cast<QCefWidget *>(calldata_ptr(cd, "qcef_widget"));
	const char *c_url = calldata_string(cd, "url");
	std::string url;
	assert(qcef_widget != nullptr);

	if (c_url)
		url = std::string(c_url);

	qcef_widget->setURL(url);
}

static void qcef_widget_set_startup_script(void *, calldata_t *cd)
{
	auto qcef_widget =
		static_cast<QCefWidget *>(calldata_ptr(cd, "qcef_widget"));
	const char *c_script = calldata_string(cd, "script");
	std::string script;
	assert(qcef_widget != nullptr);

	if (c_script)
		script = std::string(c_script);

	qcef_widget->setStartupScript(script);
}

static void qcef_widget_allow_all_popups(void *, calldata_t *cd)
{
	auto qcef_widget =
		static_cast<QCefWidget *>(calldata_ptr(cd, "qcef_widget"));
	assert(qcef_widget != nullptr);

	qcef_widget->allowAllPopups(calldata_bool(cd, "allow"));
}

static void qcef_widget_close_browser(void *, calldata_t *cd)
{
	auto qcef_widget =
		static_cast<QCefWidget *>(calldata_ptr(cd, "qcef_widget"));
	assert(qcef_widget != nullptr);

	qcef_widget->closeBrowser();
}

static void qcef_widget_reload_page(void *, calldata_t *cd)
{
	auto qcef_widget =
		static_cast<QCefWidget *>(calldata_ptr(cd, "qcef_widget"));
	assert(qcef_widget != nullptr);

	qcef_widget->reloadPage();
}

static void qcef_widget_execute_javascript(void *, calldata_t *cd)
{
	auto qcef_widget =
		static_cast<QCefWidget *>(calldata_ptr(cd, "qcef_widget"));
	const char *c_script = calldata_string(cd, "script");
	std::string script;
	assert(qcef_widget != nullptr);

	if (c_script)
		script = std::string(c_script);

	qcef_widget->executeJavaScript(script);
}

static void qcef_create_widget(void *, calldata_t *cd)
{
	proc_handler_t *ph = proc_handler_create();
	auto qcef = static_cast<QCef *>(calldata_ptr(cd, "qcef"));
	auto parent =
		static_cast<QWidget *>(calldata_ptr(cd, "qwidget_parent"));
	const char *c_url = calldata_string(cd, "url");
	auto cookie_manager = static_cast<QCefCookieManager *>(
		calldata_ptr(cd, "cookie_manager"));
	std::string url;
	assert(qcef != nullptr);

	if (c_url)
		url = std::string(c_url);

	proc_handler_add(ph, "void set_url(in ptr qcef_widget, in string url)",
			 qcef_widget_set_url, nullptr);
	proc_handler_add(
		ph,
		"void set_startup_script(in ptr qcef_widget, in string script)",
		qcef_widget_set_startup_script, nullptr);
	proc_handler_add(
		ph, "void allow_all_popups(in ptr qcef_widget, in bool allow)",
		qcef_widget_allow_all_popups, nullptr);
	proc_handler_add(ph, "void close_browser(in ptr qcef_widget)",
			 qcef_widget_close_browser, nullptr);
	proc_handler_add(ph, "void reload_page(in ptr qcef_widget)",
			 qcef_widget_reload_page, nullptr);
	proc_handler_add(
		ph,
		"void execute_javascript(in ptr qcef_widget, in string script)",
		qcef_widget_execute_javascript, nullptr);

	calldata_set_ptr(cd, "ph", ph);
	calldata_set_ptr(cd, "qcef_widget",
			 qcef->create_widget(parent, url, cookie_manager));
}

void BrowserApi::get_qcef_version(void *, calldata_t *cd)
{
	calldata_set_int(cd, "version", obs_browser_qcef_version_export());
}

void BrowserApi::create_qcef(void *, calldata_t *cd)
{
	proc_handler_t *ph;
#ifdef __linux__
	if (qApp->platformName().contains("wayland"))
		return;
#endif
	ph = proc_handler_create();

	proc_handler_add(ph, "void qcef_free(in ptr qcef)", &qcef_free,
			 nullptr);

	proc_handler_add(
		ph,
		"void init_browser(in ptr qcef, out bool already_initialized)",
		&qcef_init_browser, nullptr);
	proc_handler_add(ph,
			 "void initialized(in ptr qcef, out bool initialized)",
			 &qcef_initialized, nullptr);
	proc_handler_add(
		ph,
		"void wait_for_browser_init(in ptr qcef, out bool initialized)",
		&qcef_wait_for_browser_init, nullptr);
	proc_handler_add(
		ph,
		"void create_widget(in ptr qcef, in ptr qwidget_parent, in string url, in ptr qcef_cookie_manager, out ptr ph, out ptr qcef_widget)",
		&qcef_create_widget, nullptr);
	proc_handler_add(
		ph,
		"void create_cookie_manager(in ptr qcef, in string storage_path, in bool persist_session_cookies, out ptr ph, out ptr qcef_cookie_manager)",
		&qcef_create_cookie_manager, nullptr);
	proc_handler_add(
		ph,
		"void get_cookie_path(in ptr qcef, in string storage_path, out ptr path)",
		&qcef_get_cookie_path, nullptr);
	proc_handler_add(
		ph,
		"void add_popup_whitelist_url(in ptr qcef, in string url, in ptr qobject)",
		&qcef_add_popup_whitelist_url, nullptr);
	proc_handler_add(
		ph,
		"void add_force_popup_url(in ptr qcef, in string url, in ptr qobject)",
		&qcef_add_force_popup_url, nullptr);

	calldata_set_ptr(cd, "ph", ph);
	calldata_set_ptr(cd, "qcef", obs_browser_create_qcef());
}
