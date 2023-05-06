ADD_LIBRARY(PDK-UDPForwarder SHARED UDPForwarderPlugin.cxx)
TARGET_LINK_LIBRARIES(PDK-UDPForwarder SystemTools ${Boost_LIBRARIES} fmt::fmt rxs_parsing)
install(TARGETS PDK-UDPForwarder DESTINATION .)  # Important to use relative path here (used by CPack)!