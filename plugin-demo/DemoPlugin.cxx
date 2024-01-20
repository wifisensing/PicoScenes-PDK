// DemoPlugin.cxx
#include "DemoPlugin.hxx"

#include <boost/algorithm/string/case_conv.hpp>


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

std::shared_ptr<boost::program_options::options_description> DemoPlugin::pluginOptionsDescription() {
    return options;
}

void DemoPlugin::initialization() {

    /* In this area, you can customize commands,
     * but be mindful not to replicate the commands used by other plugins.
     */

    options = std::make_shared<po::options_description>("Demo Options", 120);
    options->add_options()
            ("demo", po::value<std::string>(), "--demo <param>");
}

void DemoPlugin::parseAndExecuteCommands(const std::string &commandString) {

    // Create a variables map to store parsed options
    po::variables_map vm;

    // Define the command line options style
    auto style = pos::allow_long | pos::allow_dash_for_short |
                 pos::long_allow_adjacent | pos::long_allow_next |
                 pos::short_allow_adjacent | pos::short_allow_next;

    // Parse the command string using Boost.ProgramOptions and store options in the variables map
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*options).style(style).allow_unregistered().run(), vm);

    // Notify the variables map about the parsed options
    po::notify(vm);

    // Check if the "demo" option is present
    if (vm.count("demo")) {
        // Get the value of the "demo" option
        auto modeString = vm["demo"].as<std::string>();

        // Check if the modeString contains "logger" and start the Rx service accordingly
        if (modeString.find("logger") != std::string::npos) {
            nic->startRxService();
        }
        // Check if the modeString contains "injector" and start the Tx service with basic frame transmission
        else if (modeString.find("injector") != std::string::npos) {
            nic->startTxService();

            // Generate a random task ID within a specified range
            auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);

            // Build a basic transmission frame with the generated task ID
            auto txframe = buildBasicFrame(taskId);

            // Transmit the PicoScenes frame synchronously
            nic->transmitPicoScenesFrameSync(*txframe);
        }
    }

}

void DemoPlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    LoggingService_debug_print("This is my rxframe: {}",rxframe.toString());
}

std::shared_ptr<ModularPicoScenesTxFrame> DemoPlugin::buildBasicFrame(uint16_t taskId) const
{
    auto frame = nic->initializeTxFrame();

    /**
     * The platform parser will parse the Tx parameters options and store the results in AbstractNIC.
     */

    frame->setTxParameters(nic->getUserSpecifiedTxParameters());
    frame->setTaskId(taskId);
    auto sourceAddr = MagicIntel123456;
    frame->setSourceAddress(sourceAddr.data());
    frame->set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    return frame;

}