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
#include "browser-app.hpp"
#include <functional>
#include <string>

#if CHROME_VERSION_BUILD < 4103 && CHROME_VERSION_BUILD >= 3683
#include <obs.hpp>
#include <unordered_map>
#include <vector>
#include <mutex>

struct AudioStream {
	OBSSource source;
	speaker_layout speakers;
	int channels;
	int sample_rate;
};
#endif

extern bool hwaccel;

struct BrowserSource {
	BrowserSource **p_prev_next = nullptr;
	BrowserSource *next = nullptr;

	obs_source_t *source = nullptr;

	bool tex_sharing_avail = false;
	bool create_browser = false;
	CefRefPtr<CefBrowser> cefBrowser;

	std::string url;
	std::string css;
	gs_texture_t *texture = nullptr;
	int width = 0;
	int height = 0;
	bool fps_custom = false;
	int fps = 0;
	bool restart = false;
	bool shutdown_on_invisible = false;
	bool is_local = false;
	bool first_update = true;
	bool reroute_audio = true;
#if defined(_WIN32) && defined(SHARED_TEXTURE_SUPPORT_ENABLED)
	bool reset_frame = false;
#endif
	bool is_showing = false;

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
	void ExecuteOnBrowser(BrowserFunc func, bool async = false);

	/* ---------------------------- */

	BrowserSource(obs_data_t *settings, obs_source_t *source);
	~BrowserSource();

	void Update(obs_data_t *settings = nullptr);
	void Tick();
	void Render();
#if CHROME_VERSION_BUILD < 4103 && CHROME_VERSION_BUILD >= 3683
	void ClearAudioStreams();
	void EnumAudioStreams(obs_source_enum_proc_t cb, void *param);
	bool AudioMix(uint64_t *ts_out, struct audio_output_data *audio_output,
		      size_t channels, size_t sample_rate);
	std::mutex audio_sources_mutex;
	std::vector<obs_source_t *> audio_sources;
	std::unordered_map<int, AudioStream> audio_streams;
#endif
	void SendMouseClick(const struct obs_mouse_event *event, int32_t type,
			    bool mouse_up, uint32_t click_count);
	void SendMouseMove(const struct obs_mouse_event *event,
			   bool mouse_leave);
	void SendMouseWheel(const struct obs_mouse_event *event, int x_delta,
			    int y_delta);
	void SendFocus(bool focus);
	void SendKeyClick(const struct obs_key_event *event, bool key_up);
	void SetShowing(bool showing);
	void SetActive(bool active);
	void Refresh();

#if defined(_WIN32) && defined(SHARED_TEXTURE_SUPPORT_ENABLED)
	inline void SignalBeginFrame();
#endif
};
