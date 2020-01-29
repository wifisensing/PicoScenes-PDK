ADD_LIBRARY(PlugIn-RXSBroadcaster SHARED RXSBroadcasterPlugin.cpp)
TARGET_LINK_LIBRARIES(PlugIn-RXSBroadcaster Utilities ${Boost_LIBRARIES})
install(TARGETS PlugIn-RXSBroadcaster DESTINATION ${INSTALL_PLUGIN_DIR})