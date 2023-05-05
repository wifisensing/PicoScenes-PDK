ADD_LIBRARY(PDK-UDPForwarder SHARED UDPForwarderPlugin.cxx)
TARGET_LINK_LIBRARIES(PDK-UDPForwarder SystemTools ${Boost_LIBRARIES} fmt::fmt rxs_parsing)
install(TARGETS PDK-UDPForwarder EXPORT PICOSCENES-PDK DESTINATION .)