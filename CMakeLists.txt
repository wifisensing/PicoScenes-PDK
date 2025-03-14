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

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS) # to suppress internal warnings of boost
add_definitions(-DBOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost COMPONENTS filesystem system thread program_options regex unit_test_framework REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
find_package(fmt REQUIRED)

# make plug-ins
add_subdirectory(plugin-demo)
add_subdirectory(plugin-echoprobe)
add_subdirectory(plugin-forwarder)
add_subdirectory(udp-remote-logger)

# include CPack settings
include(CPack-Settings.cmake)