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

#include "browser-version.h"
#include "browser-manager.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")

extern struct obs_source_info create_browser_source_info();
struct obs_source_info browser_source_info;

// Handle events from the frontend
static void handle_obs_frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {

		obs_source_t *source = obs_frontend_get_current_scene();

		const char *name = obs_source_get_name(source);

		BrowserManager::Instance()->ExecuteSceneChangeJSCallback(name);
		obs_source_release(source);
	}
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

	return true;
}

void obs_module_unload()
{
	BrowserManager::Instance()->Shutdown();
	BrowserManager::Instance()->SetModulePath(nullptr);
	BrowserManager::DestroyInstance();
}
