//
// Created by Zhiping Jiang on 10/20/17.
//

#include "UDPForwarderPlugin.hxx"
#include "boost/algorithm/string.hpp"

std::string UDPForwarderPlugin::getPluginName() {
    return "Forwarder";
}

std::string UDPForwarderPlugin::getPluginDescription() {
    return "forward all received packets to the specified destination.";
}

std::shared_ptr<boost::program_options::options_description> UDPForwarderPlugin::pluginOptionsDescription() {
    static auto options = std::make_shared<po::options_description>("Options for plugin " + this->getPluginName());
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        options->add_options()
                ("forward-to", po::value<std::string>(), "Destination address and port, e.g., 192.168.10.1:50000");
    });
    return options;
}

std::string UDPForwarderPlugin::pluginStatus() {
    return "Destination IP/Port: " + destinationIP + ":" + std::to_string(destinationPort);
}

void UDPForwarderPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("forward-to")) {
        auto input = destinationIP = vm["forward-to"].as<std::string>();
        std::vector<std::string> segments;
        boost::split(segments, input, boost::is_any_of(":"), boost::token_compress_on);
        boost::trim(segments[0]);
        boost::trim(segments[1]);
        destinationIP = segments[0];
        destinationPort = boost::lexical_cast<uint16_t>(segments[1]);

        LoggingService_info_print("UDP Forwarder destination: {}/{}\n", destinationIP, destinationPort);
    }
}

void UDPForwarderPlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    auto frameBuffer = rxframe.toBuffer();
    UDPService::getInstance("Forwarder" + destinationIP + std::to_string(destinationPort))->sendData(frameBuffer.data(), frameBuffer.size(), destinationIP, destinationPort);
}