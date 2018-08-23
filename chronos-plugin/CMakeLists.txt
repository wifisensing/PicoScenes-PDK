file(GLOB ChronosCore_Source UnifiedChronos*.*)
ADD_LIBRARY(PlugIn-UnifiedChronos SHARED ${ChronosCore_Source})
TARGET_LINK_LIBRARIES(PlugIn-UnifiedChronos Ath9kHAL Utilities rxs_parsing fmt ${Boost_LIBRARIES})
install(TARGETS PlugIn-UnifiedChronos DESTINATION ${INSTALL_PLUGIN_DIR})