//
// Created by Zhiping Jiang on 10/20/17.
//

#include "UDPBasedPacketForwarder.h"

std::string UDPBasedPacketForwarder::getPluginName() {
    return "Forwarder";
}

std::string UDPBasedPacketForwarder::getPluginDescription() {
    return "forward all received packets to the specified destination.";
}

std::shared_ptr<boost::program_options::options_description> UDPBasedPacketForwarder::pluginOptionsDescription() {
    static auto options = std::make_shared<po::options_description>("Options for plugin " + this->getPluginName());
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        options->add_options()
                ("forward-to-ip", po::value<std::string>(), "Destination IP for PlugIn UDPBasedPacketForwarder")
                ("forward-to-port", po::value<uint16_t>(), "Destination Port for PlugIn UDPBasedPacketForwarder");
    });
    return options;
}

std::string UDPBasedPacketForwarder::pluginStatus() {
    return "Destination IP/Port: " + destinationIP + ":" + std::to_string(destinationPort);
}


void UDPBasedPacketForwarder::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("forward-to-ip")) {
        destinationIP = vm["forward-to-ip"].as<std::string>();
    }

    if (vm.count("forward-to-port")) {
        destinationPort = vm["forward-to-port"].as<uint16_t>();
    }
}

void UDPBasedPacketForwarder::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    auto frameBuffer = rxframe.toBuffer();
    UDPService::getInstance("Forwarder" + destinationIP + std::to_string(destinationPort))->sendData(frameBuffer.data(), frameBuffer.size(), destinationIP, destinationPort);
}