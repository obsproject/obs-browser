cmake_minimum_required(VERSION 2.8.12)
project(obs-browser)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps")

find_package(CEF QUIET)

if(NOT CEF_FOUND)
	message(STATUS "CEF Not found -- obs-browser plugin disabled.")
	return()
endif()

option(EXPERIMENTAL_SHARED_TEXTURE_SUPPORT "Enable shared texture support for the browser plugin (Win32)" ON)
option(BROWSER_PANEL_SUPPORT "Enables Qt web browser panel support" ON)
option(BROWSER_FRONTEND_API_SUPPORT "Enables integration with obs-frontend-api library" ON)

if (EXPERIMENTAL_SHARED_TEXTURE_SUPPORT)
	set(EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED)
endif()

if(NOT APPLE)
	option(USE_QT_LOOP "Runs CEF on the main UI thread alongside Qt instead of in its own thread" OFF)
else()
	set(USE_QT_LOOP TRUE)
endif()

if(APPLE)
	option(BROWSER_DEPLOY "Marks full deployment of browser plugin" OFF)
	if(BROWSER_DEPLOY)
		add_definitions(-DBROWSER_DEPLOY)
	endif()
endif()

if (BROWSER_FRONTEND_API_SUPPORT)
	set(BROWSER_FRONTEND_API_SUPPORT_ENABLED)
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
	option(BROWSER_USE_STATIC_CRT "Use static CRT" ON)

	if(BROWSER_USE_STATIC_CRT)
		add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT>)
	endif()

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

# only allow browser panels on win32 for now -- other operating systems
# need more testing
if(WIN32 AND BROWSER_PANEL_SUPPORT_ENABLED)
	list(APPEND obs-browser_SOURCES
		panel/browser-panel.cpp
		panel/browser-panel-client.cpp
		)
	list(APPEND obs-browser_HEADERS
		panel/browser-panel.hpp
		panel/browser-panel-client.hpp
		panel/browser-panel-internal.hpp
		)
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

# ----------------------------------------------------------------------------

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

if (APPLE)
	set_target_properties(obs-browser-page PROPERTIES
		COMPILE_FLAGS "-mmacosx-version-min=10.8")
endif(APPLE)

if (WIN32)
	set_target_properties(obs-browser-page PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif(WIN32)

if (APPLE AND XCODE)
	set_xcode_property(obs-browser-page CLANG_CXX_LIBRARY "libc++")
endif(APPLE AND XCODE)

# ----------------------------------------------------------------------------

if (WIN32)
	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

	install(
		DIRECTORY "${CEF_ROOT_DIR}/Resources/"
		DESTINATION "${OBS_PLUGIN_DESTINATION}"
	)

	install(FILES
		"${CEF_ROOT_DIR}/Debug/chrome_elf.dll"
		"${CEF_ROOT_DIR}/Debug/libcef.dll"
		"${CEF_ROOT_DIR}/Debug/libEGL.dll"
		"${CEF_ROOT_DIR}/Debug/libGLESv2.dll"
		"${CEF_ROOT_DIR}/Debug/natives_blob.bin"
		"${CEF_ROOT_DIR}/Debug/snapshot_blob.bin"
		"${CEF_ROOT_DIR}/Debug/v8_context_snapshot.bin"
		DESTINATION "${OBS_PLUGIN_DESTINATION}"
		CONFIGURATIONS Debug
	)

	install(FILES
		"${CEF_ROOT_DIR}/Release/chrome_elf.dll"
		"${CEF_ROOT_DIR}/Release/libcef.dll"
		"${CEF_ROOT_DIR}/Release/libEGL.dll"
		"${CEF_ROOT_DIR}/Release/libGLESv2.dll"
		"${CEF_ROOT_DIR}/Release/natives_blob.bin"
		"${CEF_ROOT_DIR}/Release/snapshot_blob.bin"
		"${CEF_ROOT_DIR}/Release/v8_context_snapshot.bin"
		DESTINATION "${OBS_PLUGIN_DESTINATION}"
		CONFIGURATIONS Release RelWithDebInfo MinSizeRel
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
		"${CEF_ROOT_DIR}/Release/natives_blob.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/snapshot_blob.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/v8_context_snapshot.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
	)
endif()

if(APPLE AND NOT BROWSER_DEPLOY)
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser.so")
	add_custom_command(TARGET obs-browser-page POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser-page")
endif()

install_obs_plugin_with_data(obs-browser data)
install_obs_plugin(obs-browser-page)
