#pragma once

class BrowserOBSBridge
{
public:
	virtual const char* GetCurrentSceneJSONData() = 0;
	virtual const char* GetStatus() = 0;
};
