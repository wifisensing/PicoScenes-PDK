//
// Created by Zhiping Jiang on 10/20/17.
//

#include "RXSBroadcasterPlugIn.h"

std::string RXSBroadcasterPlugIn::pluginName() {
    return "RXS_Broadcaster";
}

std::string RXSBroadcasterPlugIn::pluginDescription() {
    return "broadcast all received rxs to network via UDP";
}

std::shared_ptr<boost::program_options::options_description> RXSBroadcasterPlugIn::pluginOptionsDescription() {
    static auto options = std::make_shared<po::options_description>("Options for PlugIn " + this->pluginName());
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        options->add_options()
                ("rxs-broadcaster-dest-ip", po::value<std::string>(), "Destination IP for PlugIn RXS-Broadcaster")
                ("rxs-broadcaster-dest-port", po::value<uint16_t>(), "Destination Port for PlugIn RXS-Broadcaster");
    });
    return options;
}

std::string RXSBroadcasterPlugIn::pluginStatus() {
    return "Destination IP/Port: "+destinationIP+":"+ std::to_string(destinationPort);
}


bool RXSBroadcasterPlugIn::handleCommandString(std::string commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("rxs-broadcaster-dest-ip")) {
        destinationIP = vm["rxs-broadcaster-dest-ip"].as<std::string>();
    }

    if (vm.count("rxs-broadcaster-dest-port")) {
        destinationPort = vm["rxs-broadcaster-dest-port"].as<uint16_t>();
    }

    return true;
}

bool RXSBroadcasterPlugIn::RXSHandle(const struct RXS_enhanced *rxs) {
    UDPService::getInstance("RXSExport"+destinationIP+std::to_string(destinationPort))->sendData(rxs->rawBuffer, rxs->rawBufferLength, destinationIP, destinationPort);
    return false;
}

void RXSBroadcasterPlugIn::initialization() {
    AbstractRXSPlugIn::initialization();
}


void RXSBroadcasterPlugIn::serialize() {
    propertyDescriptionTree.clear();
    propertyDescriptionTree.put("rxs-broadcaster-dest-ip", destinationIP);
    propertyDescriptionTree.put("rxs-broadcaster-dest-port", destinationPort);
}

property_tree::ptree RXSBroadcasterPlugIn::plugInRESTfulPOST(const pt::ptree &request) {
    if (request.count("json")) {
        auto jsonPart = request.get_child("json");
        if (jsonPart.count("rxs-broadcaster-dest-ip") > 0) {
            destinationIP = jsonPart.get<std::string>("rxs-broadcaster-dest-ip");
        }
        if (jsonPart.count("rxs-broadcaster-dest-port") > 0) {
            destinationPort = jsonPart.get<uint16_t>("rxs-broadcaster-dest-port");
        }
    }
    return pt::ptree();
}
