/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QAction>
#include <QMainWindow>

#include "browser-version.h"
#include "browser-manager.hpp"
#include "util.hpp"

#include "wcui/forms/wcui-browser-dialog.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")

extern struct obs_source_info create_browser_source_info();
struct obs_source_info browser_source_info;

// Handle events from the frontend
static void handle_obs_frontend_event(enum obs_frontend_event event, void *)
{
	switch(event)
	{
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		BrowserManager::Instance()->DispatchJSEvent("obsStreamingStarting", nullptr);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		BrowserManager::Instance()->DispatchJSEvent("obsStreamingStarted", nullptr);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		BrowserManager::Instance()->DispatchJSEvent("obsStreamingStopping", nullptr);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		BrowserManager::Instance()->DispatchJSEvent("obsStreamingStopped", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		BrowserManager::Instance()->DispatchJSEvent("obsRecordingStarting", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		BrowserManager::Instance()->DispatchJSEvent("obsRecordingStarted", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		BrowserManager::Instance()->DispatchJSEvent("obsRecordingStopping", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		BrowserManager::Instance()->DispatchJSEvent("obsRecordingStopped", nullptr);
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		{
			obs_source_t *source = obs_frontend_get_current_scene();

			const char* jsonString = obsSourceToJSON(source);

			BrowserManager::Instance()->DispatchJSEvent("obsSceneChanged", jsonString);

			obs_source_release(source);
			break;
		}
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED:
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		BrowserManager::Instance()->DispatchJSEvent("obsExit", nullptr);
		break;
	}
}

static WCUIBrowserDialog* s_wcui_WCUIBrowserDialog = NULL;

// Initialize WCUI (Web Configuration UI)
//
static void wcui_init()
{
	// Enable CEF high DPI support
	CefEnableHighDPISupport();

	// Browser dialog setup
	obs_frontend_push_ui_translation(obs_module_get_string);

	QMainWindow* obs_main_window = (QMainWindow*)obs_frontend_get_main_window();

	s_wcui_WCUIBrowserDialog = new WCUIBrowserDialog(
		obs_main_window,
		obs_get_module_binary_path(obs_current_module()),
		obs_module_config_path(""));

	obs_frontend_pop_ui_translation();

	// Tools menu item setup
	QAction* menu_action = (QAction*)obs_frontend_add_tools_menu_qaction(
		obs_module_text("WCUIToolsMenuItemTitle"));

	// Connect tools menu item to browser dialog show/hide method
	menu_action->connect(
		menu_action,
		&QAction::triggered,
		[] {
			if (s_wcui_WCUIBrowserDialog != NULL)
			{
				s_wcui_WCUIBrowserDialog->ShowModal();
			}
		});
}

// Shutdown WCUI (Web Configuration UI)
//
static void wcui_shutdown()
{
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[browser_source: 'Version: %s']", OBS_BROWSER_VERSION);

	browser_source_info = create_browser_source_info();

	BrowserManager::Instance()->SetModulePath(
			obs_get_module_binary_path(obs_current_module()));

	BrowserManager::Instance()->Startup();
	obs_register_source(&browser_source_info);
	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);

	wcui_init();

	return true;
}

void obs_module_unload()
{
	BrowserManager::Instance()->Shutdown();
	BrowserManager::Instance()->SetModulePath(nullptr);
	BrowserManager::DestroyInstance();

	wcui_shutdown();
}
