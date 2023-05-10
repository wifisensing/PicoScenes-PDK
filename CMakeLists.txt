cmake_minimum_required(VERSION 3.5)
PROJECT(PicoScenes-Three-Plugins)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wno-attributes")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wno-attributes")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/..)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/..)

set(PICOSCENES_BASE_DIR /usr/local/PicoScenes)
include_directories(${PICOSCENES_BASE_DIR}/include/)
link_directories(${PICOSCENES_BASE_DIR}/lib/)
set(INSTALL_PLUGIN_DIR ${PICOSCENES_BASE_DIR}/plugins/)
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fno-stack-check")
endif ()

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS) # to suppress internal warnings of boost
find_package(Boost COMPONENTS filesystem system thread program_options regex REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
find_package(fmt REQUIRED)
add_definitions(-DFMT_DEPRECATED_OSTREAM)

# make plug-ins
add_subdirectory(plugin-demo)
add_subdirectory(plugin-echoprobe)
add_subdirectory(plugin-forwarder)

# include CPack settings
include(CPack-Settings.cmake)