include(FindPackageHandleStandardArgs)

SET(CEF_ROOT_DIR "" CACHE PATH "Path to a CEF distributed build")

message(STATUS "Looking for Chromium Embedded Framework in ${CEF_ROOT_DIR}")

find_path(CEF_INCLUDE_DIR "include/cef_version.h"
	HINTS ${CEF_ROOT_DIR})

if(APPLE)
	find_library(CEF_LIBRARY
		NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
		NO_DEFAULT_PATH
		PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Release)
	find_library(CEFWRAPPER_LIBRARY
		NAMES cef_dll_wrapper libcef_dll_wrapper
		NO_DEFAULT_PATH
		PATHS ${CEF_ROOT_DIR}/build/libcef_dll/Release
			${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Release
			${CEF_ROOT_DIR}/build/libcef_dll
			${CEF_ROOT_DIR}/build/libcef_dll_wrapper)
elseif(UNIX)
	find_library(CEF_LIBRARY
		NAMES libcef.so "Chromium Embedded Framework"
		NO_DEFAULT_PATH
		PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Release)
	find_library(CEFWRAPPER_LIBRARY
		NAMES libcef_dll_wrapper.a
		NO_DEFAULT_PATH
		PATHS ${CEF_ROOT_DIR}/build/libcef_dll_wrapper
			${CEF_ROOT_DIR}/libcef_dll_wrapper)
else()
	find_library(CEF_LIBRARY
		NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
		PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Release)
	find_library(CEFWRAPPER_LIBRARY
		NAMES cef_dll_wrapper libcef_dll_wrapper
		PATHS ${CEF_ROOT_DIR}/build/libcef_dll/Release
			${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Release
			${CEF_ROOT_DIR}/build/libcef_dll
			${CEF_ROOT_DIR}/build/libcef_dll_wrapper)
	if(WIN32)
		find_library(CEFWRAPPER_LIBRARY_DEBUG
			NAMES cef_dll_wrapper libcef_dll_wrapper
			PATHS ${CEF_ROOT_DIR}/build/libcef_dll/Debug ${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Debug)
	endif()
endif()

if(NOT CEF_LIBRARY)
	message(WARNING "Could not find the CEF shared library" )
	set(CEF_FOUND FALSE)
	return()
endif()

if(NOT CEFWRAPPER_LIBRARY)
	message(WARNING "Could not find the CEF wrapper library" )
	set(CEF_FOUND FALSE)
	return()
endif()

if(WIN32)
	set(CEF_LIBRARIES
			${CEF_LIBRARY}
			optimized ${CEFWRAPPER_LIBRARY})
	if (CEFWRAPPER_LIBRARY_DEBUG)
		list(APPEND CEF_LIBRARIES
				debug ${CEFWRAPPER_LIBRARY_DEBUG})
	endif()
else()
	# Fixes cmake 3.19.0 commit that added support for modern Xcode build system, but "forgot"
	# to also escape framework names themselves in addition to the framework path:
	# Commit https://gitlab.kitware.com/cmake/cmake/-/commit/ce2dee9e5bae37c8117087bb83add075c3c123b4
	if(${CMAKE_VERSION} VERSION_GREATER "3.19.0" AND XCODE)
		string(REPLACE "Chromium Embedded Framework" "\"Chromium Embedded Framework\"" CEF_LIBRARY_FIXED ${CEF_LIBRARY})
		set(CEF_LIBRARIES
				${CEF_LIBRARY_FIXED}
				${CEFWRAPPER_LIBRARY})
	else()
		set(CEF_LIBRARIES
				${CEF_LIBRARY}
				${CEFWRAPPER_LIBRARY})
	endif()
endif()

find_package_handle_standard_args(CEF DEFAULT_MSG CEF_LIBRARY
	CEFWRAPPER_LIBRARY CEF_INCLUDE_DIR)
mark_as_advanced(CEF_LIBRARY CEF_WRAPPER_LIBRARY CEF_LIBRARIES
	CEF_INCLUDE_DIR)
