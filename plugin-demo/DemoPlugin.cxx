//
// Created by Zhiping Jiang on 10/20/17.
//

#include "DemoPlugin.hxx"

std::string DemoPlugin::getPluginName() {
    return "PicoScenes Demo Plugin";
}

std::string DemoPlugin::getPluginDescription() {
    return "Demonstrate the PicoScenes Plugin functionality";
}

std::string DemoPlugin::pluginStatus() {
    return "this method returns the status of the plugin.";
}

std::shared_ptr<boost::program_options::options_description> DemoPlugin::pluginOptionsDescription() {
    static auto instance = std::make_shared<boost::program_options::options_description>("Demo Options based on boost::program_options", 120);
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        instance->add_options()
                ("option1", po::value<uint32_t>(), "You should refer Boost::Program_Options for the help on this thing.");
    });

    return instance;
}

void DemoPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    auto parsedOptions = po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription()).allow_unregistered().style(po::command_line_style::unix_style & ~po::command_line_style::allow_guessing).run();
    po::store(parsedOptions, vm);
    po::notify(vm);

    if (vm.count("option1")) {
        auto option1Value = vm["option1"].as<uint32_t>();
        LoggingService::info_print("Demo Plugin: option1={}.\n", int(option1Value));
    }
}