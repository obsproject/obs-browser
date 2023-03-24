find_package(X11 REQUIRED)

target_link_libraries(obs-browser PRIVATE CEF::Wrapper CEF::Library X11::X11)
set_target_properties(obs-browser PROPERTIES BUILD_RPATH "$ORIGIN/" INSTALL_RPATH "$ORIGIN/")

add_executable(browser-helper)
add_executable(OBS::browser-helper ALIAS browser-helper)

target_sources(browser-helper PRIVATE cef-headers.hpp obs-browser-page/obs-browser-page-main.cpp browser-app.cpp
                                      browser-app.hpp deps/json11/json11.cpp deps/json11/json11.hpp)

target_include_directories(browser-helper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps"
                                                  "${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page")

target_link_libraries(browser-helper PRIVATE CEF::Wrapper CEF::Library)

set(OBS_EXECUTABLE_DESTINATION "${OBS_PLUGIN_DESTINATION}")
set_target_properties_obs(
  browser-helper
  PROPERTIES FOLDER plugins/obs-browser
             BUILD_RPATH "$ORIGIN/"
             INSTALL_RPATH "$ORIGIN/"
             PREFIX ""
             OUTPUT_NAME obs-browser-page)
