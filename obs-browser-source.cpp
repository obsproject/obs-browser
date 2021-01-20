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

#include "obs-browser-source.hpp"
#include "browser-client.hpp"
#include "browser-scheme.hpp"
#include "wide-string.hpp"
#include "json11/json11.hpp"
#include <util/threading.h>
#include <QApplication>
#include <util/dstr.h>
#include <functional>
#include <thread>
#include <mutex>

#ifdef __linux__
#include "linux-keyboard-helpers.hpp"
#endif

#ifdef USE_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

using namespace std;
using namespace json11;

extern bool QueueCEFTask(std::function<void()> task);

static mutex browser_list_mutex;
static BrowserSource *first_browser = nullptr;

static void SendBrowserVisibility(CefRefPtr<CefBrowser> browser, bool isVisible)
{
	if (!browser)
		return;

#if ENABLE_WASHIDDEN
	if (isVisible) {
		browser->GetHost()->WasHidden(false);
		browser->GetHost()->Invalidate(PET_VIEW);
	} else {
		browser->GetHost()->WasHidden(true);
	}
#endif

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("Visibility");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();
	args->SetBool(0, isVisible);
	SendBrowserProcessMessage(browser, PID_RENDERER, msg);
}

void DispatchJSEvent(std::string eventName, std::string jsonString,
		     BrowserSource *browser = nullptr);

BrowserSource::BrowserSource(obs_data_t *, obs_source_t *source_)
	: source(source_)
{
	/* defer update */
	obs_source_update(source, nullptr);

	lock_guard<mutex> lock(browser_list_mutex);
	p_prev_next = &first_browser;
	next = first_browser;
	if (first_browser)
		first_browser->p_prev_next = &next;
	first_browser = this;
}

BrowserSource::~BrowserSource()
{
	DestroyBrowser();
	DestroyTextures();

	lock_guard<mutex> lock(browser_list_mutex);
	if (next)
		next->p_prev_next = p_prev_next;
	*p_prev_next = next;
}

void BrowserSource::ExecuteOnBrowser(BrowserFunc func, bool async)
{
	if (!async) {
#ifdef USE_QT_LOOP
		if (QThread::currentThread() == qApp->thread()) {
			if (!!cefBrowser)
				func(cefBrowser);
			return;
		}
#endif
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&]() {
			if (!!cefBrowser)
				func(cefBrowser);
			os_event_signal(finishedEvent);
		});
		if (success) {
			os_event_wait(finishedEvent);
		}
		os_event_destroy(finishedEvent);
	} else {
		CefRefPtr<CefBrowser> browser = cefBrowser;
		if (!!browser) {
#ifdef USE_QT_LOOP
			QueueBrowserTask(cefBrowser, func);
#else
			QueueCEFTask([=]() { func(browser); });
#endif
		}
	}
}

bool BrowserSource::CreateBrowser()
{
	return QueueCEFTask([this]() {
#ifdef SHARED_TEXTURE_SUPPORT_ENABLED
		if (hwaccel) {
			obs_enter_graphics();
			tex_sharing_avail = gs_shared_texture_available();
			obs_leave_graphics();
		}
#else
		bool hwaccel = false;
#endif

		CefRefPtr<BrowserClient> browserClient = new BrowserClient(
			this, hwaccel && tex_sharing_avail, reroute_audio);

		CefWindowInfo windowInfo;
#if CHROME_VERSION_BUILD < 3071
		windowInfo.transparent_painting_enabled = true;
#endif
		windowInfo.width = width;
		windowInfo.height = height;
		windowInfo.windowless_rendering_enabled = true;

#ifdef SHARED_TEXTURE_SUPPORT_ENABLED
		windowInfo.shared_texture_enabled = hwaccel;
#endif

		CefBrowserSettings cefBrowserSettings;

#ifdef SHARED_TEXTURE_SUPPORT_ENABLED
#ifdef _WIN32
		if (!fps_custom) {
			windowInfo.external_begin_frame_enabled = true;
			cefBrowserSettings.windowless_frame_rate = 0;
		} else {
			cefBrowserSettings.windowless_frame_rate = fps;
		}
#else
		double video_fps = obs_get_active_fps();
		cefBrowserSettings.windowless_frame_rate =
			(fps_custom) ? fps : video_fps;
#endif
#else
		cefBrowserSettings.windowless_frame_rate = fps;
#endif

		cefBrowserSettings.default_font_size = 16;
		cefBrowserSettings.default_fixed_font_size = 16;

#if ENABLE_LOCAL_FILE_URL_SCHEME
		if (is_local) {
			/* Disable web security for file:// URLs to allow
			 * local content access to remote APIs */
			cefBrowserSettings.web_security = STATE_DISABLED;
		}
#endif

		cefBrowser = CefBrowserHost::CreateBrowserSync(
			windowInfo, browserClient, url, cefBrowserSettings,
#if CHROME_VERSION_BUILD >= 3770
			CefRefPtr<CefDictionaryValue>(),
#endif
			nullptr);
#if CHROME_VERSION_BUILD >= 3683
		if (reroute_audio)
			cefBrowser->GetHost()->SetAudioMuted(true);
#endif

		SendBrowserVisibility(cefBrowser, is_showing);
	});
}

void BrowserSource::DestroyBrowser(bool async)
{
	ExecuteOnBrowser(
		[](CefRefPtr<CefBrowser> cefBrowser) {
			CefRefPtr<CefClient> client =
				cefBrowser->GetHost()->GetClient();
			BrowserClient *bc =
				reinterpret_cast<BrowserClient *>(client.get());
			if (bc) {
				bc->bs = nullptr;
			}

			/*
		 * This stops rendering
		 * http://magpcss.org/ceforum/viewtopic.php?f=6&t=12079
		 * https://bitbucket.org/chromiumembedded/cef/issues/1363/washidden-api-got-broken-on-branch-2062)
		 */
			cefBrowser->GetHost()->WasHidden(true);
			cefBrowser->GetHost()->CloseBrowser(true);
		},
		async);

	cefBrowser = nullptr;
}
#if CHROME_VERSION_BUILD < 4103 && CHROME_VERSION_BUILD >= 3683
void BrowserSource::ClearAudioStreams()
{
	QueueCEFTask([this]() {
		audio_streams.clear();
		std::lock_guard<std::mutex> lock(audio_sources_mutex);
		audio_sources.clear();
	});
}
#endif
void BrowserSource::SendMouseClick(const struct obs_mouse_event *event,
				   int32_t type, bool mouse_up,
				   uint32_t click_count)
{
	uint32_t modifiers = event->modifiers;
	int32_t x = event->x;
	int32_t y = event->y;

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> cefBrowser) {
			CefMouseEvent e;
			e.modifiers = modifiers;
			e.x = x;
			e.y = y;
			CefBrowserHost::MouseButtonType buttonType =
				(CefBrowserHost::MouseButtonType)type;
			cefBrowser->GetHost()->SendMouseClickEvent(
				e, buttonType, mouse_up, click_count);
		},
		true);
}

void BrowserSource::SendMouseMove(const struct obs_mouse_event *event,
				  bool mouse_leave)
{
	uint32_t modifiers = event->modifiers;
	int32_t x = event->x;
	int32_t y = event->y;

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> cefBrowser) {
			CefMouseEvent e;
			e.modifiers = modifiers;
			e.x = x;
			e.y = y;
			cefBrowser->GetHost()->SendMouseMoveEvent(e,
								  mouse_leave);
		},
		true);
}

void BrowserSource::SendMouseWheel(const struct obs_mouse_event *event,
				   int x_delta, int y_delta)
{
	uint32_t modifiers = event->modifiers;
	int32_t x = event->x;
	int32_t y = event->y;

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> cefBrowser) {
			CefMouseEvent e;
			e.modifiers = modifiers;
			e.x = x;
			e.y = y;
			cefBrowser->GetHost()->SendMouseWheelEvent(e, x_delta,
								   y_delta);
		},
		true);
}

void BrowserSource::SendFocus(bool focus)
{
	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> cefBrowser) {
			cefBrowser->GetHost()->SendFocusEvent(focus);
		},
		true);
}

void BrowserSource::SendKeyClick(const struct obs_key_event *event, bool key_up)
{
	uint32_t modifiers = event->modifiers;
	UNUSED_PARAMETER(modifiers);
	std::string text = event->text;
#ifdef __linux__
	uint32_t native_vkey = KeyboardCodeFromXKeysym(event->native_vkey);
#else
	uint32_t native_vkey = event->native_vkey;
#endif
	uint32_t native_scancode = event->native_scancode;
	uint32_t native_modifiers = event->native_modifiers;

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> cefBrowser) {
			CefKeyEvent e;
			e.windows_key_code = native_vkey;
			e.native_key_code = native_scancode;

			e.type = key_up ? KEYEVENT_KEYUP : KEYEVENT_RAWKEYDOWN;

			if (!text.empty()) {
				wstring wide = to_wide(text);
				if (wide.size())
					e.character = wide[0];
			}

			//e.native_key_code = native_vkey;
			e.modifiers = native_modifiers;

			cefBrowser->GetHost()->SendKeyEvent(e);
			if (!text.empty() && !key_up) {
				e.type = KEYEVENT_CHAR;
#ifdef __linux__
				e.windows_key_code =
					KeyboardCodeFromXKeysym(e.character);
#else
				e.windows_key_code = e.character;
#endif
				e.native_key_code = native_scancode;
				cefBrowser->GetHost()->SendKeyEvent(e);
			}
		},
		true);
}

void BrowserSource::SetShowing(bool showing)
{
	is_showing = showing;

	if (shutdown_on_invisible) {
		if (showing) {
			Update();
		} else {
			DestroyBrowser(true);
		}
	} else {
		ExecuteOnBrowser(
			[=](CefRefPtr<CefBrowser> cefBrowser) {
				CefRefPtr<CefProcessMessage> msg =
					CefProcessMessage::Create("Visibility");
				CefRefPtr<CefListValue> args =
					msg->GetArgumentList();
				args->SetBool(0, showing);
				SendBrowserProcessMessage(cefBrowser,
							  PID_RENDERER, msg);
			},
			true);
		Json json = Json::object{{"visible", showing}};
		DispatchJSEvent("obsSourceVisibleChanged", json.dump(), this);
#if defined(_WIN32) && defined(SHARED_TEXTURE_SUPPORT_ENABLED)
		if (showing && !fps_custom) {
			reset_frame = false;
		}
#endif

		SendBrowserVisibility(cefBrowser, showing);
	}
}

void BrowserSource::SetActive(bool active)
{
	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> cefBrowser) {
			CefRefPtr<CefProcessMessage> msg =
				CefProcessMessage::Create("Active");
			CefRefPtr<CefListValue> args = msg->GetArgumentList();
			args->SetBool(0, active);
			SendBrowserProcessMessage(cefBrowser, PID_RENDERER,
						  msg);
		},
		true);
	Json json = Json::object{{"active", active}};
	DispatchJSEvent("obsSourceActiveChanged", json.dump(), this);
}

void BrowserSource::Refresh()
{
	ExecuteOnBrowser(
		[](CefRefPtr<CefBrowser> cefBrowser) {
			cefBrowser->ReloadIgnoreCache();
		},
		true);
}
#ifdef SHARED_TEXTURE_SUPPORT_ENABLED
#ifdef _WIN32
inline void BrowserSource::SignalBeginFrame()
{
	if (reset_frame) {
		ExecuteOnBrowser(
			[](CefRefPtr<CefBrowser> cefBrowser) {
				cefBrowser->GetHost()->SendExternalBeginFrame();
			},
			true);

		reset_frame = false;
	}
}
#endif
#endif

void BrowserSource::Update(obs_data_t *settings)
{
	if (settings) {
		bool n_is_local;
		int n_width;
		int n_height;
		bool n_fps_custom;
		int n_fps;
		bool n_shutdown;
		bool n_restart;
		bool n_reroute;
		std::string n_url;
		std::string n_css;

		n_is_local = obs_data_get_bool(settings, "is_local_file");
		n_width = (int)obs_data_get_int(settings, "width");
		n_height = (int)obs_data_get_int(settings, "height");
		n_fps_custom = obs_data_get_bool(settings, "fps_custom");
		n_fps = (int)obs_data_get_int(settings, "fps");
		n_shutdown = obs_data_get_bool(settings, "shutdown");
		n_restart = obs_data_get_bool(settings, "restart_when_active");
		n_css = obs_data_get_string(settings, "css");
		n_url = obs_data_get_string(settings,
					    n_is_local ? "local_file" : "url");
		n_reroute = obs_data_get_bool(settings, "reroute_audio");

		if (n_is_local && !n_url.empty()) {
			n_url = CefURIEncode(n_url, false);

#ifdef _WIN32
			size_t slash = n_url.find("%2F");
			size_t colon = n_url.find("%3A");

			if (slash != std::string::npos &&
			    colon != std::string::npos && colon < slash)
				n_url.replace(colon, 3, ":");
#endif

			while (n_url.find("%5C") != std::string::npos)
				n_url.replace(n_url.find("%5C"), 3, "/");

			while (n_url.find("%2F") != std::string::npos)
				n_url.replace(n_url.find("%2F"), 3, "/");

#if !ENABLE_LOCAL_FILE_URL_SCHEME
			/* http://absolute/ based mapping for older CEF */
			n_url = "http://absolute/" + n_url;
#elif defined(_WIN32)
			/* Widows-style local file URL:
			 * file:///C:/file/path.webm */
			n_url = "file:///" + n_url;
#else
			/* UNIX-style local file URL:
			 * file:///home/user/file.webm */
			n_url = "file://" + n_url;
#endif
		}

#if ENABLE_LOCAL_FILE_URL_SCHEME
		if (astrcmpi_n(n_url.c_str(), "http://absolute/", 16) == 0) {
			/* Replace http://absolute/ URLs with file://
			 * URLs if file:// URLs are enabled */
			n_url = "file:///" + n_url.substr(16);
			n_is_local = true;
		}
#endif

		if (n_is_local == is_local && n_width == width &&
		    n_height == height && n_fps_custom == fps_custom &&
		    n_fps == fps && n_shutdown == shutdown_on_invisible &&
		    n_restart == restart && n_css == css && n_url == url &&
		    n_reroute == reroute_audio) {
			return;
		}

		is_local = n_is_local;
		width = n_width;
		height = n_height;
		fps = n_fps;
		fps_custom = n_fps_custom;
		shutdown_on_invisible = n_shutdown;
		reroute_audio = n_reroute;
		restart = n_restart;
		css = n_css;
		url = n_url;

		obs_source_set_audio_active(source, reroute_audio);
	}

	DestroyBrowser(true);
	DestroyTextures();
#if CHROME_VERSION_BUILD < 4103 && CHROME_VERSION_BUILD >= 3683
	ClearAudioStreams();
#endif
	if (!shutdown_on_invisible || obs_source_showing(source))
		create_browser = true;

	first_update = false;
}

void BrowserSource::Tick()
{
	if (create_browser && CreateBrowser())
		create_browser = false;
#if defined(_WIN32) && defined(SHARED_TEXTURE_SUPPORT_ENABLED)
	if (!fps_custom)
		reset_frame = true;
#endif
}

extern void ProcessCef();

void BrowserSource::Render()
{
	bool flip = false;
#ifdef SHARED_TEXTURE_SUPPORT_ENABLED
	flip = hwaccel;
#endif

	if (texture) {
#ifdef __APPLE__
		gs_effect_t *effect = obs_get_base_effect(
			(hwaccel) ? OBS_EFFECT_DEFAULT_RECT
				  : OBS_EFFECT_PREMULTIPLIED_ALPHA);
#else
		gs_effect_t *effect =
			obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
#endif

		const bool previous = gs_set_linear_srgb(true);
		while (gs_effect_loop(effect, "Draw"))
			obs_source_draw(texture, 0, 0, 0, 0, flip);
		gs_set_linear_srgb(previous);
	}

#if defined(_WIN32) && defined(SHARED_TEXTURE_SUPPORT_ENABLED)
	SignalBeginFrame();
#elif USE_QT_LOOP
	ProcessCef();
#endif
}

static void ExecuteOnBrowser(BrowserFunc func, BrowserSource *bs)
{
	lock_guard<mutex> lock(browser_list_mutex);

	if (bs) {
		BrowserSource *bsw = reinterpret_cast<BrowserSource *>(bs);
		bsw->ExecuteOnBrowser(func, true);
	}
}

static void ExecuteOnAllBrowsers(BrowserFunc func)
{
	lock_guard<mutex> lock(browser_list_mutex);

	BrowserSource *bs = first_browser;
	while (bs) {
		BrowserSource *bsw = reinterpret_cast<BrowserSource *>(bs);
		bsw->ExecuteOnBrowser(func, true);
		bs = bs->next;
	}
}

void DispatchJSEvent(std::string eventName, std::string jsonString,
		     BrowserSource *browser)
{
	const auto jsEvent = [=](CefRefPtr<CefBrowser> cefBrowser) {
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		args->SetString(0, eventName);
		args->SetString(1, jsonString);
		SendBrowserProcessMessage(cefBrowser, PID_RENDERER, msg);
	};

	if (!browser)
		ExecuteOnAllBrowsers(jsEvent);
	else
		ExecuteOnBrowser(jsEvent, browser);
}
