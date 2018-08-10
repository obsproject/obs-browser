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

option(EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED "Enable shared texture support for the browser plugin (Win32)" ON)
option(BROWSER_PANEL_SUPPORT_ENABLED "Enables Qt web browser panel support" ON)

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

	list(APPEND obs-browser_SOURCES
		panel/browser-panel.cpp
		panel/browser-panel-client.cpp
		)
	list(APPEND obs-browser_HEADERS
		panel/browser-panel.hpp
		panel/browser-panel-client.hpp
		panel/browser-panel-internal.hpp
		)
	list(APPEND obs-browser_LIBRARIES
		Qt5::Widgets
		)
endif()

add_library(obs-browser MODULE
	${obs-browser_SOURCES}
	${obs-browser_HEADERS}
	)

target_link_libraries(obs-browser
	${obs-browser_LIBRARIES}
	)

# ----------------------------------------------------------------------------

set(cef-bootstrap_SOURCES
	cef-bootstrap/cef-bootstrap-main.cpp
	browser-app.cpp
	deps/json11/json11.cpp
	)
set(cef-bootstrap_HEADERS
	cef-bootstrap/cef-bootstrap-main.cpp
	browser-app.hpp
	deps/json11/json11.hpp
	cef-headers.hpp
	)

add_executable(cef-bootstrap
	${cef-bootstrap_SOURCES}
	${cef-bootstrap_HEADERS}
	)
target_link_libraries(cef-bootstrap
	${CEF_LIBRARIES}
	)

if (APPLE)
	set_target_properties(cef-bootstrap PROPERTIES
		COMPILE_FLAGS "-mmacosx-version-min=10.8")
endif(APPLE)

if (WIN32)
	set_target_properties(cef-bootstrap PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif(WIN32)

if (APPLE AND XCODE)
	set_xcode_property(cef-bootstrap CLANG_CXX_LIBRARY "libc++")
endif(APPLE AND XCODE)

# ----------------------------------------------------------------------------

if (WIN32)
	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CEF_ROOT_DIR}/Resources"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
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

install_obs_plugin_with_data(obs-browser data)
install_obs_plugin(cef-bootstrap)
