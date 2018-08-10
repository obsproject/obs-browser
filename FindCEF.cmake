include(FindPackageHandleStandardArgs)
include(SelectLibraryConfigurations)

set(CEF_ROOT_DIR "" CACHE PATH "Path to a CEF distributed build")
set(CEF_WRAPPER_ROOT_DIR "" CACHE PATH "Path to a CEF dll wrapper build directory")

message(STATUS "Looking for Chromium Embedded Framework in ${CEF_ROOT_DIR}")

find_path(CEF_INCLUDE_DIR "include/cef_version.h"
	HINTS ${CEF_ROOT_DIR})

find_library(CEF_LIBRARY_RELEASE
	NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
	PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Release)

find_library(CEF_LIBRARY_DEBUG
	NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
	PATHS ${CEF_ROOT_DIR} ${CEF_ROOT_DIR}/Debug)

find_library(CEF_WRAPPER_LIBRARY_RELEASE
	NAMES cef_dll_wrapper libcef_dll_wrapper
	PATHS
		${CEF_ROOT_DIR}/build/libcef_dll/Release
		${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Release
		${CEF_ROOT_DIR}/build/libcef_dll
		${CEF_ROOT_DIR}/build/libcef_dll_wrapper
		${CEF_WRAPPER_ROOT_DIR}
		${CEF_WRAPPER_ROOT_DIR}/Release)

find_library(CEF_WRAPPER_LIBRARY_DEBUG
	NAMES cef_dll_wrapper libcef_dll_wrapper
	PATHS
		${CEF_ROOT_DIR}/build/libcef_dll/Debug
		${CEF_ROOT_DIR}/build/libcef_dll_wrapper/Debug
		${CEF_WRAPPER_ROOT_DIR}/Debug)

find_package_handle_standard_args(
	CEF
	DEFAULT_MSG
	CEF_LIBRARY	CEFWRAPPER_LIBRARY CEF_INCLUDE_DIR)

select_library_configurations(CEF)
select_library_configurations(CEF_WRAPPER)

#Create CEF import target (inspired by Zlib script)

if(NOT TARGET CEF::CEF)
	add_library(CEF::CEF UNKNOWN IMPORTED)
	set_target_properties(CEF::CEF PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${CEF_INCLUDE_DIR}")

	if(CEF_LIBRARY_RELEASE)
		set_property(TARGET CEF::CEF APPEND PROPERTY
			IMPORTED_CONFIGURATIONS RELEASE)
		set_target_properties(CEF::CEF PROPERTIES
			IMPORTED_LOCATION_RELEASE "${CEF_LIBRARY_RELEASE}")
	endif()

	if(CEF_LIBRARY_DEBUG)
		set_property(TARGET CEF::CEF APPEND PROPERTY
			IMPORTED_CONFIGURATIONS DEBUG)
		set_target_properties(CEF::CEF PROPERTIES
			IMPORTED_LOCATION_DEBUG "${CEF_LIBRARY_DEBUG}")
	endif()

	if(NOT CEF_LIBRARY_RELEASE AND NOT CEF_LIBRARY_DEBUG)
		set_property(TARGET CEF::CEF APPEND PROPERTY
			IMPORTED_LOCATION "${CEF_LIBRARY}")
	endif()
endif()

if(NOT TARGET CEF::Wrapper)
	add_library(CEF::Wrapper UNKNOWN IMPORTED)

	if(CEF_WRAPPER_LIBRARY_RELEASE)
		set_property(TARGET CEF::Wrapper APPEND PROPERTY
			IMPORTED_CONFIGURATIONS RELEASE)
		set_target_properties(CEF::Wrapper PROPERTIES
			IMPORTED_LOCATION_RELEASE "${CEF_WRAPPER_LIBRARY_RELEASE}")
	endif()

	if(CEF_WRAPPER_LIBRARY_DEBUG)
		set_property(TARGET CEF::Wrapper APPEND PROPERTY
			IMPORTED_CONFIGURATIONS DEBUG)
		set_target_properties(CEF::Wrapper PROPERTIES
			IMPORTED_LOCATION_DEBUG "${CEF_WRAPPER_LIBRARY_DEBUG}")
	endif()

	if(NOT CEF_WRAPPER_LIBRARY_RELEASE AND NOT CEF_WRAPPER_LIBRARY_DEBUG)
		set_property(TARGET CEF::Wrapper APPEND PROPERTY
			IMPORTED_LOCATION "${CEF_WRAPPER_LIBRARY}")
	endif()

	set_target_properties(CEF::CEF PROPERTIES
		INTERFACE_LINK_LIBRARIES CEF::Wrapper)
endif()