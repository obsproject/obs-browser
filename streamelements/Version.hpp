#pragma once

/* Numeric value indicating the current major version of the API.
 * This value must be incremented each time a breaking change to
 * the API is introduced(change of existing API methods/properties
 * signatures).
 */
#ifndef HOST_API_VERSION_MAJOR
#define HOST_API_VERSION_MAJOR 1
#endif

/* Numeric value indicating the current minor version of the API.
 * This value will be incremented each time a non-breaking change
 * to the API is introduced (additional functionality, bugfixes
 * of existing functionality).
 */
#ifndef HOST_API_VERSION_MINOR
#define HOST_API_VERSION_MINOR 17
#endif

/* Numeric value in the YYYYMMDDHHmmss format, indicating the current
 * version of the plugin.
 *
 * This version number is used by obs-streamelements plug-in to
 * determine whether an update is available and should be offered to
 * the user.
 *
 * This value should be set as part of the build process.
 */
#ifndef STREAMELEMENTS_PLUGIN_VERSION
#include "Version.generated.hpp"
#endif
