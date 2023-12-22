//
// Created by Zhiping Jiang on 10/20/17.
//

#include "DemoPlugin.hxx"

#include <boost/algorithm/string/predicate.hpp>


std::string DemoPlugin::getPluginName() {
    return "PicoScenes Demo Plugin";
}

std::string DemoPlugin::getPluginDescription() {
    return "Demonstrate the PicoScenes Plugin functionality";
}

std::string DemoPlugin::pluginStatus() {
    return "this method returns the status of the plugin.";
}

std::vector<PicoScenesDeviceType> DemoPlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300, PicoScenesDeviceType::IWLMVM_AX200, PicoScenesDeviceType::IWLMVM_AX210, PicoScenesDeviceType::VirtualSDR, PicoScenesDeviceType::USRP, PicoScenesDeviceType::SoapySDR};
    return supportedDevices;
}

void DemoPlugin::initialization() {
    options = std::make_shared<po::options_description>("Demo Options", 120);
    options->add_options()
            ("demo", po::value<std::string>(), "--demo <param>");
}

std::shared_ptr<boost::program_options::options_description> DemoPlugin::pluginOptionsDescription() {
    return options;
}


void DemoPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    auto style = pos::allow_long | pos::allow_dash_for_short |
                 pos::long_allow_adjacent | pos::long_allow_next |
                 pos::short_allow_adjacent | pos::short_allow_next;

    po::store(po::command_line_parser(po::split_unix(commandString)).options(*echoProbeOptions).style(style).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("demo")) {
        auto modeString = vm["demo"].as<std::string>();
        boost::algorithm::to_lower(modeString);
        boost::trim(modeString);

        /**
         * Injector and Logger no logger performs stopTx/Rx() because of loopback mode for SDR multi-channel
         */
        if (modeString.find("injector2") != std::string::npos) {
            parameters.workingMode = EchoProbeWorkingMode::Injector;
            nic->startTxService();
        } else if (modeString.find("logger2") != std::string::npos) {
            parameters.workingMode = EchoProbeWorkingMode::Logger;
            nic->startRxService();
        } else
            throw std::invalid_argument("Unsupported --mode option value by EchoProbe plugins.");
    }


}