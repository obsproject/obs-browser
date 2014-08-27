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

void
BrowserSource::UpdateSettings(obs_data_t settings)
{
	url = obs_data_get_string(settings, "url");
	width = obs_data_get_int(settings, "width");
	height = obs_data_get_int(settings, "height");
	fps = obs_data_get_int(settings, "fps");

	UpdateBrowser();
}

void
BrowserSource::UpdateBrowser()
{
	if (browserIdentifier != 0) {
		obs_enter_graphics();
		LockTexture();
		browserIdentifier = 0;
		BrowserManager::Instance()->DestroyBrowser(browserIdentifier);
		UnlockTexture();
		obs_leave_graphics();
	}

	std::shared_ptr<BrowserListener> browserListener(CreateListener());

	BrowserSettings browserSettings {
		.url = url,
		.width = width,
		.height = height,
		.fps = fps
	};

	browserIdentifier = BrowserManager::Instance()->CreateBrowser(
		browserSettings, browserListener);
}