ADD_LIBRARY(PDK-EchoProbe SHARED EchoProbePlugin.cxx EchoProbeParameters.cxx EchoProbeInitiator.cxx EchoProbeResponder.cxx EchoProbeRequestSegment.cxx EchoProbeReplySegment.cxx)
TARGET_LINK_LIBRARIES(PDK-EchoProbe NICHAL ${Boost_LIBRARIES} fmt::fmt SystemTools FrontEnd rxs_parsing PlotService)
if (WIN32)
    find_package(PThreads4W REQUIRED)
    TARGET_LINK_LIBRARIES(PDK-EchoProbe PThreads4W::PThreads4W)
endif()
install(TARGETS PDK-EchoProbe DESTINATION .)  # Important to use relative path here (used by CPack)!