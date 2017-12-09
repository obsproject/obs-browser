#include "util.hpp"

/**
	Takes an OBS source and generates a JSON encoded string representing information about the source.
	
	@param source is the OBS source that we want to turn into json data
	@return json encoded string
*/
const char* obsSourceToJSON(obs_source_t *source)
{
	const char *name = obs_source_get_name(source);

	json_t *obj = json_object();
	json_object_set_new(obj, "name", json_string(name));
	json_object_set_new(obj, "width", json_integer(obs_source_get_width(source)));
	json_object_set_new(obj, "height", json_integer(obs_source_get_height(source)));
	const char *jsonString = json_dumps(obj, 0);
	free(obj);

	return jsonString;
}

/**
	Gets the OBS status and generates a JSON encoded string representing the status (recording, streaming etc.).

	@return json encoded string
*/
const char* obsStatusToJSON()
{
	json_t *obj = json_object();
	json_object_set_new(obj, "recording", json_boolean(obs_frontend_recording_active()));
	json_object_set_new(obj, "streaming", json_boolean(obs_frontend_streaming_active()));
	json_object_set_new(obj, "replaybuffer", json_boolean(obs_frontend_replay_buffer_active()));
	const char *jsonString = json_dumps(obj, 0);
	free(obj);

	return jsonString;
}
