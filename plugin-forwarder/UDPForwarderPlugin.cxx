//
// Created by Zhiping Jiang on 10/20/17.
//

#include "UDPForwarderPlugin.hxx"
#include "boost/algorithm/string.hpp"

std::string UDPForwarderPlugin::getPluginName() {
    return "UDPForwarder";
}

std::string UDPForwarderPlugin::getPluginDescription() {
    return "forward all received packets to the specified destination.";
}

std::vector<PicoScenesDeviceType> UDPForwarderPlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300, PicoScenesDeviceType::IWLMVM_AX200, PicoScenesDeviceType::IWLMVM_AX210, PicoScenesDeviceType::VirtualSDR, PicoScenesDeviceType::USRP, PicoScenesDeviceType::SoapySDR};
    return supportedDevices;
}

void UDPForwarderPlugin::initialization() {
    options = std::make_shared<po::options_description>("UDPForward Options");
    options->add_options()
            ("forward-to", po::value<std::string>(), "Destination address and port, e.g., 192.168.10.1:50000");
}

std::shared_ptr<boost::program_options::options_description> UDPForwarderPlugin::pluginOptionsDescription() {
    return options;
}

std::string UDPForwarderPlugin::pluginStatus() {
    return "Destination IP/Port: " + destinationIP.value_or("null") + ":" + std::to_string(destinationPort.value_or(0u));
}

void UDPForwarderPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("forward-to")) {
        auto input = vm["forward-to"].as<std::string>();
        std::vector<std::string> segments;
        boost::split(segments, input, boost::is_any_of(":"), boost::token_compress_on);
        boost::trim(segments[0]);
        boost::trim(segments[1]);
        destinationIP = segments[0];
        destinationPort = boost::lexical_cast<uint16_t>(segments[1]);

        LoggingService_info_print("UDP Forwarder destination: {}/{}\n", *destinationIP, *destinationPort);
    }
}

void UDPForwarderPlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    if (destinationIP && destinationPort) {
        auto frameBuffer = rxframe.toBuffer();
        UDPService::getInstance("Forwarder" + *destinationIP + std::to_string(*destinationPort))->sendData(frameBuffer.data(), frameBuffer.size(), *destinationIP, *destinationPort);
    }
}
