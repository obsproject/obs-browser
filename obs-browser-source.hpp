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

#pragma once

#include <obs-module.h>

#include "cef-headers.hpp"
#include "browser-config.h"

#include <functional>
#include <string>

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
extern bool hwaccel;
#endif

struct BrowserSource {
	BrowserSource         **p_prev_next            = nullptr;
	BrowserSource         *next                    = nullptr;

	obs_source_t          *source                  = nullptr;

	bool                  tex_sharing_avail        = false;
	bool                  create_browser           = false;
	CefRefPtr<CefBrowser> cefBrowser;

	std::string           url;
	std::string           css;
	gs_texture_t          *texture                 = nullptr;
	int                   width                    = 0;
	int                   height                   = 0;
	int                   fps                      = 0;
	bool                  restart                  = false;
	bool                  shutdown_on_invisible    = false;
	bool                  is_local                 = false;
#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	bool                  reset_frame              = false;
#endif

	inline void DestroyTextures()
	{
		if (texture) {
			obs_enter_graphics();
			gs_texture_destroy(texture);
			texture = nullptr;
			obs_leave_graphics();
		}
	}

	/* ---------------------------- */

	bool CreateBrowser();
	void DestroyBrowser(bool async = false);
	void ExecuteOnBrowser(std::function<void()> func, bool async = false);

	/* ---------------------------- */

	BrowserSource(obs_data_t *settings, obs_source_t *source);
	~BrowserSource();

	void Update(obs_data_t *settings = nullptr);
	void Tick();
	void Render();
	void SendMouseClick(
			const struct obs_mouse_event *event,
			int32_t type,
			bool mouse_up,
			uint32_t click_count);
	void SendMouseMove(
			const struct obs_mouse_event *event,
			bool mouse_leave);
	void SendMouseWheel(
			const struct obs_mouse_event *event,
			int x_delta,
			int y_delta);
	void SendFocus(bool focus);
	void SendKeyClick(
			const struct obs_key_event *event,
			bool key_up);
	void SetShowing(bool showing);
	void SetActive(bool active);
	void Refresh();

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
	inline void SignalBeginFrame();
#endif
};
