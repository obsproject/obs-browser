# Fix XCode includes to ignore warnings on system includes
function(target_include_directories_system _target)
  if(XCODE)
    foreach(_arg ${ARGN})
      if("${_arg}" STREQUAL "PRIVATE" OR "${_arg}" STREQUAL "PUBLIC" OR "${_arg}" STREQUAL "INTERFACE")
        set(_scope ${_arg})
      else()
        target_compile_options(${_target} ${_scope} -isystem${_arg})
      endif()
    endforeach()
  else()
    target_include_directories(${_target} SYSTEM ${_scope} ${ARGN})
  endif()
endfunction()


# Install bundles into obs plugin directory
macro(install_obs_plugin_bundle target target-bundle-name)
	if(APPLE)
		set(_bit_suffix "")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_bit_suffix "64bit/")
	else()
		set(_bit_suffix "32bit/")
	endif()

	set_target_properties(${target} PROPERTIES
		PREFIX "")

	install(TARGETS ${target}
		BUNDLE DESTINATION "${OBS_PLUGIN_DESTINATION}")

	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"$<TARGET_FILE_DIR:${target}>/../../../${target-bundle-name}"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/obs-plugins/${_bit_suffix}/${target-bundle-name}"
		VERBATIM)

	if(DEFINED ENV{obsInstallerTempDir})
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_directory
				"$<TARGET_FILE_DIR:${target}>/../../../${target-bundle-name}"
				"$ENV{obsInstallerTempDir}/${OBS_PLUGIN_DESTINATION}/${target-bundle-name}"
			VERBATIM)
	endif()

endmacro()

macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
    set_property(TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY}
        ${XCODE_VALUE})
endmacro (set_xcode_property)