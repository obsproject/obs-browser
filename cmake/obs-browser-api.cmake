find_package(Qt6 REQUIRED Widgets)

add_library(obs-browser-api INTERFACE)
add_library(OBS::browser-api ALIAS obs-browser-api)

target_sources(obs-browser-api PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/obs-browser-api.hpp>
                                      $<INSTALL_INTERFACE:${OBS_INCLUDE_DESTINATION}/obs-browser-api.hpp>)

target_link_libraries(obs-browser-api INTERFACE OBS::libobs Qt::Widgets)

target_include_directories(obs-browser-api INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/lib"
                                                     "$<INSTALL_INTERFACE:${OBS_INCLUDE_DESTINATION}>")

set_target_properties(obs-browser-api PROPERTIES PREFIX "" PUBLIC_HEADER lib/obs-browser-api.hpp)

target_export(obs-browser-api)
