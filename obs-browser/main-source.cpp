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
#include <util/dstr.h>

#include "browser-source.hpp"
#include "browser-manager.hpp"

static void browser_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "url",
			"http://www.obsproject.com");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_string(settings, "css", "body { background-color: rgba(0, 0, 0, 0); margin: 0px auto; overflow: hidden; }");
}

static bool restart_button_clicked(obs_properties_t *props,
		obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	UNUSED_PARAMETER(data);

	BrowserManager::Instance()->Restart();
	return true;
}

static bool refreshnocache_button_clicked(obs_properties_t *props,
	obs_property_t *property, void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	BrowserManager::Instance()->RefreshPageNoCache(bs->GetBrowserIdentifier());
	
	return true;
}

static bool is_local_file_modified(obs_properties_t *props,
		obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

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
	struct dstr path = { 0 };
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);
	// use this when obs allows non-readonly paths
	obs_property_t *prop = obs_properties_add_bool(props, "is_local_file",
			obs_module_text("LocalFile"));

	if (bs && bs->GetUrl() != "")
	{
		const char *slash;

		dstr_copy(&path, bs->GetUrl().c_str());
		dstr_replace(&path, "\\", "/");
		slash = strrchr(path.array, '/');
		if (slash)
			dstr_resize(&path, slash - path.array + 1);
	}


	obs_property_set_modified_callback(prop, is_local_file_modified);
	obs_properties_add_path(props, "local_file",
			obs_module_text("Local file"), OBS_PATH_FILE, "*.*",
			path.array);
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
		obs_module_text("RefreshNoCache"), refreshnocache_button_clicked);

#ifdef __APPLE__
	// osx is the only process-isolated cef impl
	obs_properties_add_button(props, "restart",
			obs_module_text("Restart CEF"), restart_button_clicked);
#endif
	return props;
}

static void browser_source_update(void *data, obs_data_t *settings)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->UpdateSettings(settings);
}

static uint32_t browser_source_get_width(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	return bs->GetWidth();
}

static uint32_t browser_source_get_height(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	return bs->GetHeight();
}


static const char *browser_source_get_name(void *)
{
	return obs_module_text("BrowserSource");
}

static void browser_source_activate(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	obs_data_t *settings = obs_source_get_settings(bs->GetSource());
	bool restart = obs_data_get_bool(settings, "restart_when_active");

	if (restart)
		BrowserManager::Instance()->RefreshPageNoCache(bs->GetBrowserIdentifier());

	bs->ExecuteActiveJSCallback(true);
}

static void browser_source_deactivate(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	bs->ExecuteActiveJSCallback(false);
}

// Called when the source is visible
static void browser_source_show(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	if ( bs->GetShutdown() ) {
		bs->UpdateBrowser();
	}
	else {
		bs->ExecuteVisiblityJSCallback(true);
	}
}

// Called when the source is no longer visible
static void browser_source_hide(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	if (bs->GetShutdown()) {
		BrowserManager::Instance()->DestroyBrowser(bs->GetBrowserIdentifier());
	}
	else {
		bs->ExecuteVisiblityJSCallback(false);
	}
}

static void *browser_source_create(obs_data_t *settings, obs_source_t *source)
{
	BrowserSource *browserSource = new BrowserSource(settings, source);

	if (browserSource->GetShutdown() && !obs_source_showing(source))
		BrowserManager::Instance()->DestroyBrowser(browserSource->GetBrowserIdentifier());

	return browserSource;
}

static void browser_source_destroy(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	delete bs;
}


static void browser_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	BrowserSource *bs = static_cast<BrowserSource *>(data);

	if (bs->GetBrowserIdentifier() == 0) {
		bs->UpdateBrowser();
		return;
	}

	bs->RenderActiveTexture(effect);
}

static void browser_source_mouse_click(void *data,
		const struct obs_mouse_event *event, int32_t type,
		bool mouse_up, uint32_t click_count)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->SendMouseClick(event, type, mouse_up, click_count);

	blog(LOG_DEBUG,
		"mouse_click x:%d y:%d mouse_up:'%s' count:'%d'", event->x, 
		event->y, mouse_up ? "true" : "false", click_count);
}

static void browser_source_mouse_move(void *data,
		const struct obs_mouse_event *event, bool mouse_leave)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->SendMouseMove(event, mouse_leave);
}

static void browser_source_mouse_wheel(void *data,
		const struct obs_mouse_event *event, int x_delta, int y_delta)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->SendMouseWheel(event, x_delta, y_delta);
}

static void browser_source_focus(void *data, bool focus)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->SendFocus(focus);
}

static void browser_source_key_click(void *data,
		const struct obs_key_event *event, bool key_up)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->SendKeyClick(event, key_up);
}

struct obs_source_info
create_browser_source_info() 
{
	struct obs_source_info browser_source_info = { 0 };

	browser_source_info.id = "browser_source";
	browser_source_info.type = OBS_SOURCE_TYPE_INPUT;
	browser_source_info.output_flags = OBS_SOURCE_VIDEO |
			OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_CUSTOM_DRAW;
	// interaction
	browser_source_info.mouse_click = browser_source_mouse_click;
	browser_source_info.mouse_move = browser_source_mouse_move;
	browser_source_info.mouse_wheel = browser_source_mouse_wheel;
	browser_source_info.focus = browser_source_focus;
	browser_source_info.key_click = browser_source_key_click;

	browser_source_info.get_name = browser_source_get_name;
	browser_source_info.create = browser_source_create;
	browser_source_info.destroy = browser_source_destroy;
	browser_source_info.update = browser_source_update;
	browser_source_info.get_width = browser_source_get_width;
	browser_source_info.get_height = browser_source_get_height;
	browser_source_info.get_properties = browser_source_get_properties;
	browser_source_info.get_defaults = browser_source_get_defaults;
	browser_source_info.video_render = browser_source_render;
	browser_source_info.show = browser_source_show;
	browser_source_info.hide = browser_source_hide;
	browser_source_info.activate = browser_source_activate;
	browser_source_info.deactivate = browser_source_deactivate;


	return browser_source_info;
}