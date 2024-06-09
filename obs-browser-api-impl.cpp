// SPDX-FileCopyrightText: 2023 tytan652 <tytan652@tytanium.xyz>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "obs-browser-api-impl.hpp"

#include <obs-browser-api.hpp>

BrowserApi::BrowserApi()
{
	proc_handler_t *global_ph;

	ph = proc_handler_create();
	proc_handler_add(ph, "void get_api_version(out int version)",
			 &get_api_version, nullptr);

#ifdef BROWSER_AVAILABLE
	proc_handler_add(ph, "void get_qcef_version(out int version)",
			 &get_qcef_version, nullptr);
	proc_handler_add(ph, "void create_qcef(out ptr ph, out ptr qcef)",
			 &create_qcef, nullptr);
#endif

	global_ph = obs_get_proc_handler();
	proc_handler_add(global_ph, "void obs_browser_api_get_ph(out ptr ph)",
			 get_proc_handler, this);
}

BrowserApi::~BrowserApi()
{
	proc_handler_destroy(ph);
}

void BrowserApi::get_proc_handler(void *priv_data, calldata_t *cd)
{
	auto api = static_cast<BrowserApi *>(priv_data);

	calldata_set_ptr(cd, "ph", api->ph);
}

void BrowserApi::get_api_version(void *, calldata_t *cd)
{
	calldata_set_int(cd, "version", OBS_BROWSER_API_VERSION);
}
