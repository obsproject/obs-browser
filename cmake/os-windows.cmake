target_compile_options(obs-browser PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)
target_compile_definitions(obs-browser PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)

target_link_libraries(obs-browser PRIVATE CEF::Wrapper CEF::Library d3d11 dxgi)
target_link_options(obs-browser PRIVATE /IGNORE:4099)

add_executable(obs-browser-helper WIN32 EXCLUDE_FROM_ALL)
add_executable(OBS::browser-helper ALIAS obs-browser-helper)

target_sources(
  obs-browser-helper
  PRIVATE cef-headers.hpp
          obs-browser-page/obs-browser-page-main.cpp
          browser-app.cpp
          browser-app.hpp
          obs-browser-page.manifest)

target_include_directories(obs-browser-helper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps"
                                                      "${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page")

target_compile_options(obs-browser-helper PRIVATE $<IF:$<CONFIG:DEBUG>,/MTd,/MT>)
target_compile_definitions(obs-browser-helper PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)

target_link_libraries(obs-browser-helper PRIVATE CEF::Wrapper CEF::Library nlohmann_json::nlohmann_json)
target_link_options(obs-browser-helper PRIVATE /IGNORE:4099 /SUBSYSTEM:WINDOWS)

set(OBS_EXECUTABLE_DESTINATION "${OBS_PLUGIN_DESTINATION}")
set_target_properties_obs(
  obs-browser-helper
  PROPERTIES FOLDER plugins/obs-browser
             PREFIX ""
             OUTPUT_NAME obs-browser-page)
