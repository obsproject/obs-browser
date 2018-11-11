include(FindPackageHandleStandardArgs)

SET(BUGSPLAT_ROOT_DIR "" CACHE PATH "Path to BugSplat SDK")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	# 64 bits
	find_library(BUGSPLAT_LIBRARY
		NAMES BugSplat64.lib "BugSplat"
		PATHS ${BUGSPLAT_ROOT_DIR} ${BUGSPLAT_ROOT_DIR}/lib64)

	set(BUGSPLAT_BIN_DIR "${BUGSPLAT_ROOT_DIR}/bin64")
	set(BUGSPLAT_BIN_FILES
		${BUGSPLAT_BIN_DIR}/BsSndRpt64.exe
		${BUGSPLAT_BIN_DIR}/BugSplat64.dll
		${BUGSPLAT_BIN_DIR}/BugSplat64.pdb
		${BUGSPLAT_BIN_DIR}/BugSplatHD64.exe
		${BUGSPLAT_BIN_DIR}/bugsplathd64.pdb
		${BUGSPLAT_BIN_DIR}/BugSplatRc64.dll
	)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	# 32 bits
	set(BUGSPLAT_LIBRARY "${BUGSPLAT_ROOT_DIR}/lib/BugSplat.lib")

	set(BUGSPLAT_BIN_DIR "${BUGSPLAT_ROOT_DIR}/bin")
	set(BUGSPLAT_BIN_FILES
		${BUGSPLAT_BIN_DIR}/BsSndRpt.exe
		${BUGSPLAT_BIN_DIR}/BugSplat.dll
		${BUGSPLAT_BIN_DIR}/BugSplat.pdb
		${BUGSPLAT_BIN_DIR}/BugSplatHD.exe
		${BUGSPLAT_BIN_DIR}/bugsplathd.pdb
		${BUGSPLAT_BIN_DIR}/BugSplatRc.dll
	)
else()
	message(WARNING "Could not detect CPU architecture")
	set(BUGSPLAT_FOUND FALSE)
	return()
endif()

if(NOT BUGSPLAT_LIBRARY)
	message(WARNING "Could not find the BugSplat shared library" )
	set(BUGSPLAT_FOUND FALSE)
	return()
endif()

set(BUGSPLAT_INCLUDE_DIR "${BUGSPLAT_ROOT_DIR}/inc")

set(BUGSPLAT_LIBRARIES
		optimized ${BUGSPLAT_LIBRARY})

find_package_handle_standard_args(BugSplat DEFAULT_MSG BUGSPLAT_LIBRARY BUGSPLAT_INCLUDE_DIR)
mark_as_advanced(BUGSPLAT_LIBRARY BUGSPLAT_LIBRARIES BUGSPLAT_INCLUDE_DIR)
