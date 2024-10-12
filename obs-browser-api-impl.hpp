// SPDX-FileCopyrightText: 2023 tytan652 <tytan652@tytanium.xyz>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <obs.hpp>

class BrowserApi {
	proc_handler_t *ph;

	static void get_proc_handler(void *priv_data, calldata_t *cd);

	static void get_api_version(void *, calldata_t *cd);

#ifdef BROWSER_AVAILABLE
	static void get_qcef_version(void *, calldata_t *cd);
	static void create_qcef(void *, calldata_t *cd);
#endif

public:
	BrowserApi();
	~BrowserApi();
};
