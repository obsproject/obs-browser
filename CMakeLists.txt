cmake_minimum_required(VERSION 2.8.12)
project(obs-browser)

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps")

find_package(CEF QUIET)
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Libcurl REQUIRED)
find_package(BugSplat REQUIRED)

include_directories(SYSTEM
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
	${LIBCURL_INCLUDE_DIRS}
	${BUGSPLAT_INCLUDE_DIR}
)

#qt5_wrap_ui(NetworkDialog "networkdialog.ui")
qt5_add_resources(obs-browser-streamelements_QRC_SOURCES "streamelements/streamelements.qrc")

if(NOT CEF_FOUND)
	message(STATUS "CEF Not found -- obs-browser plugin disabled.")
	return()
endif()

if (NOT BUGSPLAT_FOUND)
	message(STATUS "BugSplat Not found -- obs-browser plugin disabled.")
	return()
endif()

option(EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED "Enable shared texture support for the browser plugin (Win32)" ON)
option(BROWSER_PANEL_SUPPORT_ENABLED "Enables Qt web browser panel support" ON)

if(APPLE)
	option(BROWSER_DEPLOY "Marks full deployment of browser plugin" OFF)
	if(BROWSER_DEPLOY)
		add_definitions(-DBROWSER_DEPLOY)
	endif()
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
	Qt5::Core
	Qt5::Widgets
	w32-pthreads
	${LIBCURL_LIBRARIES}
	${BUGSPLAT_LIBRARIES}
	)

list(APPEND obs-browser_LIBRARIES
	${CEF_LIBRARIES})

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
	obs-browser-plugin.cpp
	browser-scheme.cpp
	browser-client.cpp
	browser-app.cpp
	deps/json11/json11.cpp
	deps/base64/base64.cpp
	deps/wide-string.cpp
	streamelements/Version.cpp
	streamelements/StreamElementsAsyncTaskQueue.cpp
	streamelements/StreamElementsCefClient.cpp
	streamelements/StreamElementsBrowserWidget.cpp
	streamelements/StreamElementsBrowserWidgetManager.cpp
	streamelements/StreamElementsBandwidthTestClient.cpp
	streamelements/StreamElementsObsBandwidthTestClient.cpp
	streamelements/StreamElementsWidgetManager.cpp
	streamelements/StreamElementsObsAppMonitor.cpp
	streamelements/StreamElementsApiMessageHandler.cpp
	streamelements/StreamElementsConfig.cpp
	streamelements/StreamElementsGlobalStateManager.cpp
	streamelements/StreamElementsMenuManager.cpp
	streamelements/StreamElementsBandwidthTestManager.cpp
	streamelements/StreamElementsOutputSettingsManager.cpp
	streamelements/StreamElementsWorkerManager.cpp
	streamelements/StreamElementsBrowserDialog.cpp
	streamelements/StreamElementsUtils.cpp
	streamelements/StreamElementsHotkeyManager.cpp
	streamelements/StreamElementsReportIssueDialog.cpp
	streamelements/StreamElementsProgressDialog.cpp
	streamelements/StreamElementsPerformanceHistoryTracker.cpp
	streamelements/StreamElementsNetworkDialog.cpp
	streamelements/StreamElementsAnalyticsEventsManager.cpp
	streamelements/StreamElementsCrashHandler.cpp
	streamelements/StreamElementsMessageBus.cpp
	streamelements/StreamElementsBrowserSourceApiMessageHandler.cpp
	streamelements/StreamElementsControllerServer.cpp
	streamelements/StreamElementsObsSceneManager.cpp
	streamelements/StreamElementsLocalWebFilesServer.cpp
	streamelements/StreamElementsExternalSceneDataProviderSlobsClient.cpp
	streamelements/StreamElementsHttpClient.cpp
	streamelements/StreamElementsNativeOBSControlsManager.cpp
	streamelements/deps/zip/zip.c
	streamelements/deps/server/NamedPipesServer.cpp
	streamelements/deps/server/NamedPipesServerClientHandler.cpp
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
	streamelements/Version.hpp
	streamelements/Version.generated.hpp
	streamelements/StreamElementsUtils.hpp
	streamelements/StreamElementsAsyncTaskQueue.hpp
	streamelements/StreamElementsCefClient.hpp
	streamelements/StreamElementsBrowserWidget.hpp
	streamelements/StreamElementsBrowserWidgetManager.hpp
	streamelements/StreamElementsBandwidthTestClient.hpp
	streamelements/StreamElementsObsBandwidthTestClient.hpp
	streamelements/StreamElementsWidgetManager.hpp
	streamelements/StreamElementsObsAppMonitor.hpp
	streamelements/StreamElementsBrowserMessageHandler.hpp
	streamelements/StreamElementsApiMessageHandler.hpp
	streamelements/StreamElementsConfig.hpp
	streamelements/StreamElementsGlobalStateManager.hpp
	streamelements/StreamElementsMenuManager.hpp
	streamelements/StreamElementsBandwidthTestManager.hpp
	streamelements/StreamElementsOutputSettingsManager.hpp
	streamelements/StreamElementsWorkerManager.cpp
	streamelements/StreamElementsBrowserDialog.hpp
	streamelements/StreamElementsHotkeyManager.hpp
	streamelements/StreamElementsReportIssueDialog.hpp
	streamelements/StreamElementsProgressDialog.hpp
	streamelements/StreamElementsPerformanceHistoryTracker.hpp
	streamelements/StreamElementsNetworkDialog.hpp
	streamelements/StreamElementsAnalyticsEventsManager.hpp
	streamelements/StreamElementsCrashHandler.hpp
	streamelements/StreamElementsMessageBus.hpp
	streamelements/StreamElementsBrowserSourceApiMessageHandler.hpp
	streamelements/StreamElementsControllerServer.hpp
	streamelements/StreamElementsObsSceneManager.hpp
	streamelements/StreamElementsFileSystemMapper.hpp
	streamelements/StreamElementsLocalWebFilesServer.hpp
	streamelements/StreamElementsExternalSceneDataProviderManager.hpp
	streamelements/StreamElementsExternalSceneDataProviderSlobsClient.hpp
	streamelements/StreamElementsExternalSceneDataProvider.hpp
	streamelements/StreamElementsHttpClient.hpp
	streamelements/StreamElementsNativeOBSControlsManager.hpp
	streamelements/deps/zip/zip.h
	streamelements/deps/zip/miniz.h
	streamelements/deps/moodycamel/concurrentqueue.h
	streamelements/deps/moodycamel/blockingconcurrentqueue.h
	streamelements/deps/server/NamedPipesServer.hpp
	streamelements/deps/server/NamedPipesServerClientHandler.hpp
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
	${obs-browser-streamelements_QRC_SOURCES}
	)

target_link_libraries(obs-browser
	${obs-browser_LIBRARIES}
	)

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
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/streamelements/qt.conf"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
	)

	# Copy BugSplat binary files
	foreach( bugsplat_bin_file ${BUGSPLAT_BIN_FILES})
		add_custom_command(TARGET obs-browser POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
			${bugsplat_bin_file}
			"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
		)
	endforeach( bugsplat_bin_file )
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
