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

#include "browser-source.hpp"

static void
browser_source_get_defaults(obs_data_t settings)
{
	obs_data_set_default_string(settings, "url",
		"http://www.obsproject.com");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
}

static obs_properties_t
browser_source_get_properties()
{
	obs_properties_t props = obs_properties_create();

	obs_properties_add_text(props, "url",
		obs_module_text("URL"), OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "width",
		obs_module_text("Width"), 1, 4096, 1);
	obs_properties_add_int(props, "height",
		obs_module_text("Height"), 1, 4096, 1);
	obs_properties_add_int(props, "fps",
		obs_module_text("FPS"), 1, 60, 1);

	return props;
}

static void
browser_source_update(void *data, obs_data_t settings)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	bs->UpdateSettings(settings);
}

static uint32_t
browser_source_get_width(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	return bs->GetWidth();
}

static uint32_t
browser_source_get_height(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	return bs->GetHeight();
}


static const char *
browser_source_get_name(void)
{
	return obs_module_text("BrowserSource");
}


static void *
browser_source_create(obs_data_t settings, obs_source_t source)
{
	BrowserSource *browserSource = new BrowserSource(settings, source);

	return browserSource;
}

static void
browser_source_destroy(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);

	delete bs;
}


static void browser_source_render(void *data, gs_effect_t effect)
{

	UNUSED_PARAMETER(effect);

	BrowserSource *bs = static_cast<BrowserSource *>(data);

	if (bs->GetBrowserIdentifier() == 0) {
		bs->UpdateBrowser();
		return;
	}

	bs->RenderActiveTexture();

}

struct obs_source_info browser_source_info = {
	.id             = "browser_source",
	.type           = OBS_SOURCE_TYPE_INPUT,
#ifdef __APPLE__
	.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
#else
	.output_flags	= OBS_SOURCE_ASYNC_VIDEO
#endif
	.get_name       = browser_source_get_name,
	.create         = browser_source_create,
	.destroy        = browser_source_destroy,
	.update         = browser_source_update,
	.get_width      = browser_source_get_width,
	.get_height     = browser_source_get_height,
	.get_properties = browser_source_get_properties,
	.get_defaults	= browser_source_get_defaults,
	.video_render   = browser_source_render
};