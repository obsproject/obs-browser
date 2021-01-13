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
#include <obs.hpp>
#include <functional>
#include <thread>
#include <mutex>

#include "obs-browser-source.hpp"
#include "browser-scheme.hpp"
#include "browser-app.hpp"
#include "browser-version.h"
#include "browser-config.h"

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

#if defined(USE_UI_LOOP) && defined(__APPLE__)
#include "browser-mac.h"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")

using namespace std;
using namespace json11;

static thread manager_thread;
static bool manager_initialized = false;
os_event_t *cef_started_event = nullptr;

static int adapterCount = 0;
static std::wstring deviceId;

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
bool hwaccel = false;
#endif

/* ========================================================================= */

#if defined(USE_UI_LOOP) && defined (WIN32)
extern MessageObject messageObject;
#endif


class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override
	{
#ifdef USE_UI_LOOP
		/* you have to put the tasks on the Qt event queue after this
		 * call otherwise the CEF message pump may stop functioning
		 * correctly, it's only supposed to take 10ms max */
#ifdef WIN32
		QMetaObject::invokeMethod(&messageObject, "ExecuteTask",
					  Qt::QueuedConnection,
					  Q_ARG(MessageTask, task));
#elif __APPLE__
		ExecuteTask(task);
#endif
#else
		task();
#endif
	}

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI,
			   CefRefPtr<BrowserTask>(new BrowserTask(task)));
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
				    "https://obsproject.com/browser-source");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	obs_data_set_default_bool(settings, "fps_custom", false);
#else
	obs_data_set_default_bool(settings, "fps_custom", true);
#endif
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_string(settings, "css", default_css);

#ifdef __APPLE__
	obs_data_set_default_bool(settings, "reroute_audio", true);
#else
	obs_data_set_default_bool(settings, "reroute_audio", false);
#endif
}

static bool is_local_file_modified(obs_properties_t *props, obs_property_t *,
				   obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "is_local_file");
	obs_property_t *url = obs_properties_get(props, "url");
	obs_property_t *local_file = obs_properties_get(props, "local_file");
	obs_property_set_visible(url, !enabled);
	obs_property_set_visible(local_file, enabled);

	return true;
}

static bool is_fps_custom(obs_properties_t *props, obs_property_t *,
			  obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, "fps_custom");
	obs_property_t *fps = obs_properties_get(props, "fps");
	obs_property_set_visible(fps, enabled);

	return true;
}

static obs_properties_t *browser_source_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	DStr path;

	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	obs_property_t *prop = obs_properties_add_bool(
		props, "is_local_file", obs_module_text("LocalFile"));

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
				obs_module_text("LocalFile"), OBS_PATH_FILE,
				"*.*", path->array);
	obs_properties_add_text(props, "url", obs_module_text("URL"),
				OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "width", obs_module_text("Width"), 1,
			       4096, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1,
			       4096, 1);

	obs_property_t *fps_set = obs_properties_add_bool(
		props, "fps_custom", obs_module_text("CustomFrameRate"));
	obs_property_set_modified_callback(fps_set, is_fps_custom);

#if !EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	obs_property_set_enabled(fps_set, false);
#endif

	obs_properties_add_bool(props, "reroute_audio",
				obs_module_text("RerouteAudio"));

	obs_properties_add_int(props, "fps", obs_module_text("FPS"), 1, 60, 1);
	obs_property_t *p = obs_properties_add_text(
		props, "css", obs_module_text("CSS"), OBS_TEXT_MULTILINE);
	obs_property_text_set_monospace(p, true);
	obs_properties_add_bool(props, "shutdown",
				obs_module_text("ShutdownSourceNotVisible"));
	obs_properties_add_bool(props, "restart_when_active",
				obs_module_text("RefreshBrowserActive"));

	obs_properties_add_button(
		props, "refreshnocache", obs_module_text("RefreshNoCache"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			static_cast<BrowserSource *>(data)->Refresh();
			return false;
		});
	return props;
}

static CefRefPtr<BrowserApp> app;

static void BrowserInit(void)
{
#if defined(__APPLE__) && defined(USE_UI_LOOP)
	ExecuteSyncTask([]() {
#endif
		string path = obs_get_module_binary_path(obs_current_module());
		path = path.substr(0, path.find_last_of('/') + 1);
		path += "//obs-browser-page";
#ifdef _WIN32
		path += ".exe";
		CefMainArgs args;
#else
		/* On non-windows platforms, ie macOS, we'll want to pass thru flags to
		* CEF */
		struct obs_cmdline_args cmdline_args = obs_get_cmdline_args();
		CefMainArgs args(cmdline_args.argc, cmdline_args.argv);
#endif

		CefSettings settings;
		settings.log_severity = LOGSEVERITY_DISABLE;
		settings.windowless_rendering_enabled = true;
		settings.no_sandbox = true;

#ifdef USE_UI_LOOP
		settings.external_message_pump = true;
		settings.multi_threaded_message_loop = false;
#endif

#ifdef __APPLE__
#ifdef BROWSER_DEPLOY
		std::string binPath = getExecutablePath();
		binPath = binPath.substr(0, binPath.find_last_of('/'));
		binPath += "/Frameworks/Chromium\ Embedded\ Framework.framework";
		CefString(&settings.framework_dir_path) = binPath;
#else
		CefString(&settings.framework_dir_path) = CEF_LIBRARY;
#endif
#endif

		std::string obs_locale = obs_get_locale();
		std::string accepted_languages;
		if (obs_locale != "en-US") {
			accepted_languages = obs_locale;
			accepted_languages += ",";
			accepted_languages += "en-US,en";
		} else {
			accepted_languages = "en-US,en";
		}

		BPtr<char> conf_path = obs_module_config_path("");
		if (conf_path) {
			if (os_mkdir(conf_path) >= 0) {
				/* Remove trailing slash since apparently this will
				* literally cause chromium to crash since it thinks
				* it's a different path */
				conf_path[strlen(conf_path.Get()) - 1] = '\0';

				BPtr<char> conf_path_abs = os_get_abs_path_ptr(conf_path);
				CefString(&settings.cache_path) = conf_path_abs;
			} else {
				blog(LOG_WARNING, "obs-browser: Could not create cache directory");
			}


		} else {
			blog(LOG_INFO, "obs-browser: empty config path");
		}

		CefString(&settings.locale) = obs_get_locale();
		CefString(&settings.accept_language_list) = accepted_languages;
		CefString(&settings.browser_subprocess_path) = path;

		bool tex_sharing_avail = false;

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
		if (hwaccel) {
			obs_enter_graphics();
			hwaccel = tex_sharing_avail = gs_shared_texture_available();
			obs_leave_graphics();
		}
#endif

	app = new BrowserApp(tex_sharing_avail);
	CefExecuteProcess(args, app, nullptr);
#ifdef _WIN32
	/* Massive (but amazing) hack to prevent chromium from modifying our
	 * process tokens and permissions, which caused us problems with winrt,
	 * used with window capture.  Note, the structure internally is just
	 * two pointers normally.  If it causes problems with future versions
	 * we'll just switch back to the static library but I doubt we'll need
	 * to. */
	uintptr_t zeroed_memory_lol[32] = {};
	CefInitialize(args, settings, app, zeroed_memory_lol);
#else
	CefInitialize(args, settings, app, nullptr);
#endif
#if !ENABLE_LOCAL_FILE_URL_SCHEME
		/* Register http://absolute/ scheme handler for older
		* CEF builds which do not support file:// URLs */
		CefRegisterSchemeHandlerFactory("http", "absolute",
						new BrowserSchemeHandlerFactory());
#endif
		os_event_signal(cef_started_event);
#if defined(__APPLE__) && defined(USE_UI_LOOP)
	});
#endif
}


#if defined(USE_UI_LOOP) && defined(WIN32)
extern MessageObject messageObject;
#elif defined(USE_UI_LOOP) && defined(__APPLE)
extern BrowserCppInt* message;
#endif


static void BrowserShutdown(void)
{
#ifdef USE_UI_LOOP
#ifdef WIN32
	while (messageObject.ExecuteNextBrowserTask())
		;
#elif __APPLE__
	while (ExecuteNextBrowserTask())
		;
#endif
	CefDoMessageLoopWork();
#endif
	CefShutdown();
	app = nullptr;
}

#ifndef USE_UI_LOOP
static void BrowserManagerThread(void)
{
	BrowserInit();
	CefRunMessageLoop();
	BrowserShutdown();
}
#endif

extern "C" EXPORT void obs_browser_initialize(void)
{
	if (!os_atomic_set_bool(&manager_initialized, true)) {
		blog(LOG_INFO, "Initialize obs_browser");
#ifdef USE_UI_LOOP
		BrowserInit();
#else
		manager_thread = thread(BrowserManagerThread);
#endif
	}
}

void RegisterBrowserSource()
{
	struct obs_source_info info = {};
	info.id = "browser_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO |
#if CHROME_VERSION_BUILD >= 3683
			    OBS_SOURCE_AUDIO |
#endif
			    OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
			    OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_properties = browser_source_get_properties;
	info.get_defaults = browser_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void *) { return obs_module_text("BrowserSource"); };
	info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		obs_browser_initialize();
		obs_source_set_audio_mixers(source, 0xFF);
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
		return new BrowserSource(settings, source);
	};
	info.destroy = [](void *data) {
		delete static_cast<BrowserSource *>(data);
	};
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->Update(settings);
	};
	info.get_width = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->width;
	};
	info.get_height = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->height;
	};
	info.video_tick = [](void *data, float) {
		static_cast<BrowserSource *>(data)->Tick();
	};
	info.video_render = [](void *data, gs_effect_t *) {
		static_cast<BrowserSource *>(data)->Render();
	};
#if CHROME_VERSION_BUILD >= 3683
	info.audio_mix = [](void *data, uint64_t *ts_out,
			    struct audio_output_data *audio_output,
			    size_t channels, size_t sample_rate) {
		return static_cast<BrowserSource *>(data)->AudioMix(
			ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void *data, obs_source_enum_proc_t cb,
				      void *param) {
		static_cast<BrowserSource *>(data)->EnumAudioStreams(cb, param);
	};
#endif
	info.mouse_click = [](void *data, const struct obs_mouse_event *event,
			      int32_t type, bool mouse_up,
			      uint32_t click_count) {
		static_cast<BrowserSource *>(data)->SendMouseClick(
			event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void *data, const struct obs_mouse_event *event,
			     bool mouse_leave) {
		static_cast<BrowserSource *>(data)->SendMouseMove(event,
								  mouse_leave);
	};
	info.mouse_wheel = [](void *data, const struct obs_mouse_event *event,
			      int x_delta, int y_delta) {
		static_cast<BrowserSource *>(data)->SendMouseWheel(
			event, x_delta, y_delta);
	};
	info.focus = [](void *data, bool focus) {
		static_cast<BrowserSource *>(data)->SendFocus(focus);
	};
	info.key_click = [](void *data, const struct obs_key_event *event,
			    bool key_up) {
		static_cast<BrowserSource *>(data)->SendKeyClick(event, key_up);
	};
	info.show = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(true);
	};
	info.hide = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(false);
	};
	info.activate = [](void *data) {
		BrowserSource *bs = static_cast<BrowserSource *>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
	};
	info.deactivate = [](void *data) {
		static_cast<BrowserSource *>(data)->SetActive(false);
	};

	obs_register_source(&info);
}

/* ========================================================================= */

extern void DispatchJSEvent(std::string eventName, std::string jsonString,
			    BrowserSource *browser = nullptr);

#if BROWSER_FRONTEND_API_SUPPORT_ENABLED
static void handle_obs_frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		DispatchJSEvent("obsStreamingStarting", "");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		DispatchJSEvent("obsStreamingStarted", "");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		DispatchJSEvent("obsStreamingStopping", "");
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		DispatchJSEvent("obsStreamingStopped", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		DispatchJSEvent("obsRecordingStarting", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		DispatchJSEvent("obsRecordingStarted", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		DispatchJSEvent("obsRecordingPaused", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		DispatchJSEvent("obsRecordingUnpaused", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		DispatchJSEvent("obsRecordingStopping", "");
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		DispatchJSEvent("obsRecordingStopped", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		DispatchJSEvent("obsReplaybufferStarting", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		DispatchJSEvent("obsReplaybufferStarted", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		DispatchJSEvent("obsReplaybufferStopping", "");
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		DispatchJSEvent("obsReplaybufferStopped", "");
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		OBSSource source = obs_frontend_get_current_scene();
		obs_source_release(source);

		if (!source)
			break;

		const char *name = obs_source_get_name(source);
		if (!name)
			break;

		Json json = Json::object{
			{"name", name},
			{"width", (int)obs_source_get_width(source)},
			{"height", (int)obs_source_get_height(source)}};

		DispatchJSEvent("obsSceneChanged", json.dump());
		break;
	}
	case OBS_FRONTEND_EVENT_EXIT:
		DispatchJSEvent("obsExit", "");
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

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory);
	if (FAILED(hr))
		return;

	while (factory->EnumAdapters1(i++, &adapter) == S_OK) {
		DXGI_ADAPTER_DESC desc;

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		if (i == 1)
			deviceId = desc.Description;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		adapterCount++;
	}
}
#endif

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
static const wchar_t *blacklisted_devices[] = {
	L"Intel", L"Microsoft", L"Radeon HD 8850M", L"Radeon HD 7660", nullptr};
#endif

static inline bool is_intel(const std::wstring &str)
{
	return wstrstri(str.c_str(), L"Intel") != 0;
}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
static void check_hwaccel_support(void)
{
	/* do not use hardware acceleration if a blacklisted device is the
	 * default and on 2 or more adapters */
	const wchar_t **device = blacklisted_devices;

	if (adapterCount >= 2 || !is_intel(deviceId)) {
		while (*device) {
			if (!!wstrstri(deviceId.c_str(), *device)) {
				hwaccel = false;
				blog(LOG_INFO, "[obs-browser]: "
					       "Blacklisted device "
					       "detected, "
					       "disabling browser "
					       "source hardware "
					       "acceleration.");
				break;
			}
			device++;
		}
	}
}
#endif

bool obs_module_load(void)
{
	blog(LOG_INFO, "[obs-browser]: Version %s", OBS_BROWSER_VERSION_STRING);

#if defined(USE_UI_LOOP) && defined(WIN32)
	qRegisterMetaType<MessageTask>("MessageTask");
#endif

	os_event_init(&cef_started_event, OS_EVENT_TYPE_MANUAL);

	CefEnableHighDPISupport();

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
	if (hwaccel) {
		check_hwaccel_support();
	}
	obs_data_release(private_data);
#endif

#ifdef __APPLE__
	if(isHighThanBigSur())
		obs_browser_initialize();
#endif

	return true;
}

void obs_module_unload(void)
{
#ifdef USE_UI_LOOP
	BrowserShutdown();
#else
	if (manager_thread.joinable()) {
		while (!QueueCEFTask([]() { CefQuitMessageLoop(); }))
			os_sleep_ms(5);

		manager_thread.join();
	}
#endif

	os_event_destroy(cef_started_event);
}
