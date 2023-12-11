//
// Created by tt on 23-12-11.
//

#include "GenshinPlugin.hxx"


std::string GenshinPlugin::getPluginName() {
    return "Genshin Plugin";
}

std::string GenshinPlugin::getPluginDescription() {
    return " Plugin drived by Mihoyo";
}

std::string GenshinPlugin::pluginStatus() {
    return "this method returns the status of the plugin.";
}

std::vector<PicoScenesDeviceType> GenshinPlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300, PicoScenesDeviceType::IWLMVM_AX200, PicoScenesDeviceType::IWLMVM_AX210, PicoScenesDeviceType::VirtualSDR, PicoScenesDeviceType::USRP, PicoScenesDeviceType::SoapySDR};
    return supportedDevices;
}

void GenshinPlugin::initialization() {
    options = std::make_shared<po::options_description>("Genshin Options", 120);
    options->add_options()
            ("genshin", po::value<std::string>(), "--genshin 神里绫华")
            ("genshin2", po::value<std::vector<std::string>>()->multitoken(), "Genshin2");
}

std::shared_ptr<boost::program_options::options_description> GenshinPlugin::pluginOptionsDescription() {
    return options;
}


void GenshinPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    auto parsedOptions = po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription()).allow_unregistered().style(po::command_line_style::unix_style & ~po::command_line_style::allow_guessing).run();
    po::store(parsedOptions, vm);
    po::notify(vm);
    if (vm.count("genshin")) {
        auto genshinValue = vm["genshin"].as<std::string>();
        boost::algorithm::to_lower(genshinValue);
        boost::trim(genshinValue);
        LoggingService_Plugin_info_print("我是{}的狗~",std::string(genshinValue));
    }
    if (vm.count("genshin2")) {
        auto genshin2Value = vm["genshin2"].as<std::vector<std::string>>();
        std::string concatenatedString;
        for (const auto& genshin2_value : genshin2Value)
        {
            concatenatedString += genshin2_value;
        }
        LoggingService_Plugin_info_print("我是{}的狗~",std::string(concatenatedString));
    }
}