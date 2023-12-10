//
// Created by tt on 23-12-10.
//

#include "ShenlilinhuaPlugin.hxx"
#include "boost/algorithm/string.hpp"

std::string ShenLiLingHuaPlugin::getPluginName() {
    return "SLLH de dog";
}

std::string ShenLiLingHuaPlugin::getPluginDescription() {
    return "SLLH.";
}

std::vector<PicoScenesDeviceType> ShenLiLingHuaPlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300, PicoScenesDeviceType::IWLMVM_AX200, PicoScenesDeviceType::IWLMVM_AX210, PicoScenesDeviceType::VirtualSDR, PicoScenesDeviceType::USRP, PicoScenesDeviceType::SoapySDR};
    return supportedDevices;
}

void ShenLiLingHuaPlugin::initialization() {
    options = std::make_shared<po::options_description>("ShenLiLingHuaPlugin Options", 120);
    options->add_options()
            ("sllh", po::value<std::string>(), "Destination address and port, e.g., 192.168.10.1:50000");
}

std::shared_ptr<boost::program_options::options_description> ShenLiLingHuaPlugin::pluginOptionsDescription() {
    return options;
}

std::string ShenLiLingHuaPlugin::pluginStatus() {
    return "this method returns the status of the plugin.";
}

void ShenLiLingHuaPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    auto parsedOptions = po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription()).allow_unregistered().style(po::command_line_style::unix_style & ~po::command_line_style::allow_guessing).run();
    po::store(parsedOptions, vm);
    po::notify(vm);


    if (vm.count("sllh")) {
        auto option1Value = vm["sllh"].as<std::string>();
        boost::algorithm::to_lower(option1Value);
        boost::trim(option1Value);

        // std::cout << option1Value << std::endl;
        LoggingService_Plugin_info_print("我是{}的狗~",std::string(option1Value));


    }
}


