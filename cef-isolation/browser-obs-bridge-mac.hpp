#pragma once

#include "cef-isolation.h"
#include "browser-obs-bridge.hpp"

class BrowserOBSBridgeMac : public BrowserOBSBridge
{
public:
	BrowserOBSBridgeMac(id<CEFIsolationService> cefIsolationService);

    const char* GetCurrentSceneJSONData() override;

private:
	id<CEFIsolationService> cefIsolationService;	
};
