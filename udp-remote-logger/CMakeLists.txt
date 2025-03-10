add_executable(UDPRemoteLogger UDPRemoteLogger.cxx)
TARGET_LINK_LIBRARIES(UDPRemoteLogger ${Boost_LIBRARIES} fmt::fmt rxs_parsing FrontEnd SystemTools)
if(WIN32)
    find_package(PThreads4W REQUIRED)
    TARGET_LINK_LIBRARIES(UDPRemoteLogger ws2_32 mswsock PThreads4W::PThreads4W)
endif()
install(TARGETS UDPRemoteLogger DESTINATION ../bin)  # Important to use relative path here (used by CPack)!