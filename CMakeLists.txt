cmake_minimum_required(VERSION 3.5)
PROJECT(PicoScenes-Three-Plugins)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(PICOSCENES_BASE_DIR /usr/local/PicoScenes)
include_directories(${PICOSCENES_BASE_DIR}/include/)
link_directories(${PICOSCENES_BASE_DIR}/lib/)
set(INSTALL_PLUGIN_DIR ${PICOSCENES_BASE_DIR}/plugins/)

# boost
find_package(Boost COMPONENTS filesystem system thread program_options regex REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
find_package(fmt REQUIRED)

# make plug-ins
add_subdirectory(plugin-demo)
add_subdirectory(plugin-echoprobe)
add_subdirectory(plugin-broadcaster)

# include CPack settings
include(CPack-Settings.txt)
