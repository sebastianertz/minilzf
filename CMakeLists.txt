cmake_minimum_required(VERSION 3.13)

project(liblzf VERSION 3.6 LANGUAGES C)

option(BUILD_SHARED_LIBS "Enable build of shared libraries" NO)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LIBRARY_NAME lzf)

# Extract API version from lzf.h
file(READ lzf.h lzf_content)
if(lzf_content MATCHES ".*#define LZF_VERSION (0x[0-9A-Fa-f]+).*")
	math(EXPR API_VERSION_MAJOR "${CMAKE_MATCH_1} >> 8")
	math(EXPR API_VERSION_MINOR "${CMAKE_MATCH_1} & 255")
	set(API_VERSION "${API_VERSION_MAJOR}.${API_VERSION_MINOR}")
	message(STATUS "liblzf API ${API_VERSION}")
else()
	message(FATAL_ERROR "cannot extract API version from lzf.h")
endif()

# library
add_library(${PROJECT_NAME} lzf.c lzf.h)
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)
set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION ${API_VERSION}
	SOVERSION ${API_VERSION_MAJOR}
	OUTPUT_NAME ${LIBRARY_NAME}
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

add_library(embed INTERFACE)
target_include_directories(embed INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)
target_sources(embed INTERFACE
	$<BUILD_INTERFACE:lzf.c>
	$<INSTALL_INTERFACE:src/${PROJECT_NAME}/lzf.c>
)
add_library(${PROJECT_NAME}::embed ALIAS embed)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	target_compile_options(${PROJECT_NAME} PRIVATE "-funroll-all-loops")
endif()

install(TARGETS ${PROJECT_NAME} embed EXPORT ${PROJECT_NAME}-targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES lzf.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})
install(FILES lzf.c DESTINATION src/${PROJECT_NAME})

# cmake files
install(EXPORT ${PROJECT_NAME}-targets NAMESPACE ${PROJECT_NAME}:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
export(EXPORT ${PROJECT_NAME}-targets NAMESPACE ${PROJECT_NAME}::)
configure_package_config_file(${PROJECT_NAME}-config.cmake.in ${PROJECT_NAME}-config.cmake
	PATH_VARS CMAKE_INSTALL_INCLUDEDIR
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
	NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file(${PROJECT_NAME}-config-version.cmake COMPATIBILITY SameMajorVersion)
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# pkgconfig file
configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
