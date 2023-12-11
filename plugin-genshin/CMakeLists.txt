# The PicoScenes Plugins MUST be named in "PDK-xxx" pattern.
ADD_LIBRARY(PDK-Genshin SHARED GenshinPlugin.cxx)
TARGET_LINK_LIBRARIES(PDK-Genshin  ${Boost_LIBRARIES} fmt::fmt SystemTools)
install(TARGETS PDK-Genshin  DESTINATION .)  # Important to use relative path here (used by CPack)!