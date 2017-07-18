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

#include "browser-source.hpp"
#include "browser-manager.hpp"
#include "browser-listener.hpp"
#include "browser-settings.hpp"

void BrowserSource::UpdateSettings(obs_data_t *settings)
{
	isLocalFile = obs_data_get_bool(settings, "is_local_file");
	url = obs_data_get_string(settings, isLocalFile ? "local_file" : "url");
	css = obs_data_get_string(settings, "css");
	width = (uint32_t)obs_data_get_int(settings, "width");
	height = (uint32_t)obs_data_get_int(settings, "height");
	fps = (uint32_t)obs_data_get_int(settings, "fps");
	shutdown = obs_data_get_bool(settings, "shutdown");

	UpdateBrowser();
}

void BrowserSource::UpdateBrowser()
{
	if (browserIdentifier != 0) {
		LockTexture();
		BrowserManager::Instance()->DestroyBrowser(browserIdentifier);
		InvalidateActiveTexture();
		browserIdentifier = 0;
		UnlockTexture();
	}

	std::shared_ptr<BrowserListener> browserListener(CreateListener());

	BrowserSettings browserSettings;

	browserSettings.url = isLocalFile? "http://absolute/"+ url : url,
	browserSettings.width = width;
	browserSettings.height = height;
	browserSettings.fps = fps;
	browserSettings.css = css;

	browserIdentifier = BrowserManager::Instance()->CreateBrowser(
			browserSettings, browserListener);
}

void BrowserSource::SendMouseClick(const struct obs_mouse_event *event,
		int32_t type, bool mouseUp, uint32_t clickCount)
{

	BrowserManager::Instance()->SendMouseClick(browserIdentifier,
			event, type, mouseUp, clickCount);
}

void BrowserSource::SendMouseMove(const struct obs_mouse_event *event,
		bool mouseLeave)
{
	BrowserManager::Instance()->SendMouseMove(browserIdentifier,
			event, mouseLeave);
}

void BrowserSource::SendMouseWheel(const struct obs_mouse_event *event,
		int xDelta, int yDelta)
{
	BrowserManager::Instance()->SendMouseWheel(browserIdentifier,
			event, xDelta, yDelta);
}

void BrowserSource::SendFocus(bool focus)
{
	BrowserManager::Instance()->SendFocus(browserIdentifier, focus);
}

void BrowserSource::SendKeyClick(const struct obs_key_event *event, bool keyUp)
{
	BrowserManager::Instance()->SendKeyClick(browserIdentifier, event,
			keyUp);
}

void BrowserSource::ExecuteVisiblityJSCallback(bool visible)
{
	BrowserManager::Instance()->ExecuteVisiblityJSCallback(browserIdentifier, visible);
}

void BrowserSource::ExecuteActiveJSCallback(bool active)
{
	BrowserManager::Instance()->ExecuteActiveJSCallback(browserIdentifier, active);
}
