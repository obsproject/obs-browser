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

#include "browser-client.hpp"
#include "obs-browser-source.hpp"
#include "base64/base64.hpp"
#include "json11/json11.hpp"

#if BROWSER_FRONTEND_API_SUPPORT_ENABLED
#include <obs-frontend-api.h>
#endif

using namespace json11;

BrowserClient::~BrowserClient()
{
#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	if (sharing_available) {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		obs_leave_graphics();
	}
#endif
}

CefRefPtr<CefLoadHandler> BrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> BrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> BrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefContextMenuHandler> BrowserClient::GetContextMenuHandler()
{
	return this;
}

bool BrowserClient::OnBeforePopup(
		CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame>,
		const CefString &,
		const CefString &,
		WindowOpenDisposition,
		bool,
		const CefPopupFeatures &,
		CefWindowInfo &,
		CefRefPtr<CefClient> &,
		CefBrowserSettings&,
		bool *)
{
	/* block popups */
	return true;
}

void BrowserClient::OnBeforeContextMenu(
		CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame>,
		CefRefPtr<CefContextMenuParams>,
		CefRefPtr<CefMenuModel> model)
{
	/* remove all context menu contributions */
	model->Clear();
}

bool BrowserClient::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId,
	CefRefPtr<CefProcessMessage> message)
{
	const std::string &name = message->GetName();
	Json json;

	if (!bs) {
		return false;
	}

	if (name == "getCurrentScene") {
		json = Json::object {
			{"name", obs_source_get_name(bs->source)},
			{"width", (int)obs_source_get_width(bs->source)},
			{"height", (int)obs_source_get_height(bs->source)}
		};

	}
#if BROWSER_FRONTEND_API_SUPPORT_ENABLED
	else if (name == "getStatus") {
		json = Json::object {
			{"recording", obs_frontend_recording_active()},
			{"streaming", obs_frontend_streaming_active()},
			{"replaybuffer", obs_frontend_replay_buffer_active()}
		};

	}
#endif
	else {
		return false;
	}

	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("executeCallback");

	CefRefPtr<CefListValue> args = msg->GetArgumentList();
	args->SetInt(0, message->GetArgumentList()->GetInt(0));
	args->SetString(1, json.dump());

	browser->SendProcessMessage(PID_RENDERER, msg);

	return true;
}

bool BrowserClient::GetViewRect(
		CefRefPtr<CefBrowser>,
		CefRect &rect)
{
	if (!bs) {
		return false;
	}

	rect.Set(0, 0, bs->width, bs->height);
	return true;
}

void BrowserClient::OnPaint(
		CefRefPtr<CefBrowser>,
		PaintElementType type,
		const RectList &,
		const void *buffer,
		int width,
		int height)
{
	if (type != PET_VIEW) {
		return;
	}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	if (sharing_available) {
		return;
	}
#endif

	if (!bs) {
		return;
	}

	if (bs->width != width || bs->height != height) {
		obs_enter_graphics();
		bs->DestroyTextures();
		obs_leave_graphics();
	}

	if (!bs->texture && width && height) {
		obs_enter_graphics();
		bs->texture = gs_texture_create(
				width, height, GS_BGRA, 1,
				(const uint8_t **)&buffer,
				GS_DYNAMIC);
		bs->width = width;
		bs->height = height;
		obs_leave_graphics();
	} else {
		obs_enter_graphics();
		gs_texture_set_image(bs->texture,
				(const uint8_t *)buffer,
				width * 4, false);
		obs_leave_graphics();
	}
}

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
void BrowserClient::OnAcceleratedPaint(
		CefRefPtr<CefBrowser>,
		PaintElementType,
		const RectList &,
		void *shared_handle)
{
	if (!bs) {
		return;
	}

	if (shared_handle != last_handle) {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		gs_texture_destroy(bs->texture);
		bs->texture = nullptr;
		texture = nullptr;

		texture = gs_texture_open_shared(
				(uint32_t)(uintptr_t)shared_handle);

		uint32_t cx = gs_texture_get_width(texture);
		uint32_t cy = gs_texture_get_height(texture);
		gs_color_format format = gs_texture_get_color_format(texture);

		bs->texture = gs_texture_create(cx, cy, format, 1, nullptr, 0);
		obs_leave_graphics();

		last_handle = shared_handle;
	}

	if (texture && bs->texture) {
		obs_enter_graphics();
		gs_copy_texture(bs->texture, texture);
		obs_leave_graphics();
	}
}
#endif

void BrowserClient::OnLoadEnd(
		CefRefPtr<CefBrowser>,
		CefRefPtr<CefFrame> frame,
		int)
{
	if (!bs) {
		return;
	}

	if (frame->IsMain()) {
		std::string base64EncodedCSS = base64_encode(bs->css);

		std::string href;
		href += "data:text/css;charset=utf-8;base64,";
		href += base64EncodedCSS;

		std::string script;
		script += "var link = document.createElement('link');";
		script += "link.setAttribute('rel', 'stylesheet');";
		script += "link.setAttribute('type', 'text/css');";
		script += "link.setAttribute('href', '" + href + "');";
		script += "document.getElementsByTagName('head')[0].appendChild(link);";

		frame->ExecuteJavaScript(script, href, 0);
	}
}

bool BrowserClient::OnConsoleMessage(CefRefPtr<CefBrowser>,
#if CHROME_VERSION_BUILD >= 3282
		cef_log_severity_t level,
#endif
		const CefString &message,
		const CefString &source,
		int line)
{
#if CHROME_VERSION_BUILD >= 3282
	if (level < LOGSEVERITY_ERROR)
		return false;
#endif

	blog(LOG_INFO, "obs-browser: %s (source: %s:%d)",
			message.ToString().c_str(),
			source.ToString().c_str(),
			line);
	return false;
}
