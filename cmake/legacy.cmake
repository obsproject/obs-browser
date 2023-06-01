project(obs-browser)

option(ENABLE_BROWSER "Enable building OBS with browser source plugin (required Chromium Embedded Framework)"
       ${OS_LINUX})

if(NOT ENABLE_BROWSER OR NOT ENABLE_UI)
  message(STATUS "OBS:  DISABLED   obs-browser")
  message(
    WARNING
      "Browser source support is not enabled by default - please switch ENABLE_BROWSER to ON and specify CEF_ROOT_DIR to enable this functionality."
  )
  return()
endif()

if(OS_WINDOWS)
  option(ENABLE_BROWSER_LEGACY "Use legacy CEF version 3770 for browser source plugin" OFF)
endif()

if(OS_MACOS OR OS_WINDOWS)
  option(ENABLE_BROWSER_SHARED_TEXTURE "Enable shared texture support for browser source plugin" ON)
endif()

option(ENABLE_BROWSER_PANELS "Enable Qt web browser panel support" ON)
option(ENABLE_BROWSER_QT_LOOP "Enable running CEF on the main UI thread alongside Qt" ${OS_MACOS})

mark_as_advanced(ENABLE_BROWSER_LEGACY ENABLE_BROWSER_SHARED_TEXTURE ENABLE_BROWSER_PANELS ENABLE_BROWSER_QT_LOOP)

find_package(CEF REQUIRED)

if(NOT TARGET CEF::Wrapper)
  message(
    FATAL_ERROR "OBS:    -        Unable to find CEF Libraries - set CEF_ROOT_DIR or configure with ENABLE_BROWSER=OFF")
endif()

find_package(nlohmann_json REQUIRED)

add_library(obs-browser MODULE)
add_library(OBS::browser ALIAS obs-browser)

if(ENABLE_BROWSER_LEGACY)
  target_compile_definitions(obs-browser PRIVATE ENABLE_BROWSER_LEGACY)
endif()

if(ENABLE_BROWSER_SHARED_TEXTURE)
  target_compile_definitions(obs-browser PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
endif()

if(ENABLE_BROWSER_QT_LOOP)
  target_compile_definitions(obs-browser PRIVATE ENABLE_BROWSER_QT_LOOP)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/browser-config.h.in ${CMAKE_BINARY_DIR}/config/browser-config.h)

target_sources(
  obs-browser
  PRIVATE obs-browser-plugin.cpp
          obs-browser-source.cpp
          obs-browser-source.hpp
          obs-browser-source-audio.cpp
          browser-app.cpp
          browser-app.hpp
          browser-client.cpp
          browser-client.hpp
          browser-scheme.cpp
          browser-scheme.hpp
          browser-version.h
          cef-headers.hpp
          deps/base64/base64.cpp
          deps/base64/base64.hpp
          deps/wide-string.cpp
          deps/wide-string.hpp
          deps/signal-restore.cpp
          deps/signal-restore.hpp
          deps/obs-websocket-api/obs-websocket-api.h
          ${CMAKE_BINARY_DIR}/config/browser-config.h)

target_include_directories(obs-browser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps ${CMAKE_BINARY_DIR}/config)

target_link_libraries(obs-browser PRIVATE OBS::libobs OBS::frontend-api nlohmann_json::nlohmann_json)

target_compile_features(obs-browser PRIVATE cxx_std_17)

if(ENABLE_BROWSER_PANELS OR ENABLE_BROWSER_QT_LOOP)
  find_qt(COMPONENTS Widgets)

  set_target_properties(
    obs-browser
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)

  target_link_libraries(obs-browser PRIVATE Qt::Widgets)
endif()

if(NOT OS_MACOS OR ENABLE_BROWSER_LEGACY)
  add_executable(obs-browser-page)

  target_sources(obs-browser-page PRIVATE cef-headers.hpp obs-browser-page/obs-browser-page-main.cpp browser-app.cpp
                                          browser-app.hpp)

  target_link_libraries(obs-browser-page PRIVATE CEF::Library nlohmann_json::nlohmann_json)

  target_include_directories(obs-browser-page PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/deps
                                                      ${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page)

  target_compile_features(obs-browser-page PRIVATE cxx_std_17)

  if(OS_WINDOWS)

    if(TARGET CEF::Wrapper_Debug)
      target_link_libraries(obs-browser-page PRIVATE optimized CEF::Wrapper)
      target_link_libraries(obs-browser-page PRIVATE debug CEF::Wrapper_Debug)
    else()
      target_link_libraries(obs-browser-page PRIVATE CEF::Wrapper)
    endif()

    target_sources(obs-browser-page PRIVATE obs-browser-page.manifest)
  else()
    target_link_libraries(obs-browser-page PRIVATE CEF::Wrapper)
  endif()

  if(ENABLE_BROWSER_LEGACY)
    target_compile_definitions(obs-browser-page PRIVATE ENABLE_BROWSER_LEGACY)
  endif()

  if(ENABLE_BROWSER_SHARED_TEXTURE)
    target_compile_definitions(obs-browser-page PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
  endif()

  if(ENABLE_BROWSER_QT_LOOP)
    target_compile_definitions(obs-browser-page PRIVATE ENABLE_BROWSER_QT_LOOP)
  endif()

  set_target_properties(obs-browser-page PROPERTIES FOLDER "plugins/obs-browser")

  setup_plugin_target(obs-browser-page)
endif()

if(OS_WINDOWS)
  if(MSVC)
    target_compile_options(obs-browser PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)

    target_compile_options(obs-browser-page PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)
  endif()

  target_link_libraries(obs-browser PRIVATE CEF::Library d3d11 dxgi)

  if(TARGET CEF::Wrapper_Debug)
    target_link_libraries(obs-browser PRIVATE optimized CEF::Wrapper)
    target_link_libraries(obs-browser PRIVATE debug CEF::Wrapper_Debug)
  else()
    target_link_libraries(obs-browser PRIVATE CEF::Wrapper)
  endif()

  target_link_options(obs-browser PRIVATE "LINKER:/IGNORE:4099")

  target_link_options(obs-browser-page PRIVATE "LINKER:/IGNORE:4099" "LINKER:/SUBSYSTEM:WINDOWS")

  list(APPEND obs-browser_LIBRARIES d3d11 dxgi)

elseif(OS_MACOS)
  find_library(COREFOUNDATION CoreFoundation)
  find_library(APPKIT AppKit)
  mark_as_advanced(COREFOUNDATION APPKIT)

  target_link_libraries(obs-browser PRIVATE ${COREFOUNDATION} ${APPKIT} CEF::Wrapper)

  target_sources(obs-browser PRIVATE macutil.mm)

  set(CEF_HELPER_TARGET "obs-browser-helper")
  set(CEF_HELPER_OUTPUT_NAME "OBS Helper")
  set(CEF_HELPER_APP_SUFFIXES "::" " (GPU):_gpu:.gpu" " (Plugin):_plugin:.plugin" " (Renderer):_renderer:.renderer")

  foreach(_SUFFIXES ${CEF_HELPER_APP_SUFFIXES})
    string(REPLACE ":" ";" _SUFFIXES ${_SUFFIXES})
    list(GET _SUFFIXES 0 _NAME_SUFFIX)
    list(GET _SUFFIXES 1 _TARGET_SUFFIX)
    list(GET _SUFFIXES 2 _PLIST_SUFFIX)

    set(_HELPER_TARGET "${CEF_HELPER_TARGET}${_TARGET_SUFFIX}")
    set(_HELPER_OUTPUT_NAME "${CEF_HELPER_OUTPUT_NAME}${_NAME_SUFFIX}")

    set(_HELPER_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/helper-info${_PLIST_SUFFIX}.plist")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/helper-info.plist" _PLIST_CONTENTS)
    string(REPLACE "\${EXECUTABLE_NAME}" "${_HELPER_OUTPUT_NAME}" _PLIST_CONTENTS ${_PLIST_CONTENTS})
    string(REPLACE "\${PRODUCT_NAME}" "${_HELPER_OUTPUT_NAME}" _PLIST_CONTENTS ${_PLIST_CONTENTS})
    string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_PLIST_SUFFIX}" _PLIST_CONTENTS ${_PLIST_CONTENTS})
    string(REPLACE "\${MINIMUM_VERSION}" "${CMAKE_OSX_DEPLOYMENT_TARGET}" _PLIST_CONTENTS ${_PLIST_CONTENTS})
    string(REPLACE "\${CURRENT_YEAR}" "${CURRENT_YEAR}" _PLIST_CONTENTS ${_PLIST_CONTENTS})
    file(WRITE ${_HELPER_INFO_PLIST} ${_PLIST_CONTENTS})

    set(MACOSX_BUNDLE_GUI_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}.helper${_PLIST_SUFFIX}")

    add_executable(${_HELPER_TARGET} MACOSX_BUNDLE)
    add_executable(OBS::browser-helper${_TARGET_SUFFIX} ALIAS ${_HELPER_TARGET})
    target_sources(${_HELPER_TARGET} PRIVATE browser-app.cpp browser-app.hpp obs-browser-page/obs-browser-page-main.cpp
                                             cef-headers.hpp)

    target_link_libraries(${_HELPER_TARGET} PRIVATE CEF::Wrapper nlohmann_json::nlohmann_json)

    target_include_directories(${_HELPER_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/deps
                                                         ${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page)

    target_compile_features(${_HELPER_TARGET} PRIVATE cxx_std_17)

    if(ENABLE_BROWSER_SHARED_TEXTURE)
      target_compile_definitions(${_HELPER_TARGET} PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
    endif()

    set_target_properties(
      ${_HELPER_TARGET}
      PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${_HELPER_INFO_PLIST}
                 OUTPUT_NAME ${_HELPER_OUTPUT_NAME}
                 FOLDER "plugins/obs-browser/helpers/"
                 XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.obsproject.obs-studio.helper${_PLIST_SUFFIX}"
                 XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
                 "${CMAKE_SOURCE_DIR}/cmake/bundle/macOS/entitlements-helper${_PLIST_SUFFIX}.plist")
  endforeach()

elseif(OS_POSIX)
  find_package(X11 REQUIRED)

  target_link_libraries(obs-browser PRIVATE CEF::Wrapper CEF::Library X11::X11)

  get_target_property(_CEF_DIRECTORY CEF::Library INTERFACE_LINK_DIRECTORIES)

  set_target_properties(obs-browser PROPERTIES BUILD_RPATH "$ORIGIN/")

  set_target_properties(obs-browser-page PROPERTIES BUILD_RPATH "$ORIGIN/")

  set_target_properties(obs-browser PROPERTIES INSTALL_RPATH "$ORIGIN/")
  set_target_properties(obs-browser-page PROPERTIES INSTALL_RPATH "$ORIGIN/")
endif()

if(ENABLE_BROWSER_PANELS)
  add_library(obs-browser-panels INTERFACE)
  add_library(OBS::browser-panels ALIAS obs-browser-panels)
  target_sources(
    obs-browser-panels INTERFACE panel/browser-panel.cpp panel/browser-panel.hpp panel/browser-panel-client.cpp
                                 panel/browser-panel-client.hpp panel/browser-panel-internal.hpp)

  target_include_directories(obs-browser-panels INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/panel)

  target_link_libraries(obs-browser-panels INTERFACE CEF::Wrapper)

  if(OS_MACOS)
    target_link_libraries(obs-browser-panels INTERFACE objc)
  endif()

  target_link_libraries(obs-browser PRIVATE obs-browser-panels)

  target_compile_definitions(obs-browser-panels INTERFACE BROWSER_AVAILABLE)

  if(ENABLE_BROWSER_QT_LOOP)
    target_compile_definitions(obs-browser-panels INTERFACE ENABLE_BROWSER_QT_LOOP)
  endif()
endif()

set_target_properties(obs-browser PROPERTIES FOLDER "plugins/obs-browser" PREFIX "")

setup_plugin_target(obs-browser)
