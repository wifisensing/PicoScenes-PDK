file(GLOB PlugIn-RXSBroadcaster_Source RXSBroadcasterPlugIn.cpp)
ADD_LIBRARY(PlugIn-RXSBroadcaster SHARED ${PlugIn-RXSBroadcaster_Source})
TARGET_LINK_LIBRARIES(PlugIn-RXSBroadcaster Utilities)
install(TARGETS PlugIn-RXSBroadcaster DESTINATION ${INSTALL_TARGET_DIR})