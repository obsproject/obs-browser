#include <obs-module.h>
#include "Version.hpp"

MODULE_EXPORT int64_t streamelements_get_version_number(void)
{
	return STREAMELEMENTS_PLUGIN_VERSION;
}
