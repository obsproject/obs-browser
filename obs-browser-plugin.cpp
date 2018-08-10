/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

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

#include <util/threading.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/dstr.hpp>
#include <obs-module.h>
#include <thread>
#include <mutex>

#include "obs-browser-source.hpp"
#include "browser-scheme.hpp"
#include "browser-app.hpp"
#include "browser-version.h"

#include "json11/json11.hpp"
#include "cef-headers.hpp"

#ifdef _WIN32
#include <util/windows/ComPtr.hpp>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#endif

#if BROWSER_FRONTEND_API_SUPPORT_ENABLED
#include <obs-frontend-api.h>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")

using namespace std;
using namespace json11;

static thread manager_thread;

static int adapterCount = 0;

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
bool hwaccel = false;
#endif

/* ========================================================================= */

class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override {task();}

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI, CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

/* ========================================================================= */

static const char *default_css = "\
body { \
background-color: rgba(0, 0, 0, 0); \
margin: 0px auto; \
overflow: hidden; \
}";

static void browser_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "url",
			"https://www.obsproject.com");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_string(settings, "css", default_css);
}

static bool is_local_file_modified(obs_properties_t *props,
		obs_property_t *, obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "is_local_file");
	obs_property_t *url = obs_properties_get(props, "url");
	obs_property_t *local_file = obs_properties_get(props, "local_file");
	obs_property_set_visible(url, !enabled);
	obs_property_set_visible(local_file, enabled);

	return true;
}

static obs_properties_t *browser_source_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	DStr path;

	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	obs_property_t *prop = obs_properties_add_bool(props, "is_local_file",
			obs_module_text("LocalFile"));

	if (bs && !bs->url.empty()) {
		const char *slash;

		dstr_copy(path, bs->url.c_str());
		dstr_replace(path, "\\", "/");
		slash = strrchr(path->array, '/');
		if (slash)
			dstr_resize(path, slash - path->array + 1);
	}

	obs_property_set_modified_callback(prop, is_local_file_modified);
	obs_properties_add_path(props, "local_file",
			obs_module_text("Local file"), OBS_PATH_FILE, "*.*",
			path->array);
	obs_properties_add_text(props, "url",
			obs_module_text("URL"), OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "width",
			obs_module_text("Width"), 1, 4096, 1);
	obs_properties_add_int(props, "height",
			obs_module_text("Height"), 1, 4096, 1);
	obs_properties_add_int(props, "fps",
			obs_module_text("FPS"), 1, 60, 1);
	obs_properties_add_text(props, "css",
			obs_module_text("CSS"), OBS_TEXT_MULTILINE);
	obs_properties_add_bool(props, "shutdown",
			obs_module_text("ShutdownSourceNotVisible"));
	obs_properties_add_bool(props, "restart_when_active",
			obs_module_text("RefreshBrowserActive"));

	obs_properties_add_button(props, "refreshnocache",
			obs_module_text("RefreshNoCache"),
			[] (obs_properties_t *, obs_property_t *, void *data)
			{
				static_cast<BrowserSource *>(data)->Refresh();
				return false;
			});
	return props;
}

static void BrowserManagerThread(void)
{
	string path = obs_get_module_binary_path(obs_current_module());
	path = path.substr(0, path.find_last_of('/') + 1);
	path += "//cef-bootstrap";
#ifdef _WIN32
	path += ".exe";
#endif

	CefMainArgs args;
	CefSettings settings;
	settings.log_severity = LOGSEVERITY_VERBOSE;
	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = true;

#if defined(__APPLE__) && defined(_DEBUG)
	CefString(&settings.framework_dir_path) =
		"/Library/Frameworks/Chromium Embedded Framework.framework";
#endif

	BPtr<char> conf_path = obs_module_config_path("");
	CefString(&settings.cache_path) = conf_path;
	CefString(&settings.browser_subprocess_path) = path;

	bool tex_sharing_avail = false;

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	if (hwaccel) {
		obs_enter_graphics();
		tex_sharing_avail = gs_shared_texture_available();
		obs_leave_graphics();
	}
#endif

	CefRefPtr<BrowserApp> app(new BrowserApp(tex_sharing_avail));
	CefExecuteProcess(args, app, nullptr);
	CefInitialize(args, settings, app, nullptr);
	CefRegisterSchemeHandlerFactory("http", "absolute",
			new BrowserSchemeHandlerFactory());
	CefRunMessageLoop();
	CefShutdown();
}

extern "C" EXPORT void obs_browser_initialize(void)
{
	static bool manager_initialized = false;
	if (!os_atomic_set_bool(&manager_initialized, true)) {
		manager_thread = thread(BrowserManagerThread);
	}
}

void RegisterBrowserSource()
{
	struct obs_source_info info = {};
	info.id                     = "browser_source";
	info.type                   = OBS_SOURCE_TYPE_INPUT;
	info.output_flags           = OBS_SOURCE_VIDEO |
	                              OBS_SOURCE_CUSTOM_DRAW |
	                              OBS_SOURCE_INTERACTION |
	                              OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_properties         = browser_source_get_properties;
	info.get_defaults           = browser_source_get_defaults;

	info.get_name = [] (void *)
	{
		return obs_module_text("BrowserSource");
	};
	info.create = [] (obs_data_t *settings, obs_source_t *source) -> void *
	{
		obs_browser_initialize();
		return new BrowserSource(settings, source);
	};
	info.destroy = [] (void *data)
	{
		delete static_cast<BrowserSource *>(data);
	};
	info.update = [] (void *data, obs_data_t *settings)
	{
		static_cast<BrowserSource *>(data)->Update(settings);
	};
	info.get_width = [] (void *data)
	{
		return (uint32_t)static_cast<BrowserSource *>(data)->width;
	};
	info.get_height = [] (void *data)
	{
		return (uint32_t)static_cast<BrowserSource *>(data)->height;
	};
	info.video_tick = [] (void *data, float)
	{
		static_cast<BrowserSource *>(data)->Tick();
	};
	info.video_render = [] (void *data, gs_effect_t *)
	{
		static_cast<BrowserSource *>(data)->Render();
	};
	info.mouse_click = [] (
			void *data,
			const struct obs_mouse_event *event,
			int32_t type,
			bool mouse_up,
			uint32_t click_count)
	{
		static_cast<BrowserSource *>(data)->SendMouseClick(
				event, type, mouse_up, click_count);
	};
	info.mouse_move = [] (
			void *data,
			const struct obs_mouse_event *event,
			bool mouse_leave)
	{
		static_cast<BrowserSource *>(data)->SendMouseMove(
				event, mouse_leave);
	};
	info.mouse_wheel = [] (
			void *data,
			const struct obs_mouse_event *event,
			int x_delta,
			int y_delta)
	{
		static_cast<BrowserSource *>(data)->SendMouseWheel(
				event, x_delta, y_delta);
	};
	info.focus = [] (void *data, bool focus)
	{
		static_cast<BrowserSource *>(data)->SendFocus(focus);
	};
	info.key_click = [] (
			void *data,
			const struct obs_key_event *event,
			bool key_up)
	{
		static_cast<BrowserSource *>(data)->SendKeyClick(event, key_up);
	};
	info.show = [] (void *data)
	{
		static_cast<BrowserSource *>(data)->SetShowing(true);
	};
	info.hide = [] (void *data)
	{
		static_cast<BrowserSource *>(data)->SetShowing(false);
	};
	info.activate = [] (void *data)
	{
		BrowserSource *bs = static_cast<BrowserSource *>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
	};
	info.deactivate = [] (void *data)
	{
		static_cast<BrowserSource *>(data)->SetActive(false);
	};

	obs_register_source(&info);
}

/* ========================================================================= */

extern void DispatchJSEvent(const char *eventName, const char *jsonString);

#if BROWSER_FRONTEND_API_SUPPORT_ENABLED
static void handle_obs_frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		DispatchJSEvent("obsStreamingStarting", nullptr);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		DispatchJSEvent("obsStreamingStarted", nullptr);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		DispatchJSEvent("obsStreamingStopping", nullptr);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		DispatchJSEvent("obsStreamingStopped", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		DispatchJSEvent("obsRecordingStarting", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		DispatchJSEvent("obsRecordingStarted", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		DispatchJSEvent("obsRecordingStopping", nullptr);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		DispatchJSEvent("obsRecordingStopped", nullptr);
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		{
			obs_source_t *source = obs_frontend_get_current_scene();

			Json json = Json::object {
				{"name", obs_source_get_name(source)},
				{"width", (int)obs_source_get_width(source)},
				{"height", (int)obs_source_get_height(source)}
			};

			obs_source_release(source);

			DispatchJSEvent("obsSceneChanged", json.dump().c_str());
			break;
		}
	case OBS_FRONTEND_EVENT_EXIT:
		DispatchJSEvent("obsExit", nullptr);
		break;
	default:;
	}
}
#endif

#ifdef _WIN32
static inline void EnumAdapterCount()
{
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	HRESULT hr;
	UINT i = 0;

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
	if (FAILED(hr))
		return;

	while (factory->EnumAdapters1(i++, &adapter) == S_OK) {
		DXGI_ADAPTER_DESC desc;

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		adapterCount++;
	}
}
#endif

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs-browser]: Version %s",
			OBS_BROWSER_VERSION_STRING);

#ifdef _WIN32
	EnumAdapterCount();
#endif
	RegisterBrowserSource();

#if BROWSER_FRONTEND_API_SUPPORT_ENABLED
	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);
#endif


#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	obs_data_t *private_data = obs_get_private_data();
	hwaccel = obs_data_get_bool(private_data, "BrowserHWAccel");
	obs_data_release(private_data);
#endif
	return true;
}

void obs_module_unload(void)
{
	if (manager_thread.joinable()) {
		while (!QueueCEFTask([] () {CefQuitMessageLoop();}))
			os_sleep_ms(5);

		manager_thread.join();
	}
}
