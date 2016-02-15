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

#pragma once

#include <memory>

#include <stdint.h>
#include <string>

#include <pthread.h>

#include <obs-module.h>
#include <util/platform.h>
#include <obs-output.h>

class TextureRef;
class BrowserListener;

class BrowserSource {

public:
	BrowserSource(obs_data_t *settings, obs_source_t *source);
	~BrowserSource();

public:
	void UpdateSettings(obs_data_t *settings);
	void UpdateBrowser();

public:
	const std::string& GetUrl() const { return url; }
	const std::string& GetCss() const { return css; }
	uint32_t GetWidth() const { return width; }
	uint32_t GetHeight() const { return height; }
	obs_source_t *GetSource() const { return source; }
	const bool GetShutdown() const { return shutdown; }

	int GetBrowserIdentifier() const { return browserIdentifier; }

	void LockTexture() { pthread_mutex_lock(&textureLock); }
	void UnlockTexture() { pthread_mutex_unlock(&textureLock); }

	void RenderActiveTexture(gs_effect_t *effect);
	void InvalidateActiveTexture();

	void SendMouseClick(const struct obs_mouse_event *event,
			int32_t type, bool mouseUp, uint32_t clickCount);
	void SendMouseMove(const struct obs_mouse_event *event,
			bool mouseLeave);
	void SendMouseWheel(const struct obs_mouse_event *event,
			int xDelta, int yDelta);
	void SendFocus(bool focus);
	void SendKeyClick(const struct obs_key_event *event, bool keyUp);
	std::shared_ptr<BrowserListener> CreateListener();


private:
	class Impl;
	std::unique_ptr<Impl> pimpl;

	obs_source_t *source;
	bool isLocalFile;
	std::string url;
	std::string css;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	bool shutdown;

	int browserIdentifier;

	pthread_mutex_t textureLock;

};
