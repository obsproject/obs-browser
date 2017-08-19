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

#import "cef-isolation-service-manager.h"
#include "browser-manager.hpp"
#include "browser-settings.hpp"
#include "browser-manager-mac.h"

BrowserManager::BrowserManager()
: pimpl(new Impl())
{
}

BrowserManager::~BrowserManager()
{
}

void BrowserManager::Startup()
{
	pimpl->Startup();
}

void BrowserManager::Shutdown()
{
	pimpl->Shutdown();
}

void BrowserManager::Restart()
{
	pimpl->Restart();
}

void BrowserManager::TickBrowser(int browserIdentifier)
{
	pimpl->TickBrowser(browserIdentifier);
}

void BrowserManager::SendMouseClick(int browserIdentifier,
		const struct obs_mouse_event *event, int32_t type, bool mouseUp,
		uint32_t clickCount)
{
	pimpl->SendMouseClick(browserIdentifier, event, type, mouseUp,
		clickCount);
}

void BrowserManager::SendMouseMove(int browserIdentifier,
		const struct obs_mouse_event *event, bool mouseLeave)
{
	pimpl->SendMouseMove(browserIdentifier, event, mouseLeave);
}

void BrowserManager::SendMouseWheel(int browserIdentifier,
		const struct obs_mouse_event *event, int xDelta, int yDelta)
{
	pimpl->SendMouseWheel(browserIdentifier, event, xDelta, yDelta);
}
void BrowserManager::SendFocus(int browserIdentifier, bool focus)
{
	pimpl->SendFocus(browserIdentifier, focus);
}

void BrowserManager::SendKeyClick(int browserIdentifier,
		const struct obs_key_event *event, bool keyUp)
{
	pimpl->SendKeyClick(browserIdentifier, event, keyUp);
}

void BrowserManager::ExecuteVisiblityJSCallback(int browserIdentifier, bool visible)
{
	pimpl->ExecuteVisiblityJSCallback(browserIdentifier, visible);
}

void BrowserManager::ExecuteActiveJSCallback(int browserIdentifier, bool active)
{
	pimpl->ExecuteActiveJSCallback(browserIdentifier, active);
}

void BrowserManager::ExecuteSceneChangeJSCallback(const char *name)
{
	pimpl->ExecuteSceneChangeJSCallback(name);
}

void BrowserManager::RefreshPageNoCache(int browserIdentifier)
{
    pimpl->RefreshPageNoCache(browserIdentifier);
}

void BrowserManager::DispatchJSEvent(const char *name, const char *jsonData)
{
	pimpl->DispatchJSEvent(name, jsonData);
}

int BrowserManager::CreateBrowser(const BrowserSettings &browserSettings,
		const std::shared_ptr<BrowserListener> &browserListener)
{
	return pimpl->CreateBrowser(browserSettings, browserListener);
}

void BrowserManager::DestroyBrowser(int browserIdentifier)
{
	pimpl->DestroyBrowser(browserIdentifier);
}

BrowserManager::Impl::Impl()
: cefIsolationServiceManager(new CEFIsolationServiceManager())
{
}

void BrowserManager::Impl::Startup() {
	cefIsolationServiceManager->Startup();
}

void BrowserManager::Impl::Shutdown() {
	cefIsolationServiceManager->Shutdown();
}

int BrowserManager::Impl::CreateBrowser(const BrowserSettings &browserSettings,
		const std::shared_ptr<BrowserListener> &browserListener)
{
	return cefIsolationServiceManager->CreateBrowser(
		browserSettings, browserListener);
}

void BrowserManager::Impl::DestroyBrowser(int browserIdentifier)
{
	cefIsolationServiceManager->DestroyBrowser(browserIdentifier);
}

void BrowserManager::Impl::Restart()
{
	cefIsolationServiceManager->Restart();
}

void BrowserManager::Impl::TickBrowser(int browserIdentifier)
{
	cefIsolationServiceManager->TickBrowser(browserIdentifier);
}

void BrowserManager::Impl::SendMouseClick(int browserIdentifier,
		const struct obs_mouse_event *event, int32_t type, bool mouseUp,
		uint32_t clickCount)
{
	cefIsolationServiceManager->SendMouseClick(browserIdentifier,
		event, type, mouseUp, clickCount);
}

void BrowserManager::Impl::SendMouseMove(int browserIdentifier,
		const struct obs_mouse_event *event, bool mouseLeave)
{
	cefIsolationServiceManager->SendMouseMove(browserIdentifier,
		event, mouseLeave);
}

void BrowserManager::Impl::SendMouseWheel(int browserIdentifier,
		const struct obs_mouse_event *event, int xDelta, int yDelta)
{
	cefIsolationServiceManager->SendMouseWheel(browserIdentifier,
		event, xDelta, yDelta);
}

void BrowserManager::Impl::SendFocus(int browserIdentifier, bool focus)
{
	cefIsolationServiceManager->SendFocus(browserIdentifier, focus);
}

void BrowserManager::Impl::SendKeyClick(int browserIdentifier,
		const struct obs_key_event *event, bool keyUp)
{
	cefIsolationServiceManager->SendKeyClick(browserIdentifier, event,
		keyUp);
}

void BrowserManager::Impl::ExecuteVisiblityJSCallback(int browserIdentifier, bool visible)
{
	cefIsolationServiceManager->ExecuteVisiblityJSCallback(browserIdentifier, visible);
}

void BrowserManager::Impl::ExecuteActiveJSCallback(int browserIdentifier, bool active)
{
	cefIsolationServiceManager->ExecuteActiveJSCallback(browserIdentifier, active);
}

void BrowserManager::Impl::ExecuteSceneChangeJSCallback(const char *name)
{
	cefIsolationServiceManager->ExecuteSceneChangeJSCallback(name);
}

void BrowserManager::Impl::RefreshPageNoCache(int browserIdentifier)
{
    cefIsolationServiceManager->RefreshPageNoCache(browserIdentifier);
}

void BrowserManager::Impl::DispatchJSEvent(const char *eventName, const char *jsonData)
{
	cefIsolationServiceManager->DispatchJSEvent(eventName, jsonData);
}

static BrowserManager *instance;

BrowserManager *BrowserManager::Instance()
{
	if (instance == nullptr) {
		instance = new BrowserManager();
	}
	return instance;
}

void BrowserManager::DestroyInstance()
{
	if (instance != nullptr) {
		delete instance;
	}
	instance = nullptr;

}
