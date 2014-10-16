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
#include <util/platform.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-browser", "en-US")

static void *obs_so;
static bool (*obs_actual_load)(void);
static void (*obs_actual_unload)(void);
static void (*obs_actual_module_set_pointer)(obs_module_t *module);

bool
obs_module_load(void)
{
	obs_so = NULL;
	
	bool success;
	char *obs_browser_path = obs_module_file("obs-browser");
	if (obs_browser_path) {
		obs_so = os_dlopen(obs_browser_path);
		if (obs_so) {
			obs_actual_load = os_dlsym(obs_so, "obs_module_load");
			obs_actual_unload = os_dlsym(obs_so, 
				"obs_module_unload");
			obs_actual_module_set_pointer = os_dlsym(obs_so,
				"obs_actual_module_so");

			if (obs_actual_load && obs_actual_unload 
				&& obs_actual_module_set_pointer) 
			{
				obs_actual_module_set_pointer(
					obs_module_pointer);
				if (obs_actual_load()) {
					goto success;
				}
			}

		}
	}

error:
	success = false;
	if (obs_so)
		os_dlclose(obs_so);
success:
	if (obs_browser_path)
		bfree(obs_browser_path);
	return success;
}

void

obs_module_unload()
{
	if (obs_so) {
		obs_actual_unload();
		os_dlclose(obs_so);
		obs_so = NULL;
	}


}