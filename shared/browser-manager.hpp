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
#include <jansson.h>

class BrowserListener;
struct BrowserSettings;

class BrowserManager {

public:

	static BrowserManager *Instance();
	static void DestroyInstance();
	
public:
	BrowserManager();
	~BrowserManager();

	void Startup();
	void Shutdown();
	void Restart();

	int CreateBrowser(const BrowserSettings &browserSettings,
			const std::shared_ptr<BrowserListener>
				&browserListener);

	void DestroyBrowser(int browserIdentifier);

	void TickBrowser(int browserIdentifier);
	void SendMouseClick(int browserIdentifier,
			const struct obs_mouse_event *event, int32_t type,
			bool mouse_up, uint32_t click_count);
	void SendMouseMove(int browserIdentifier,
			const struct obs_mouse_event *event, bool mouseLeave);
	void SendMouseWheel(int browserIdentifier,
			const struct obs_mouse_event *event, int xDelta,
			int yDelta);
	void SendFocus(int browserIdentifier, bool focus);
	void SendKeyClick(int browserIdentifier,
			const struct obs_key_event *event, bool keyUp);

	void SetModulePath(const char *path) { this->path = path; }
	const char *GetModulePath() { return path; }

	void ExecuteVisiblityJSCallback(int browserIdentifier, bool visible);

	void ExecuteActiveJSCallback(int browserIdentifier, bool active);
	
	void ExecuteSceneChangeJSCallback(const char *name);

	void RefreshPageNoCache(int browserIdentifier);

	void DispatchJSEvent(const char *eventName, const char *jsonString);

private:
	class Impl;
	std::unique_ptr<Impl> pimpl;
	const char *path;
};
