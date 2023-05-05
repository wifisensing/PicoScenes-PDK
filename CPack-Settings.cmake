if(APPLE)
    set(CPACK_GENERATOR "productbuild")
	set(CPACK_COMPONENTS_ALL Unspecified)
    set(CPACK_PACKAGE_FILE_NAME "PicoScenes-PDK")
    set(CPACK_PACKAGING_INSTALL_PREFIX ${INSTALL_PLUGIN_DIR})
    include(CPack-AutoVersioning.cmake)

else()
    if(${PICOSCENES_PLATFORM_VERSION})
        message(STATUS "Use given PICOSCENES_PLATFORM_VERSION=${PICOSCENES_PLATFORM_VERSION}")
        set(picoscenes-platform-depencency "(=${PICOSCENES_PLATFORM_VERSION})")
    else(${PICOSCENES_PLATFORM_VERSION})
        message(STATUS "PICOSCENES_PLATFORM_VERSION not defined.")
        set(picoscenes-platform-depencency "")
    endif(${PICOSCENES_PLATFORM_VERSION})

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "picoscenes-plugins-demo-echoprobe-forwarder")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Three Sample Plugins of PicoScenes System \n The plug-ins are: EchoProbe, ms-level round-trip CSI measurement in UWB spectrum; UDP-Forwarder, forwards received frames to the remote via UDP; and Demo, a basic sample code of PicoScenes Plug-In Development Kit (PSPDK). For more information, please visit https://ps.zpj.io. \n\n Wanna see which components are upgraded in this release? Check it out at <https://zpj.io/PicoScenes/pdk-changelog>.")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/changelog.md")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "picoscenes-platform${picoscenes-platform-depencency}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Zhiping Jiang<zpj@xidian.edu.cn>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://zpj.io/ps")
    set(CPACK_PACKAGE_CONTACT "zpj@xidian.edu.cn")
    include(CPack-AutoVersioning.cmake)

endif()

include(CPack)
