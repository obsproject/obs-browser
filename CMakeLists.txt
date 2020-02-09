cmake_minimum_required(VERSION 2.8.12)
project(obs-browser)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps")

option(BROWSER_LEGACY "Use legacy CEF version 3770" OFF)

find_package(CEF QUIET)

if(NOT CEF_FOUND)
	message(STATUS "CEF Not found -- obs-browser plugin disabled.")
	return()
endif()

option(SHARED_TEXTURE_SUPPORT_ENABLED "Enable shared texture support for the browser plugin" OFF)
if(WIN32 OR (APPLE AND NOT BROWSER_LEGACY))
    set(SHARED_TEXTURE_SUPPORT_ENABLED ON)
endif()

option(BROWSER_PANEL_SUPPORT_ENABLED "Enables Qt web browser panel support" ON)

if(NOT APPLE)
	option(USE_QT_LOOP "Runs CEF on the main UI thread alongside Qt instead of in its own thread" OFF)
else()
	set(USE_QT_LOOP ON)
endif()

if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	if(NOT X11_FOUND)
		message(STATUS "X11 Not found -- obs-browser plugin disabled.")
		return()
	endif()
	include_directories("${X11_INCLUDE_DIR}")
endif()


configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/browser-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/browser-config.h")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CEF_ROOT_DIR}")

# ----------------------------------------------------------------------------

set(obs-browser_LIBRARIES
	libobs
	obs-frontend-api
	)

list(APPEND obs-browser_LIBRARIES
	${CEF_LIBRARIES})

if (APPLE)
	find_library(COREFOUNDATION CoreFoundation)
	find_library(APPKIT AppKit)
	list(APPEND obs-browser_LIBRARIES
			${COREFOUNDATION}
			${APPKIT})
endif()

if(BROWSER_PANEL_SUPPORT_ENABLED OR USE_QT_LOOP)
	if(DEFINED QTDIR${_lib_suffix})
		list(APPEND CMAKE_PREFIX_PATH "${QTDIR${_lib_suffix}}")
	elseif(DEFINED QTDIR)
		list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
	elseif(DEFINED ENV{QTDIR${_lib_suffix}})
		list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR${_lib_suffix}}")
	elseif(DEFINED ENV{QTDIR})
		list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
	endif()

	set(CMAKE_AUTOMOC TRUE)
	find_package(Qt5Widgets REQUIRED)
	list(APPEND obs-browser_LIBRARIES
		Qt5::Widgets
		)
endif()

if(MSVC)
	string(REPLACE "/MD" "/MT"
		"CMAKE_C_FLAGS"
		"${CMAKE_C_FLAGS}")

	string(REPLACE "/MD" "/MT"
		"CMAKE_CXX_FLAGS"
		"${CMAKE_CXX_FLAGS}")

	string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" UPPER_CONFIG_TYPES)
	foreach(CONFIG_TYPE ${UPPER_CONFIG_TYPES})
		string(REPLACE "/MD" "/MT"
			"CMAKE_C_FLAGS_${CONFIG_TYPE}"
			"${CMAKE_C_FLAGS_${CONFIG_TYPE}}")

		string(REPLACE "/MD" "/MT"
			"CMAKE_CXX_FLAGS_${CONFIG_TYPE}"
			"${CMAKE_CXX_FLAGS_${CONFIG_TYPE}}")
	endforeach()
	list(APPEND obs-browser_LIBRARIES
		d3d11
		dxgi
		)
endif()

set(obs-browser_SOURCES
	obs-browser-source.cpp
	obs-browser-source-audio.cpp
	obs-browser-plugin.cpp
	browser-scheme.cpp
	browser-client.cpp
	browser-app.cpp
	deps/json11/json11.cpp
	deps/base64/base64.cpp
	deps/wide-string.cpp
	)
set(obs-browser_HEADERS
	obs-browser-source.hpp
	browser-scheme.hpp
	browser-client.hpp
	browser-app.hpp
	browser-version.h
	deps/json11/json11.hpp
	deps/base64/base64.hpp
	deps/wide-string.hpp
	cef-headers.hpp
	)

if (APPLE)
	list(APPEND obs-browser_SOURCES
		macutil.mm
		)
endif()

# only allow browser panels on win32 for now -- other operating systems
# need more testing
if((WIN32 OR APPLE) AND BROWSER_PANEL_SUPPORT_ENABLED)
	list(APPEND obs-browser_SOURCES
		panel/browser-panel.cpp
		panel/browser-panel-client.cpp
		)
	list(APPEND obs-browser_HEADERS
		panel/browser-panel.hpp
		panel/browser-panel-client.hpp
		panel/browser-panel-internal.hpp
		)
	if (APPLE)
		list(APPEND obs-browser_LIBRARIES objc)
	endif()
endif()

add_library(obs-browser MODULE
	${obs-browser_SOURCES}
	${obs-browser_HEADERS}
	)

target_link_libraries(obs-browser
	${obs-browser_LIBRARIES}
	)

if(USE_QT_LOOP)
	target_compile_definitions(obs-browser PRIVATE USE_QT_LOOP)
endif()

if(BROWSER_LEGACY)
    target_compile_definitions(obs-browser PRIVATE BROWSER_LEGACY)
endif()

if(SHARED_TEXTURE_SUPPORT_ENABLED)
    target_compile_definitions(obs-browser PRIVATE SHARED_TEXTURE_SUPPORT_ENABLED)
endif()

set_target_properties(obs-browser PROPERTIES FOLDER "plugins/obs-browser")

# ----------------------------------------------------------------------------

if (APPLE AND NOT BROWSER_LEGACY)
	set(obs-browser-page_SOURCES
		obs-browser-page/obs-browser-page-main.cpp
		browser-app.cpp
		deps/json11/json11.cpp
		)
	set(obs-browser-page_HEADERS
		obs-browser-page/obs-browser-page-main.cpp
		browser-app.hpp
		deps/json11/json11.hpp
		cef-headers.hpp
		)

	set(CEF_HELPER_TARGET "obs-browser-page")
	set(CEF_HELPER_OUTPUT_NAME "OBS Helper")
	set(CEF_HELPER_APP_SUFFIXES
		"::"
		" (GPU):_gpu:.gpu"
		" (Plugin):_plugin:.plugin"
		" (Renderer):_renderer:.renderer"
	)

	foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
    	# Convert to a list and extract the suffix values.
    	string(REPLACE ":" ";" _suffix_list ${_suffix_list})
    	list(GET _suffix_list 0 _name_suffix)
    	list(GET _suffix_list 1 _target_suffix)
    	list(GET _suffix_list 2 _plist_suffix)

    	# Define Helper target and output names.
    	set(_helper_target "${CEF_HELPER_TARGET}${_target_suffix}")
    	set(_helper_output_name "${CEF_HELPER_OUTPUT_NAME}${_name_suffix}")

    	# Create Helper-specific variants of the helper-Info.plist file. Do this
    	# manually because the configure_file command (which is executed as part of
    	# MACOSX_BUNDLE_INFO_PLIST) uses global env variables and would insert the
    	# wrong values with multiple targets.
    	set(_helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${_target_suffix}.plist")
    	file(READ "${CMAKE_CURRENT_SOURCE_DIR}/helper-Info.plist" _plist_contents)
    	string(REPLACE "\${EXECUTABLE_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
    	string(REPLACE "\${PRODUCT_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
    	string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents ${_plist_contents})
    	file(WRITE ${_helper_info_plist} ${_plist_contents})

    	set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.obsproject.obs-studio.helper${_plist_suffix}")

    	# Create Helper executable target.
    	add_executable(${_helper_target} MACOSX_BUNDLE
    		${obs-browser-page_SOURCES}
    		${obs-browser-page_HEADERS})
    	target_link_libraries(${_helper_target} ${CEF_LIBRARIES})
    	set_target_properties(${_helper_target} PROPERTIES
    		MACOSX_BUNDLE_INFO_PLIST ${_helper_info_plist}
    		OUTPUT_NAME ${_helper_output_name}
            FOLDER "plugins/obs-browser/helpers/"
			COMPILE_FLAGS "-mmacosx-version-min=10.8"
    	)

        if(XCODE)
			set(TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>")
		else(XCODE)
			set(TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}")
		endif(XCODE)

		add_custom_command(TARGET obs-browser POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${TARGET_DIR}/${_helper_output_name}.app"
			"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${_helper_output_name}.app"
			VERBATIM)
  	endforeach()
else()
    set(obs-browser-page_SOURCES
    	obs-browser-page/obs-browser-page-main.cpp
    	browser-app.cpp
    	deps/json11/json11.cpp
    	)
    set(obs-browser-page_HEADERS
    	obs-browser-page/obs-browser-page-main.cpp
    	browser-app.hpp
    	deps/json11/json11.hpp
    	cef-headers.hpp
    	)

    add_executable(obs-browser-page
    	${obs-browser-page_SOURCES}
    	${obs-browser-page_HEADERS}
    	)
    target_link_libraries(obs-browser-page
    	${CEF_LIBRARIES}
    	)
    set_target_properties(obs-browser-page PROPERTIES FOLDER "plugins/obs-browser")
    if(APPLE)
        target_compile_definitions(obs-browser PRIVATE BROWSER_LEGACY)
    endif()
endif()

if (WIN32)
	set_target_properties(obs-browser PROPERTIES LINK_FLAGS "/IGNORE:4099")
	set_target_properties(obs-browser-page PROPERTIES LINK_FLAGS "/IGNORE:4099 /SUBSYSTEM:WINDOWS")
endif(WIN32)

if (UNIX AND NOT APPLE)
    set_target_properties(obs-browser PROPERTIES INSTALL_RPATH "$ORIGIN/")
    set_target_properties(obs-browser-page PROPERTIES INSTALL_RPATH "$ORIGIN/")
endif()

# ----------------------------------------------------------------------------

if (WIN32)
	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CEF_ROOT_DIR}/Resources"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
	)

	target_sources(obs-browser-page
		PRIVATE obs-browser-page.manifest
	)

	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/chrome_elf.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/libcef.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/libEGL.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/libGLESv2.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/snapshot_blob.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/v8_context_snapshot.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
	)
	if(EXISTS "${CEF_ROOT_DIR}/Release/natives_blob.bin")
		add_custom_command(TARGET obs-browser POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
			"${CEF_ROOT_DIR}/Release/natives_blob.bin"
			"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		)
	endif()
endif()

if (UNIX AND NOT APPLE)
	install(DIRECTORY "${CEF_ROOT_DIR}/Resources/" DESTINATION "${OBS_PLUGIN_DESTINATION}")
	install(DIRECTORY "${CEF_ROOT_DIR}/Release/" DESTINATION "${OBS_PLUGIN_DESTINATION}")
endif()

install_obs_plugin_with_data(obs-browser data)
if(BROWSER_LEGACY OR NOT APPLE)
install_obs_plugin(obs-browser-page)
endif()
