//
// Created by Zhiping Jiang on 10/20/17.
//
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

    po::store(po::command_line_parser(po::split_unix(commandString)).options(*options).style(style).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("demo"))
    {
        auto modeString = vm["demo"].as<std::string>();
        if (modeString.find("logger") != std::string::npos) {
            nic->startRxService();
        }
        else if (modeString.find("injector") != std::string::npos)
        {
            nic->startTxService();
            auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
            auto txframe = buildBasicFrame(taskId);
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
     * @brief PicoScenes Platform CLI parser has *absorbed* the common Tx parameters.
     * The platform parser will parse the Tx parameters options and store the results in AbstractNIC.
     * Plugin developers now can access the parameters via a new method nic->getUserSpecifiedTxParameters().
     */

    frame->setTxParameters(nic->getUserSpecifiedTxParameters());
    frame->setTaskId(taskId);
    auto sourceAddr = MagicIntel123456;
    frame->setSourceAddress(sourceAddr.data());
    frame->set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    return frame;

}