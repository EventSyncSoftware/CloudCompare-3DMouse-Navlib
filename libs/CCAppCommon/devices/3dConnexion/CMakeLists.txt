# Cross-platform 3Dconnexion SpaceMouse support
# Two backends:
#   - navlib (modern Navigation Library SDK, Windows + macOS)
#   - legacy (old 3DxWare C SDK, Windows only)

# ---- Options ----

option( OPTION_3DMOUSE_NAVLIB "Build with 3Dconnexion Navigation Library (modern SDK, Windows + macOS)" OFF )

if( WIN32 )
	option( OPTION_3DMOUSE_LEGACY "Build with legacy 3DxWare SDK (Windows only, for old devices)" OFF )
else()
	set( OPTION_3DMOUSE_LEGACY OFF )
endif()

# Backward compat: old option name still works
if( NOT OPTION_3DMOUSE_NAVLIB AND NOT OPTION_3DMOUSE_LEGACY )
	if( WIN32 )
		option( OPTION_SUPPORT_3DCONNEXION_DEVICES "Build with 3dConnexion libraries (3D mouse support)" OFF )
		if( OPTION_SUPPORT_3DCONNEXION_DEVICES )
			# Old option: enable legacy backend on Windows (original behavior)
			set( OPTION_3DMOUSE_LEGACY ON CACHE BOOL "" FORCE )
		endif()
	endif()
endif()

# ---- Shared sources (added when either backend is enabled) ----

if( OPTION_3DMOUSE_NAVLIB OR OPTION_3DMOUSE_LEGACY )
	target_sources( ${PROJECT_NAME}
		PRIVATE
			${CMAKE_CURRENT_LIST_DIR}/I3DMouseBackend.h
			${CMAKE_CURRENT_LIST_DIR}/cc3DMouseManager.h
			${CMAKE_CURRENT_LIST_DIR}/cc3DMouseManager.cpp
	)

	target_include_directories( ${PROJECT_NAME}
		PUBLIC
			${CMAKE_CURRENT_LIST_DIR}
	)

	target_compile_definitions( ${PROJECT_NAME} PUBLIC CC_3DXWARE_SUPPORT )
endif()

# ---- Navlib backend ----

if( OPTION_3DMOUSE_NAVLIB )
	set( NAVLIB_SDK_DIR "" CACHE PATH "3Dconnexion navlib SDK root directory (contains TDxWare_SDKConfig.cmake)" )

	if( NAVLIB_SDK_DIR )
		# Use the SDK's cmake config to get include dirs and libraries
		find_package( TDxWare_SDK CONFIG PATHS "${NAVLIB_SDK_DIR}" NO_DEFAULT_PATH )
	else()
		find_package( TDxWare_SDK CONFIG QUIET )
	endif()

	if( TDxWare_SDK_FOUND )
		message( STATUS "3Dconnexion navlib SDK found: ${TDxWare_INCLUDE_DIRS}" )
	else()
		message( SEND_ERROR "3Dconnexion navlib SDK not found. Set NAVLIB_SDK_DIR to the SDK root directory." )
	endif()

	target_sources( ${PROJECT_NAME}
		PRIVATE
			${CMAKE_CURRENT_LIST_DIR}/navlib/Mouse3DNavlib.h
			${CMAKE_CURRENT_LIST_DIR}/navlib/Mouse3DNavlib.cpp
	)

	target_include_directories( ${PROJECT_NAME}
		PRIVATE
			${TDxWare_INCLUDE_DIRS}
	)

	target_compile_definitions( ${PROJECT_NAME} PUBLIC CC_3DMOUSE_NAVLIB )
endif()

# ---- Legacy backend (Windows only) ----

if( OPTION_3DMOUSE_LEGACY )
	set( 3DXWARE_INCLUDE_DIR "" CACHE PATH "3DxWare legacy include directory" )
	set( 3DXWARE_LIB_DIR "" CACHE PATH "3DxWare legacy libraries directory" )

	if( NOT 3DXWARE_INCLUDE_DIR )
		message( SEND_ERROR "3DXWARE include path is not specified (3DXWARE_INCLUDE_DIR)" )
	endif()

	if( NOT 3DXWARE_LIB_DIR )
		message( SEND_ERROR "3DXWARE library path is not specified (3DXWARE_LIB_DIR)" )
	endif()

	target_sources( ${PROJECT_NAME}
		PRIVATE
			${CMAKE_CURRENT_LIST_DIR}/legacy/Mouse3DLegacy.h
			${CMAKE_CURRENT_LIST_DIR}/legacy/Mouse3DLegacy.cpp
	)

	target_include_directories( ${PROJECT_NAME}
		PUBLIC
			${3DXWARE_INCLUDE_DIR}
	)

	target_compile_definitions( ${PROJECT_NAME} PUBLIC CC_3DMOUSE_LEGACY )
endif()

# ---- Link function for parent CMakeLists ----

function( target_link_3DXWARE ) # 1 argument: ARGV0 = target
	if( OPTION_3DMOUSE_NAVLIB AND TDxWare_SDK_FOUND )
		target_link_libraries( ${ARGV0} PUBLIC ${TDxWare_LIBRARIES} )
	endif()

	if( OPTION_3DMOUSE_LEGACY )
		target_link_libraries( ${ARGV0} PUBLIC optimized ${3DXWARE_LIB_DIR}/siapp.lib ${3DXWARE_LIB_DIR}/spwmath.lib )

		if( CMAKE_CONFIGURATION_TYPES )
			target_link_libraries( ${ARGV0} PUBLIC debug ${3DXWARE_LIB_DIR}/siapp.lib ${3DXWARE_LIB_DIR}/spwmathD.lib )
		endif()
	endif()
endfunction()
