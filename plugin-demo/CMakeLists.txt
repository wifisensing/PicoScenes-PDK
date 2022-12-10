# The PicoScenes Plugins MUST be named in "PDK-xxx" pattern.
ADD_LIBRARY(PDK-demo SHARED DemoPlugin.cxx)
TARGET_LINK_LIBRARIES(PDK-demo ${Boost_LIBRARIES})
install(TARGETS PDK-demo DESTINATION ${INSTALL_PLUGIN_DIR})