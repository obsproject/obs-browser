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

#include "cef-headers.hpp"
#include "browser-app.hpp"

#ifdef _WIN32

// GPU hint exports for AMD/NVIDIA laptops
#ifdef _MSC_VER
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

int CALLBACK WinMain(HINSTANCE, HINSTANCE,
	LPSTR, int)
{
	CefMainArgs mainArgs(nullptr);
#else
int main(int argc, char *argv[])
{
	CefMainArgs mainArgs(argc, argv);
#endif
	CefRefPtr<BrowserApp> mainApp(new BrowserApp());
	return CefExecuteProcess(mainArgs, mainApp.get(), NULL);
}
