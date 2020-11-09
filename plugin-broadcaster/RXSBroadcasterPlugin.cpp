//
// Created by Zhiping Jiang on 10/20/17.
//

#include "RXSBroadcasterPlugin.h"

std::string RXSBroadcasterPlugin::getPluginName() {
    return "RXS_Broadcaster";
}

std::string RXSBroadcasterPlugin::getPluginDescription() {
    return "broadcast all received rxs to network via UDP";
}

std::shared_ptr<boost::program_options::options_description> RXSBroadcasterPlugin::pluginOptionsDescription() {
    static auto options = std::make_shared<po::options_description>("Options for plugin " + this->getPluginName());
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        options->add_options()
                ("rxs-broadcaster-dest-ip", po::value<std::string>(), "Destination IP for PlugIn RXS-Broadcaster")
                ("rxs-broadcaster-dest-port", po::value<uint16_t>(), "Destination Port for PlugIn RXS-Broadcaster");
    });
    return options;
}

std::string RXSBroadcasterPlugin::pluginStatus() {
    return "Destination IP/Port: " + destinationIP + ":" + std::to_string(destinationPort);
}


void RXSBroadcasterPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("rxs-broadcaster-dest-ip")) {
        destinationIP = vm["rxs-broadcaster-dest-ip"].as<std::string>();
    }

    if (vm.count("rxs-broadcaster-dest-port")) {
        destinationPort = vm["rxs-broadcaster-dest-port"].as<uint16_t>();
    }
}

void RXSBroadcasterPlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    UDPService::getInstance("RXSExport" + destinationIP + std::to_string(destinationPort))->sendData(rxframe.rawBuffer.get(), rxframe.rawBufferLength, destinationIP, destinationPort);
}