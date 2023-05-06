# The PicoScenes Plugins MUST be named in "PDK-xxx" pattern.
ADD_LIBRARY(PDK-demo SHARED DemoPlugin.cxx)
TARGET_LINK_LIBRARIES(PDK-demo ${Boost_LIBRARIES} fmt::fmt SystemTools)
install(TARGETS PDK-demo DESTINATION .)  # Important to use relative path here (used by CPack)!