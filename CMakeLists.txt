cmake_minimum_required(VERSION 3.5)
PROJECT(PicoScenes-Three-Plugins)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set (CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE  "-O3")

set(PICOSCENES_BASE_DIR /usr/share/PicoScenes)
include_directories(${PICOSCENES_BASE_DIR}/include/)
link_directories(${PICOSCENES_BASE_DIR}/lib/)
set(INSTALL_PLUGIN_DIR ${PICOSCENES_BASE_DIR}/plug-ins/)

# make plug-ins
add_subdirectory(plugin-demo)
add_subdirectory(chronos-plugin)
add_subdirectory(rxs-broadcaster-plugin)

# include CPack settings
include(CPack-Settings.txt)
