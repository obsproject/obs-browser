#cmake_minimum_required(VERSION 3.0)
cmake_minimum_required(VERSION 2.8.12)
project(cef-isolation)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")

include(BSUtils)

find_package(LibObs REQUIRED)
find_package(CEF REQUIRED)

if(MSVC)
	option(USE_STATIC_CRT "Use static CRT" ON)

	if(USE_STATIC_CRT)
		set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS} /MT")
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS} /MT")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} /MT")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} /MTd")
		set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS} /MT")
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} /MT")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /MT")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} /MTd")
	endif()
endif()

if(APPLE)
	find_library(COCOA_FRAMEWORK Cocoa)
	find_library(APPKIT_FRAMEWORK AppKit)
	find_library(IOSURFACE_FRAMEWORK IOSurface)
endif(APPLE)

if (NOT APPLE)
	# This is needed to load the libraries from a 
	# separate directory
	add_library(obs-browser-facade MODULE
		obs-browser-facade/main.c)
	target_link_libraries(obs-browser-facade
		libobs)
endif()

set(obs-browser_SOURCES
	obs-browser/main.cpp
	obs-browser/main-source.cpp
	shared/browser-source.cpp
	shared/base64.cpp
	shared/util.cpp)

set(obs-browser_HEADERS
	shared/browser-manager.hpp
	shared/browser-source.hpp
	shared/browser-listener.hpp
	shared/browser-settings.hpp
	shared/browser-types.h
	shared/base64.hpp
	shared/browser-types.h
	shared/browser-version.h
	shared/util.hpp)

if (APPLE)
	list(APPEND obs-browser_SOURCES
		shared-apple/browser-bridges.mm
		obs-browser/apple/browser-source-mac.mm
		obs-browser/apple/browser-manager-mac.mm
		obs-browser/apple/browser-source-listener-mac.mm
		obs-browser/apple/cef-isolation-service-manager.mm
		obs-browser/apple/cef-isolation-service.mm
		obs-browser/apple/client-connection-delegate.mm
		obs-browser/apple/graphics-helpers.c)


	list(APPEND obs-browser_HEADERS
		shared-apple/cef-logging.h
		shared-apple/cef-isolation.h
		shared-apple/browser-bridges.h
		obs-browser/apple/browser-source-mac.h
		obs-browser/apple/browser-manager-mac.h
		obs-browser/apple/browser-source-listener-mac.h
		obs-browser/apple/cef-isolation-service.h
		obs-browser/apple/cef-isolation-service-manager.h
		obs-browser/apple/client-connection-delegate.h
		obs-browser/apple/graphics-helpers.h
		obs-browser/apple/texture-ref.hpp)

else (APPLE)
	
	list(APPEND obs-browser_SOURCES
		shared/browser-client.cpp
		shared/browser-task.cpp
		shared/browser-app.cpp
		obs-browser/browser-source-listener-base.cpp
		obs-browser/browser-manager-base.cpp
		obs-browser/browser-render-handler.cpp
		obs-browser/browser-source-base.cpp
		obs-browser/browser-load-handler.cpp
		obs-browser/browser-obs-bridge-base.cpp
		shared/browser-scheme.cpp
		fmt/format.cc)

	list(APPEND obs-browser_HEADERS
		shared/browser-client.hpp
		shared/browser-task.hpp
		shared/browser-app.hpp
		obs-browser/browser-source-listener-base.hpp
		obs-browser/browser-manager-base.hpp
		obs-browser/browser-render-handler.hpp
		obs-browser/browser-source-base.hpp
		obs-browser/browser-load-handler.hpp
		obs-browser/browser-obs-bridge-base.hpp
		shared/browser-scheme.hpp
		fmt/format.h)

endif(APPLE)

set(obs-browser_LIBRARIES
	obs-frontend-api
	${OBS_JANSSON_IMPORT})

if (APPLE)
	list(APPEND obs-browser_LIBRARIES
		${IOSURFACE_FRAMEWORK})
endif(APPLE)


add_library(obs-browser MODULE 
	${obs-browser_SOURCES} 
	${obs-browser_HEADERS})


target_include_directories(obs-browser PRIVATE 
	"obs-browser" 
	"shared")

target_include_directories(obs-browser PUBLIC
	${OBS_JANSSON_INCLUDE_DIRS})

if (APPLE)
	target_include_directories(obs-browser PUBLIC 
		"shared-apple")
else (APPLE)
	target_include_directories_system(obs-browser PUBLIC ${CEF_ROOT_DIR})
endif(APPLE)


if (APPLE)
	set_xcode_property(obs-browser CLANG_CXX_LIBRARY "libc++")
endif(APPLE)

if (NOT APPLE)
	list(APPEND obs-browser_LIBRARIES
		${CEF_LIBRARIES})
	if (WIN32)
		list(APPEND obs-browser_LIBRARIES
			w32-pthreads)
	endif(WIN32)
else(NOT APPLE)
	list(APPEND obs-browser_LIBRARIES 
		${COCOA_FRAMEWORK} 
		${APPKIT_FRAMEWORK}
		${IOSURFACE_FRAMEWORK})
endif(NOT APPLE)

target_link_libraries(obs-browser 
        ${obs-browser_LIBRARIES})

if(APPLE)
	set(cef-isolation_SOURCES
		cef-isolation/main.mm
		cef-isolation/cef-isolated-client.mm
		cef-isolation/browser-obs-bridge-mac.mm
		cef-isolation/browser-handle.mm
		cef-isolation/browser-render-handler.mm
		cef-isolation/browser-texture-mac.mm
		cef-isolation/service-connection-delegate.mm
		shared/browser-client.cpp
		shared/browser-task.cpp
		shared/browser-app.cpp
		shared/browser-scheme.cpp
		shared/base64.cpp
		obs-browser/browser-load-handler.cpp
		fmt/format.cc)

	set(cef-isolation_HEADERS
		shared-apple/cef-logging.h
		shared-apple/cef-isolation.h
		cef-isolation/cef-isolated-client.h
		cef-isolation/browser-handle.h
		cef-isolation/browser-render-handler.hpp
		cef-isolation/browser-texture-mac.h
		cef-isolation/service-connection-delegate.h
		cef-isolation/browser-obs-bridge-mac.hpp
		shared/browser-texture.hpp
		shared/browser-client.hpp
		shared/browser-task.hpp
		shared/browser-app.hpp
		shared/base64.hpp
		obs-browser/browser-load-handler.hpp
		shared/browser-scheme.hpp
		shared/browser-types.h
		shared/browser-obs-bridge.hpp
		shared-apple/browser-bridges.h)

	add_executable(cef-isolation
		${cef-isolation_SOURCES}
		${cef-isolation_HEADERS})

	set(MACOSX_DEPLOYMENT_TARGET "10.8")

	if (XCODE)
		set_xcode_property(cef-isolation
			CLANG_CXX_LIBRARY "libc++")
	endif(XCODE)

	target_include_directories(cef-isolation PRIVATE 
		"cef-isolation" 
		"shared"
		"shared-apple")
	
	target_include_directories_system(cef-isolation
		PUBLIC
			${CEF_INCLUDE_DIR}
			${OBS_JANSSON_INCLUDE_DIRS})
	
	target_link_libraries(cef-isolation
		${CEF_LIBRARIES}
		${OBS_JANSSON_IMPORT}
		${APPKIT_FRAMEWORK}
		${IOSURFACE_FRAMEWORK})

endif(APPLE)

set(cef-bootstrap_SOURCES
	cef-bootstrap/main.cpp
	shared/browser-app.cpp
	fmt/format.cc)

set(cef-bootstrap_HEADERS
	shared/browser-app.hpp
	fmt/format.h)

add_executable(cef-bootstrap 
	${cef-bootstrap_SOURCES} 
	${cef-bootstrap_HEADERS})

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

target_include_directories(cef-bootstrap PRIVATE "shared")
target_include_directories_system(cef-bootstrap
	PUBLIC
		${CEF_ROOT_DIR}
		${OBS_JANSSON_INCLUDE_DIRS})

target_link_libraries(cef-bootstrap
	${CEF_LIBRARIES}
	${OBS_JANSSON_IMPORT})

if (APPLE)

	set_target_properties(cef-isolation PROPERTIES
		OUTPUT_NAME "CEF"
		MACOSX_BUNDLE TRUE
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@executable_path/.."
		MACOSX_BUNDLE_BUNDLE_NAME "CEF Helper"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.catchexception.cef.cef-isolation")

	set_target_properties(cef-bootstrap PROPERTIES
		OUTPUT_NAME "CEF Helper"
		MACOSX_BUNDLE TRUE
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@executable_path/../../../.."
		MACOSX_BUNDLE_BUNDLE_NAME "CEF Helper"
		MACOSX_BUNDLE_GUI_IDENTIFIER "org.catchexception.cef.cef-bootstrap")


	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND install_name_tool -change "@executable_path/Chromium Embedded Framework" "@rpath/CEF.app/Contents/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "$<TARGET_FILE:obs-browser>"
		)

	add_custom_command(TARGET cef-isolation
		# Disable taskbar visibility
		COMMAND defaults write "$<TARGET_FILE_DIR:cef-isolation>/../Info.plist" "LSUIElement" 1
		)

	add_custom_command(TARGET cef-bootstrap POST_BUILD
		# Remember if you change this but don't cause a recompile it will not rename it successfully
		COMMAND install_name_tool -change "@executable_path/Chromium Embedded Framework" "@rpath/Chromium Embedded Framework.framework/Chromium Embedded Framework" "$<TARGET_FILE:cef-bootstrap>"
		
		# Disable taskbar visibility
		COMMAND defaults write "$<TARGET_FILE_DIR:cef-bootstrap>/../Info.plist" "LSUIElement" 1
		)

	add_custom_command(TARGET cef-isolation POST_BUILD
		# Remember if you change this but don't cause a recompile in the shared object it will not rename it successfully
		COMMAND install_name_tool -change "@executable_path/Chromium Embedded Framework" "@rpath/Chromium Embedded Framework.framework/Chromium Embedded Framework" "$<TARGET_FILE:cef-isolation>"		

		# Make a Frameworks directory
		COMMAND mkdir -p "$<TARGET_FILE_DIR:cef-isolation>/../Frameworks"

		# Copy the CEF and support frameworks
		COMMAND cp -Rf "${CEF_ROOT_DIR}/Release/Chromium Embedded Framework.framework" "$<TARGET_FILE_DIR:cef-isolation>/../Frameworks/"

		# Setup the helper apps
		COMMAND rm -rf "$<TARGET_FILE_DIR:cef-isolation>/../Frameworks/CEF Helper.app"
		COMMAND cp -Rf "$<TARGET_FILE_DIR:cef-bootstrap>/../../../CEF Helper.app" "$<TARGET_FILE_DIR:cef-isolation>/../Frameworks"
		)
		
endif(APPLE)

if(APPLE)
	install_obs_plugin_with_data(obs-browser data)
	install_obs_plugin_bundle(cef-isolation "CEF.app")
else(APPLE)
	install_obs_plugin_with_data(obs-browser data)
	install_obs_plugin(cef-bootstrap)
endif(APPLE)
