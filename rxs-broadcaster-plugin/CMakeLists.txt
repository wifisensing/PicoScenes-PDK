ADD_LIBRARY(PlugIn-RXSBroadcaster SHARED RXSBroadcasterPlugIn.cpp)
TARGET_LINK_LIBRARIES(PlugIn-RXSBroadcaster Utilities)
#install(TARGETS PlugIn-RXSBroadcaster DESTINATION ${INSTALL_TARGET_DIR})