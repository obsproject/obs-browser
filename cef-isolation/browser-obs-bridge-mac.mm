#include "browser-obs-bridge-mac.hpp"

BrowserOBSBridgeMac::BrowserOBSBridgeMac(id<CEFIsolationService> cefIsolationService)
: cefIsolationService(cefIsolationService)
{}

const char* BrowserOBSBridgeMac::GetCurrentSceneJSONData()
{
	return [cefIsolationService getCurrentSceneJSONData];
}
