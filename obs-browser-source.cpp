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
#include "wide-string.hpp"
#include <util/threading.h>
#include <functional>
#include <thread>
#include <mutex>

using namespace std;

extern bool QueueCEFTask(std::function<void()> task);

static mutex browser_list_mutex;
static BrowserSource *first_browser = nullptr;

BrowserSource::BrowserSource(obs_data_t *, obs_source_t *source_)
	: source(source_)
{
	/* defer update */
	obs_source_update(source, nullptr);

	lock_guard<mutex> lock(browser_list_mutex);
	p_prev_next = &first_browser;
	next = first_browser;
	if (first_browser) first_browser->p_prev_next = &next;
	first_browser = this;
}

BrowserSource::~BrowserSource()
{
	DestroyBrowser();
	DestroyTextures();

	lock_guard<mutex> lock(browser_list_mutex);
	if (next) next->p_prev_next = p_prev_next;
	*p_prev_next = next;
}

void BrowserSource::ExecuteOnBrowser(std::function<void()> func, bool async)
{
	if (!async) {
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&] () {
			if (!!cefBrowser)
				func();
			os_event_signal(finishedEvent);
		});
		if (success)
			os_event_wait(finishedEvent);
		os_event_destroy(finishedEvent);
	} else {
		QueueCEFTask([this, func] () {
			if (!!cefBrowser)
				func();
		});
	}
}

bool BrowserSource::CreateBrowser()
{
	return QueueCEFTask([this] ()
	{
#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
		if (hwaccel) {
			obs_enter_graphics();
			tex_sharing_avail = gs_shared_texture_available();
			obs_leave_graphics();
		}
#else
		bool hwaccel = false;
#endif

		struct obs_video_info ovi;
		obs_get_video_info(&ovi);

		CefRefPtr<BrowserClient> browserClient =
			new BrowserClient(this, hwaccel && tex_sharing_avail);

		CefWindowInfo windowInfo;
#if CHROME_VERSION_BUILD < 3071
		windowInfo.transparent_painting_enabled = true;
#endif
		windowInfo.width = width;
		windowInfo.height = height;
		windowInfo.windowless_rendering_enabled = true;

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
		windowInfo.shared_texture_enabled = hwaccel;
		windowInfo.external_begin_frame_enabled = true;
#endif

		CefBrowserSettings cefBrowserSettings;

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
		cefBrowserSettings.windowless_frame_rate = 0;
#else
		cefBrowserSettings.windowless_frame_rate = fps;
#endif

		cefBrowser = CefBrowserHost::CreateBrowserSync(
				windowInfo,
				browserClient,
				url,
				cefBrowserSettings,
				nullptr);
	});
}

void BrowserSource::DestroyBrowser(bool async)
{
	ExecuteOnBrowser([this] ()
	{
		CefRefPtr<CefClient> client =
				cefBrowser->GetHost()->GetClient();
		BrowserClient *bc =
				reinterpret_cast<BrowserClient*>(client.get());
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
		cefBrowser = nullptr;
	}, async);
}

void BrowserSource::SendMouseClick(
		const struct obs_mouse_event *event,
		int32_t type,
		bool mouse_up,
		uint32_t click_count)
{
	uint32_t modifiers = event->modifiers;
	int32_t  x         = event->x;
	int32_t  y         = event->y;

	ExecuteOnBrowser([this, modifiers, x, y, type, mouse_up, click_count] ()
	{
		CefMouseEvent e;
		e.modifiers = modifiers;
		e.x = x;
		e.y = y;
		CefBrowserHost::MouseButtonType buttonType =
			(CefBrowserHost::MouseButtonType)type;
		cefBrowser->GetHost()->SendMouseClickEvent(e, buttonType,
			mouse_up, click_count);
	}, true);
}

void BrowserSource::SendMouseMove(
		const struct obs_mouse_event *event,
		bool mouse_leave)
{
	uint32_t modifiers = event->modifiers;
	int32_t  x         = event->x;
	int32_t  y         = event->y;

	ExecuteOnBrowser([this, modifiers, x, y, mouse_leave] ()
	{
		CefMouseEvent e;
		e.modifiers = modifiers;
		e.x = x;
		e.y = y;
		cefBrowser->GetHost()->SendMouseMoveEvent(e, mouse_leave);
	}, true);
}

void BrowserSource::SendMouseWheel(
		const struct obs_mouse_event *event,
		int x_delta,
		int y_delta)
{
	uint32_t modifiers = event->modifiers;
	int32_t  x         = event->x;
	int32_t  y         = event->y;

	ExecuteOnBrowser([this, modifiers, x, y, x_delta, y_delta] ()
	{
		CefMouseEvent e;
		e.modifiers = modifiers;
		e.x = x;
		e.y = y;
		cefBrowser->GetHost()->SendMouseWheelEvent(e, x_delta, y_delta);
	}, true);
}

void BrowserSource::SendFocus(bool focus)
{
	ExecuteOnBrowser([this, focus] ()
	{
		cefBrowser->GetHost()->SendFocusEvent(focus);
	}, true);
}

void BrowserSource::SendKeyClick(
		const struct obs_key_event *event,
		bool key_up)
{
	uint32_t    modifiers   = event->modifiers;
	std::string text        = event->text;
	uint32_t    native_vkey = event->native_vkey;

	ExecuteOnBrowser([this, modifiers, text, native_vkey, key_up] ()
	{
		CefKeyEvent e;
		e.windows_key_code = native_vkey;
		e.native_key_code = 0;

		e.type = key_up ? KEYEVENT_KEYUP : KEYEVENT_RAWKEYDOWN;

		if (!text.empty()) {
			wstring wide = to_wide(text);
			if (wide.size())
				e.character = wide[0];
		}

		//e.native_key_code = native_vkey;
		e.modifiers = modifiers;

		cefBrowser->GetHost()->SendKeyEvent(e);
		if (!text.empty() && !key_up) {
			e.type = KEYEVENT_CHAR;
			e.windows_key_code = e.character;
			e.character = 0;
			cefBrowser->GetHost()->SendKeyEvent(e);
		}
	}, true);
}

void BrowserSource::SetShowing(bool showing)
{
	if (!showing) {
		ExecuteOnBrowser([this] ()
		{
			cefBrowser->GetHost()->WasHidden(true);
		}, true);
	}

	if (shutdown_on_invisible) {
		if (showing) {
			Update();
		} else {
			DestroyBrowser(true);
		}
	} else {
		ExecuteOnBrowser([this, showing] ()
		{
			CefRefPtr<CefProcessMessage> msg =
				CefProcessMessage::Create("Visibility");
			CefRefPtr<CefListValue> args = msg->GetArgumentList();
			args->SetBool(0, showing);
			cefBrowser->SendProcessMessage(PID_RENDERER, msg);
		}, true);
	}

	if (showing) {
		ExecuteOnBrowser([this] ()
		{
			cefBrowser->GetHost()->WasHidden(false);
			cefBrowser->GetHost()->Invalidate(PET_VIEW);
		}, true);
	}
}

void BrowserSource::SetActive(bool active)
{
	ExecuteOnBrowser([this, active] ()
	{
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("Active");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetBool(0, active);
		cefBrowser->SendProcessMessage(PID_RENDERER, msg);
	}, true);
}

void BrowserSource::Refresh()
{
	ExecuteOnBrowser([this] ()
	{
		cefBrowser->ReloadIgnoreCache();
	}, true);
}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
inline void BrowserSource::SignalBeginFrame()
{
	if (reset_frame) {
		ExecuteOnBrowser([this] ()
		{
			cefBrowser->GetHost()->SendExternalBeginFrame();
		}, true);

		reset_frame = false;
	}
}
#endif

void BrowserSource::Update(obs_data_t *settings)
{
	if (settings) {
		bool n_is_local;
		int n_width;
		int n_height;
		int n_fps;
		bool n_shutdown;
		bool n_restart;
		std::string n_url;
		std::string n_css;

		n_is_local  = obs_data_get_bool(settings, "is_local_file");
		n_width     = (int)obs_data_get_int(settings, "width");
		n_height    = (int)obs_data_get_int(settings, "height");
		n_fps       = (int)obs_data_get_int(settings, "fps");
		n_shutdown  = obs_data_get_bool(settings, "shutdown");
		n_restart   = obs_data_get_bool(settings, "restart_when_active");
		n_css       = obs_data_get_string(settings, "css");
		n_url       = obs_data_get_string(settings,
				n_is_local ? "local_file" : "url");

		if (n_is_local == is_local &&
		    n_width == width &&
		    n_height == height &&
		    n_fps == fps &&
		    n_shutdown == shutdown_on_invisible &&
		    n_restart == restart &&
		    n_css == css &&
		    n_url == url) {
			return;
		}

		is_local              = n_is_local;
		width                 = n_width;
		height                = n_height;
		fps                   = n_fps;
		shutdown_on_invisible = n_shutdown;
		restart               = n_restart;
		css                   = n_css;
		url                   = n_url;
	}

	DestroyBrowser(true);
	DestroyTextures();

	if (!shutdown_on_invisible || obs_source_showing(source))
		create_browser = true;
}

void BrowserSource::Tick()
{
	if (create_browser && CreateBrowser())
		create_browser = false;
#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	reset_frame = true;
#endif
}

void BrowserSource::Render()
{
	bool flip = false;
#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	flip = hwaccel;
#endif

	if (texture) {
		gs_effect_t *effect = obs_get_base_effect(
				OBS_EFFECT_PREMULTIPLIED_ALPHA);
		while (gs_effect_loop(effect, "Draw"))
			obs_source_draw(texture, 0, 0, 0, 0, flip);
	}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	SignalBeginFrame();
#endif
}

static void ExecuteOnAllBrowsers(function<void(BrowserSource*)> func)
{
	lock_guard<mutex> lock(browser_list_mutex);

	BrowserSource *bs = first_browser;
	while (bs) {
		BrowserSource *bsw =
			reinterpret_cast<BrowserSource *>(bs);
		bsw->ExecuteOnBrowser([&] () {func(bsw);});
		bs = bs->next;
	}
}

void DispatchJSEvent(const char *eventName, const char *jsonString)
{
	ExecuteOnAllBrowsers([&] (BrowserSource *bsw)
	{
		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		args->SetString(0, eventName);
		args->SetString(1, jsonString);
		bsw->cefBrowser->SendProcessMessage(PID_RENDERER, msg);
	});
}
