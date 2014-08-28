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

void
BrowserManager::Startup()
{
	pimpl->Startup();
}

void
BrowserManager::Shutdown()
{
	pimpl->Shutdown();
}

void
BrowserManager::TickBrowser(const int browserIdentifier)
{
	pimpl->TickBrowser(browserIdentifier);
}

int
BrowserManager::CreateBrowser(
	const BrowserSettings &browserSettings,
	const std::shared_ptr<BrowserListener> &browserListener)
{
	return pimpl->CreateBrowser(browserSettings, browserListener);
}

void
BrowserManager::DestroyBrowser(const int browserIdentifier)
{
	pimpl->DestroyBrowser(browserIdentifier);
}

BrowserManager::Impl::Impl()
: cefIsolationServiceManager(new CEFIsolationServiceManager())
{
}

void
BrowserManager::Impl::Startup() {
	cefIsolationServiceManager->Startup();
}

void
BrowserManager::Impl::Shutdown() {
	cefIsolationServiceManager->Shutdown();
}

int
BrowserManager::Impl::CreateBrowser(
	const BrowserSettings &browserSettings,
	const std::shared_ptr<BrowserListener> &browserListener)
{
	return cefIsolationServiceManager->CreateBrowser(
		browserSettings, browserListener);
}

void
BrowserManager::Impl::DestroyBrowser(const int browserIdentifier)
{
	cefIsolationServiceManager->DestroyBrowser(browserIdentifier);
}

void
BrowserManager::Impl::TickBrowser(const int browserIdentifier)
{
	cefIsolationServiceManager->TickBrowser(browserIdentifier);
}

static BrowserManager *instance;

BrowserManager *
BrowserManager::Instance()
{
	if (instance == nullptr) {
		instance = new BrowserManager();
	}
	return instance;
}

void
BrowserManager::DestroyInstance()
{
	if (instance != nullptr) {
		delete instance;
	}
	instance = nullptr;

}
