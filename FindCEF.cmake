set(CEF_ROOT_DIR "" CACHE PATH "Path to a CEF distributed build")
set(CEF_WRAPPER_DIR "" CACHE PATH "Path to CEF wrapper build")

find_path(CEF_INCLUDE_DIR "include/cef_version.h"
	HINTS ${CEF_ROOT_DIR})

find_library(CEF_LIBRARY
	NAMES cef libcef cef.lib libcef.o
	PATHS ${CEF_ROOT_DIR}/Release)

find_library(CEF_DEBUG_LIBRARY
	NAMES cef libcef cef.lib libcef.o
	PATHS ${CEF_ROOT_DIR}/Debug)

find_library(CEF_WRAPPER_LIBRARY
	NAMES cef_dll_wrapper libcef_dll_wrapper
	PATHS ${CEF_WRAPPER_DIR})

set(CEF_LIBRARIES ${CEF_WRAPPER_LIBRARY} ${CEF_LIBRARY})

# Debug binaries may be missing so it's always optional.
if (CEF_DEBUG_LIBRARY)
	set (CEF_DEBUG_LIBRARIES ${CEF_WRAPPER_LIBRARY} ${CEF_DEBUG_LIBRARY})
endif()

find_package_handle_standard_args(CEF DEFAULT_MSG CEF_LIBRARY
	CEF_WRAPPER_LIBRARY CEF_INCLUDE_DIR)

mark_as_advanced(CEF_LIBRARY CEF_WRAPPER_LIBRARY CEF_LIBRARIES
	CEF_INCLUDE_DIR)