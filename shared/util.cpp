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