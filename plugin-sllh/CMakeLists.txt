# The PicoScenes Plugins MUST be named in "PDK-xxx" pattern.
ADD_LIBRARY(PDK-SL SHARED ShenlilinhuaPlugin.cxx)
TARGET_LINK_LIBRARIES(PDK-SL  ${Boost_LIBRARIES} fmt::fmt SystemTools)
install(TARGETS PDK-SL  DESTINATION .)  # Important to use relative path here (used by CPack)!