//
// Created by Zhiping Jiang on 10/20/17.
//

#include "DemoPlugIn.h"

std::string DemoPlugIn::pluginName() {
    return "Demo_PlugIn";
}

std::string DemoPlugIn::pluginDescription() {
    return "to demonstrate the PlugIn functionality";
}

void DemoPlugIn::initialization() {
    // do something to initialize this plugin.
}

bool DemoPlugIn::RXSHandle(const struct RXS_enhanced *rxs) {
    return false;
}

std::string DemoPlugIn::pluginStatus() {
    return "pluginStatus method returns A used-defined string indicating the status of this PlugIn.";
}

std::shared_ptr<boost::program_options::options_description> DemoPlugIn::pluginOptionsDescription() {
    static auto instance =  std::make_shared<boost::program_options::options_description>("Demo Options based on boost::program_options");
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        instance->add_options()
                ("option1", po::value<uint16_t>(), "You should refer Boost::Program_Options for the help on this thing.");
    });

    return instance;
}

bool DemoPlugIn::handleCommandString(std::string commandString) {
    // The following 3 lines are the "fixed" routine process to parse commandString into option--value map.
    // You just copy these three lines, and done.
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    // you MUST AVOID the naming collision with the main server and other plugins. Hence, a longer and more specific option name is preferred.
    if (vm.count("plugin-demo-option1")) {
        auto value_uint16_t= vm["plugin-demo-option1"].as<uint16_t>();
        // do something with this input option.
        return value_uint16_t;
    }
    
    return true;
}
