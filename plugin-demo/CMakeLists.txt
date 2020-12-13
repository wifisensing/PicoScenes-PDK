# The PicoScenes Plugins MUST be named in "PDK-xxx" pattern.
ADD_LIBRARY(PDK-demo SHARED DemoPlugin.cpp)
if (APPLE)
    TARGET_LINK_LIBRARIES(PDK-demo Utilities fmt::fmt-header-only EnhancedConsoleText ${Boost_LIBRARIES})
else ()
    TARGET_LINK_LIBRARIES(PDK-demo Utilities ${Boost_LIBRARIES})
endif ()
install(TARGETS PDK-demo DESTINATION ${INSTALL_PLUGIN_DIR})