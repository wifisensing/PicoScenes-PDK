file(GLOB EchoProbeCore_Source EchoProbe*.*)
ADD_LIBRARY(PlugIn-EchoProbe SHARED ${EchoProbeCore_Source})
TARGET_LINK_LIBRARIES(PlugIn-EchoProbe Ath9kHAL Utilities rxs_parsing fmt ${Boost_LIBRARIES})
install(TARGETS PlugIn-EchoProbe DESTINATION ${INSTALL_PLUGIN_DIR})