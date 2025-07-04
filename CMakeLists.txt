cmake_minimum_required(VERSION 3.5)
PROJECT(PicoScenes-Three-Plugins)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 20)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -Wno-attributes")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wno-attributes")
endif()

if(WIN32)
    add_compile_definitions(
        "VC_EXTRALEAN"
        "WIN32_LEAN_AND_MEAN"
        "NOMINMAX"
        "__SSE2__"
    )
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/..)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/..)

if(WIN32)
    set(PICOSCENES_BASE_DIR "PicoScenes" CACHE PATH "PicoScenes base directory")
    set(INSTALL_PLUGIN_DIR "${PICOSCENES_BASE_DIR}/Plugins/")
    include_directories("C:/Program Files/${PICOSCENES_BASE_DIR}/include/")
    link_directories("C:/Program Files/${PICOSCENES_BASE_DIR}/lib/")
else()
    set(PICOSCENES_BASE_DIR /usr/local/PicoScenes)
    set(INSTALL_PLUGIN_DIR ${PICOSCENES_BASE_DIR}/plugins/)
endif()
include_directories(${PICOSCENES_BASE_DIR}/include/)
link_directories(${PICOSCENES_BASE_DIR}/pslib/)

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