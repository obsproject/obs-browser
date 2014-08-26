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

#include <map>

#include <obs-module.h>

#include "cef-logging.h"

#include "browser-manager.hpp"
#include "browser-listener.hpp"
#include "browser-source.hpp"
#include "browser-settings.hpp"

class TextureRef {
public:
	TextureRef(const IOSurfaceRef ioSurfaceRef, const gs_texture_t texture)
	: ioSurfaceRef(ioSurfaceRef), texture(texture)
	{
		IOSurfaceIncrementUseCount(ioSurfaceRef);
	}

	~TextureRef() {
		gs_texture_destroy(texture);
		IOSurfaceDecrementUseCount(ioSurfaceRef);
		CFRelease(ioSurfaceRef);
	}

public:
	gs_texture_t GetTexture() const { return texture; }
	IOSurfaceRef GetSurfaceRef() const { return ioSurfaceRef; }

private:
	const IOSurfaceRef ioSurfaceRef;
	const gs_texture_t texture;
};

class BrowserSourceListener : public BrowserListener
{
public:
	BrowserSourceListener(BrowserSource * const browserSource)
	: browserSource(browserSource)
	{}
	
	~BrowserSourceListener()
	{}

	void OnDraw(
		const int surfaceHandle,
		const int width,
		const int height) override
	{
		UNUSED_PARAMETER(width);
		UNUSED_PARAMETER(height);

		if (textureMap.count(surfaceHandle) > 0) {


			obs_enter_graphics();
			browserSource->LockTexture();
			if (gs_texture_rebind_iosurface(
				textureMap[surfaceHandle]->GetTexture(),
				textureMap[surfaceHandle]->GetSurfaceRef()))
			{
				browserSource->SetCurrentRenderTexture(
					textureMap[surfaceHandle].get());
			} else {
				browserSource->SetCurrentRenderTexture(nullptr);
			}
			browserSource->UnlockTexture();
			obs_leave_graphics();

		} else {
			CEFLogError(@"Asked to draw unknown surface with handle"
				    "%d", surfaceHandle);
		}
	}

	bool CreateSurface(
		const int width,
		const int height,
		int * const surfaceHandle) override
	{
		unsigned pixelFormat = 'BGRA';
		unsigned bytesPerElement = 4;

		unsigned long bytesPerRow = width * bytesPerElement;

		if (!bytesPerRow) {
			return 0;
		}

		unsigned long allocSize = height * bytesPerRow;

		if (!allocSize) {
			return 0;
		}

		CFDictionaryRef dictionaryRef =
			(CFDictionaryRef)
				[NSDictionary dictionaryWithObjectsAndKeys:
				[NSNumber numberWithInt: width],
				(id)kIOSurfaceWidth,
				[NSNumber numberWithInt: height],
				(id)kIOSurfaceHeight,
				[NSNumber numberWithInt:bytesPerElement],
				(id)kIOSurfaceBytesPerElement,
				[NSNumber numberWithBool:YES],
				(id)kIOSurfaceIsGlobal,
				[NSNumber numberWithInt: pixelFormat],
				(id)kIOSurfacePixelFormat,
				[NSNumber numberWithLong: bytesPerRow],
				(id)kIOSurfaceBytesPerRow,
				[NSNumber numberWithLong: allocSize],
				(id)kIOSurfaceAllocSize,
				nil];

		IOSurfaceRef ioSurfaceRef = IOSurfaceCreate(dictionaryRef);

		if (ioSurfaceRef == nullptr) {
			return false;
		}

		obs_enter_graphics();
		gs_texture_t ioSurfaceTex =
			gs_texture_create_from_iosurface(ioSurfaceRef);

		obs_leave_graphics();

		if (ioSurfaceTex == nullptr) {
			CFRelease(ioSurfaceRef);
			return false;
		}

		int ioSurfaceHandle = IOSurfaceGetID(ioSurfaceRef);

		std::shared_ptr<TextureRef> textureRef(
			new TextureRef(ioSurfaceRef, ioSurfaceTex));

		textureMap[ioSurfaceHandle] = textureRef;

		*surfaceHandle = ioSurfaceHandle;

		return true;
	}

	void DestroySurface(const int surfaceHandle) override
	{
		if (textureMap.count(surfaceHandle) == 1) {
			obs_enter_graphics();
			textureMap.erase(surfaceHandle);
			obs_leave_graphics();
		}
	}

private:
	BrowserSource * const browserSource;
	std::map<int, std::shared_ptr<TextureRef>> textureMap;
};

BrowserSource::BrowserSource(obs_data_t settings, obs_source_t source)
: source(source), hasValidBrowserIdentifier(false),
	currentRenderTexture(nullptr)
{
	pthread_mutex_init(&textureLock, NULL);
	UpdateSettings(settings);

}

BrowserSource::~BrowserSource()
{
	if (hasValidBrowserIdentifier) {
		BrowserManager::Instance()->DestroyBrowser(browserIdentifier);
	}

	hasValidBrowserIdentifier = false;
	pthread_mutex_destroy(&textureLock);
}

TextureRef *
BrowserSource::GetCurrentRenderTexture() const
{
	return currentRenderTexture;
}

void
BrowserSource::SetCurrentRenderTexture(TextureRef *texture)
{
	currentRenderTexture = texture;
}

static inline
void build_sprite_rect(struct gs_vb_data *data, float origin_x, float origin_y,
	float end_x, float end_y);


void
BrowserSource::UpdateBrowser()
{
	if (hasValidBrowserIdentifier) {
		obs_enter_graphics();
		LockTexture();
		BrowserManager::Instance()->DestroyBrowser(browserIdentifier);
		SetCurrentRenderTexture(nullptr);
		UnlockTexture();
		obs_leave_graphics();
	}

	hasValidBrowserIdentifier = false;

	std::shared_ptr<BrowserListener> browserListener(
		new BrowserSourceListener(this));

	BrowserSettings browserSettings {
		.url = url,
		.width = width,
		.height = height,
		.fps = fps
	};

	browserIdentifier = BrowserManager::Instance()->CreateBrowser(
		browserSettings, browserListener);

	if (browserIdentifier > 0)
	{
		hasValidBrowserIdentifier = true;
	}
}


void
BrowserSource::UpdateSettings(obs_data_t settings)
{
	url = obs_data_get_string(settings, "url");
	width = obs_data_get_int(settings, "width");
	height = obs_data_get_int(settings, "height");
	fps = obs_data_get_int(settings, "fps");

	UpdateBrowser();
}

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



gs_vertbuffer_t
create_vertex_buffer()
{
	struct gs_vb_data *vb_data = gs_vbdata_create();

	vb_data->num = 4;
	vb_data->points = (struct vec3 *)bzalloc(sizeof(struct vec3) * 4);
	if (!vb_data->points) {
		gs_vbdata_destroy(vb_data);
		return nullptr;
	}

	vb_data->num_tex = 1;
	vb_data->tvarray = (struct gs_tvertarray *)
	bzalloc(sizeof(struct gs_tvertarray));

	if (!vb_data->tvarray) {
		gs_vbdata_destroy(vb_data);
		return nullptr;
	}

	vb_data->tvarray[0].width = 2;
	vb_data->tvarray[0].array = bzalloc(sizeof(struct vec2) * 4);

	if (!vb_data->tvarray[0].array) {
		gs_vbdata_destroy(vb_data);
		return nullptr;
	}

	return gs_vertexbuffer_create(vb_data, GS_DYNAMIC);
}

static void
build_sprite(
	    struct gs_vb_data *data,
	    float fcx,
	    float fcy,
	    float start_u,
	    float end_u,
	    float start_v,
	    float end_v)
{
	struct vec2 *tvarray = (struct vec2 *)data->tvarray[0].array;

	vec3_set(data->points+1,  fcx, 0.0f, 0.0f);
	vec3_set(data->points+2, 0.0f,  fcy, 0.0f);
	vec3_set(data->points+3,  fcx,  fcy, 0.0f);
	vec2_set(tvarray,   start_u, start_v);
	vec2_set(tvarray+1, end_u,   start_v);
	vec2_set(tvarray+2, start_u, end_v);
	vec2_set(tvarray+3, end_u,   end_v);
}

static inline
void build_sprite_rect(
		     struct gs_vb_data *data,
		     float origin_x,
		     float origin_y,
		     float end_x,
		     float end_y)
{
	build_sprite(data, fabs(end_x - origin_x), fabs(end_y - origin_y),
		    origin_x, end_x, origin_y, end_y);
}


static void *
browser_source_create(obs_data_t settings, obs_source_t source)
{
	BrowserSource *browserSource = new BrowserSource(settings, source);

	obs_enter_graphics();
	char *drawEffectFile = obs_module_file("draw_rect.effect");
	browserSource->SetDrawEffect(
		gs_effect_create_from_file(drawEffectFile, NULL));
	bfree(drawEffectFile);

	struct gs_sampler_info info = {
		.filter = GS_FILTER_LINEAR,
		.address_u = GS_ADDRESS_CLAMP,
		.address_v = GS_ADDRESS_CLAMP,
		.address_w = GS_ADDRESS_CLAMP,
		.max_anisotropy = 1,
	};
	browserSource->SetSampler(gs_samplerstate_create(&info));
	browserSource->SetVertexBuffer(create_vertex_buffer());

	obs_leave_graphics();

	return browserSource;
}

static void
browser_source_destroy(void *data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	obs_enter_graphics();
	if (bs->GetVertexBuffer() != nullptr) {
		gs_vertexbuffer_destroy(bs->GetVertexBuffer());
		bs->SetVertexBuffer(nullptr);
	}
	if (bs->GetSampler() != nullptr) {
		gs_samplerstate_destroy(bs->GetSampler());
		bs->SetSampler(nullptr);
	}
	if (bs->GetDrawEffect() != nullptr) {
		gs_effect_destroy(bs->GetDrawEffect());
		bs->SetDrawEffect(nullptr);
	}
	delete bs;
	obs_leave_graphics();
}


static void browser_source_render(void *data, gs_effect_t effect)
{

	UNUSED_PARAMETER(effect);

	BrowserSource *bs = static_cast<BrowserSource *>(data);

	// if we dont have a browser, try to fetch one
	// ignore race conditions for now /o/
	if (!bs->HasValidBrowserIdentifier()) {
		bs->UpdateBrowser();
		return;
	}

	bs->LockTexture();

	if (bs->GetCurrentRenderTexture() != nullptr) {
		gs_vertexbuffer_flush(bs->GetVertexBuffer());
		gs_load_vertexbuffer(bs->GetVertexBuffer());
		gs_load_indexbuffer(NULL);
		gs_load_samplerstate(bs->GetSampler(), 0);

		gs_technique_t tech = gs_effect_get_technique(
			bs->GetDrawEffect(), "Default");
		gs_effect_set_texture(gs_effect_get_param_by_idx(
			bs->GetDrawEffect(), 1),
			bs->GetCurrentRenderTexture()->GetTexture());
		gs_technique_begin(tech);
		gs_technique_begin_pass(tech, 0);
		gs_draw(GS_TRISTRIP, 0, 4);

		gs_technique_end_pass(tech);
		gs_technique_end(tech);
	}

	bs->UnlockTexture();
}

static void browser_capture_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	BrowserSource *bs = static_cast<BrowserSource *>(data);

	obs_enter_graphics();
	bs->LockTexture();

	build_sprite_rect(
		gs_vertexbuffer_get_data(bs->GetVertexBuffer()),
		0, 0, bs->GetWidth(), bs->GetHeight());


	bs->UnlockTexture();
	obs_leave_graphics();

}

struct obs_source_info browser_source_info = {
	.id             = "browser_source",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name       = browser_source_get_name,
	.create         = browser_source_create,
	.destroy        = browser_source_destroy,
	.update         = browser_source_update,
	.get_width      = browser_source_get_width,
	.get_height     = browser_source_get_height,
	.get_properties = browser_source_get_properties,
	.get_defaults	= browser_source_get_defaults,
	.video_render   = browser_source_render,
	.video_tick     = browser_capture_video_tick
};