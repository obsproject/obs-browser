find_qt(COMPONENTS Widgets)

find_library(COREFOUNDATION CoreFoundation)
find_library(APPKIT AppKit)
mark_as_advanced(COREFOUNDATION APPKIT)

target_compile_definitions(obs-browser PRIVATE ENABLE_BROWSER_SHARED_TEXTURE ENABLE_BROWSER_QT_LOOP)
if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.3)
  target_compile_options(obs-browser PRIVATE -Wno-error=unqualified-std-cast-call)
endif()

target_link_libraries(obs-browser PRIVATE Qt::Widgets ${COREFOUNDATION} ${APPKIT} CEF::Wrapper)

set(helper_basename browser-helper)
set(helper_output_name "OBS Helper")
set(helper_suffixes "::" " (GPU):_gpu:.gpu" " (Plugin):_plugin:.plugin" " (Renderer):_renderer:.renderer")

foreach(helper IN LISTS helper_suffixes)
  string(REPLACE ":" ";" helper ${helper})
  list(GET helper 0 helper_name)
  list(GET helper 1 helper_target)
  list(GET helper 2 helper_plist)

  set(target_name ${helper_basename}${helper_target})
  set(target_output_name "${helper_output_name}${helper_name}")
  set(EXECUTABLE_NAME "${target_output_name}")
  set(BUNDLE_ID_SUFFIX ${helper_plist})

  configure_file(cmake/macos/Info-helper.plist.in Info-Helper${helper_plist}.plist)

  add_executable(${target_name} MACOSX_BUNDLE EXCLUDE_FROM_ALL)
  add_executable(OBS::${target_name} ALIAS ${target_name})

  target_sources(${target_name} PRIVATE browser-app.cpp browser-app.hpp obs-browser-page/obs-browser-page-main.cpp
                                        cef-headers.hpp deps/json11/json11.cpp deps/json11/json11.hpp)
  target_compile_definitions(${target_name} PRIVATE ENABLE_BROWSER_SHARED_TEXTURE)
  if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 14.0.3)
    target_compile_options(${target_name} PRIVATE -Wno-error=unqualified-std-cast-call)
  endif()

  target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps"
                                                    "${CMAKE_CURRENT_SOURCE_DIR}/obs-browser-page")

  target_link_libraries(${target_name} PRIVATE CEF::Wrapper)

  set_target_properties(
    ${target_name}
    PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info-Helper${helper_plist}.plist"
               OUTPUT_NAME "${target_output_name}"
               FOLDER plugins/obs-browser/Helpers
               XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.obsproject.obs-studio.helper${helper_plist}
               XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
               "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/entitlements-helper${helper_plist}.plist")
endforeach()

set_target_properties(
  obs-browser
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)
