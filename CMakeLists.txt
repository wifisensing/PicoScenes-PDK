cmake_minimum_required(VERSION 3.5)
PROJECT(PicoScenes)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set (CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE  "-O3")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/..)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${PROJECT_BINARY_DIR}/..)
set(INSTALL_HEADER_DIR /usr/share/PicoScenes/include)
set(INSTALL_TARGET_DIR /usr/share/PicoScenes/bin)

# boost
find_package(Boost COMPONENTS filesystem system program_options regex REQUIRED)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

# include various of settings.
include(Build-Settings.txt)

# make plug-ins
add_subdirectory(plugin-demo)
add_subdirectory(chronos-plugin)
add_subdirectory(rxs-broadcaster-plugin)

# include CPack settings
include(CPack-Settings.txt)


