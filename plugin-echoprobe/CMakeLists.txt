ADD_LIBRARY(PDK-EchoProbe SHARED EchoProbePlugin.cxx EchoProbeParameters.cxx EchoProbeInitiator.cxx EchoProbeResponder.cxx EchoProbeRequestSegment.cxx EchoProbeReplySegment.cxx)
TARGET_LINK_LIBRARIES(PDK-EchoProbe NICHAL ${Boost_LIBRARIES} fmt::fmt SystemTools FrontEnd rxs_parsing)
install(TARGETS PDK-EchoProbe DESTINATION ${INSTALL_PLUGIN_DIR})