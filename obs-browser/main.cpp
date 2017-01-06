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
#include "util.hpp"

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
