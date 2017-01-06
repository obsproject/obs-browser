#include "browser-obs-bridge-base.hpp"

#include "util.hpp"

BrowserOBSBridgeBase::BrowserOBSBridgeBase()
{}

const char* BrowserOBSBridgeBase::GetCurrentSceneJSONData()
{
	obs_source_t *source = obs_frontend_get_current_scene();

	const char* jsonString = obsSourceToJSON(source);

	obs_source_release(source);

	return jsonString; 
}
