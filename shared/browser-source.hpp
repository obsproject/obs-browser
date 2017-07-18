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

#include <mutex>

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
	inline const std::string& GetUrl() const { return url; }
	inline const std::string& GetCss() const { return css; }
	inline uint32_t GetWidth() const { return width; }
	inline uint32_t GetHeight() const { return height; }
	inline obs_source_t *GetSource() const { return source; }
	inline const bool GetShutdown() const { return shutdown; }

	inline int GetBrowserIdentifier() const { return browserIdentifier; }

	inline void LockTexture() { textureLock.lock(); }
	inline bool TryLockTexture() { return textureLock.try_lock(); }
	inline void UnlockTexture() { textureLock.unlock(); }

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

	void ExecuteVisiblityJSCallback(bool visible);
	void ExecuteActiveJSCallback(bool active);

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
	bool restart_when_active;

	int browserIdentifier;

	std::mutex textureLock;
};
