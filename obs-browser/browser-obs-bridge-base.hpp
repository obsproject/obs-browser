#pragma once

#include "browser-obs-bridge.hpp"

class BrowserOBSBridgeBase : public BrowserOBSBridge
{
public:
	BrowserOBSBridgeBase();

    const char* GetCurrentSceneJSONData() override;
};
