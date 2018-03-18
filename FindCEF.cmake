include(FindPackageHandleStandardArgs)

SET(CEF_ROOT_DIR "" CACHE PATH "Path to a CEF distributed build")

message("Looking for Chromium Embedded Framework in ${CEF_ROOT_DIR}")

find_path(CEF_INCLUDE_DIR "include/cef_version.h"
	HINTS ${CEF_ROOT_DIR})

find_library(CEF_LIBRARY
	NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
	PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Release)
find_library(CEF_LIBRARY_DEBUG
	NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
	PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Debug)

find_library(CEFWRAPPER_LIBRARY
	NAMES cef_dll_wrapper libcef_dll_wrapper
	PATHS ${CEF_ROOT_DIR}/build/libcef_dll/Release
		${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Release
		${CEF_ROOT_DIR}/build/libcef_dll
		${CEF_ROOT_DIR}/build/libcef_dll_wrapper)
find_library(CEFWRAPPER_LIBRARY_DEBUG
	NAMES cef_dll_wrapper libcef_dll_wrapper
	PATHS ${CEF_ROOT_DIR}/build/libcef_dll/Debug ${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Debug)

if (NOT CEF_LIBRARY)
	message(FATAL_ERROR "Could not find the CEF shared library" )
endif (NOT CEF_LIBRARY)

if (NOT CEFWRAPPER_LIBRARY)
	message(FATAL_ERROR "Could not find the CEF wrapper library" )
endif (NOT CEFWRAPPER_LIBRARY)

set(CEF_LIBRARIES
	debug ${CEFWRAPPER_LIBRARY_DEBUG}
	debug ${CEF_LIBRARY_DEBUG}
	optimized ${CEFWRAPPER_LIBRARY}
	optimized ${CEF_LIBRARY})

find_package_handle_standard_args(CEF DEFAULT_MSG CEF_LIBRARY 
	CEFWRAPPER_LIBRARY CEF_INCLUDE_DIR)

mark_as_advanced(CEF_LIBRARY CEF_WRAPPER_LIBRARY CEF_LIBRARIES 
	CEF_INCLUDE_DIR)
