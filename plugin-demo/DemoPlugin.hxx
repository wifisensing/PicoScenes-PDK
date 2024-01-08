//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_DEMOPLUGIN
#define PICOSCENES_DEMOPLUGIN

#include <iostream>
#include <mutex>
#include <PicoScenes/AbstractPicoScenesPlugin.hxx>
#include <PicoScenes/MAC80211CSIExtractableNIC.hxx>

class DemoPlugin : public AbstractPicoScenesPlugin {
public:

    // Get the name of the plugin
    std::string getPluginName() override;

    // Get the description of the plugin
    std::string getPluginDescription() override;

    // Get the status of the plugin
    std::string pluginStatus() override;

    // Get the supported device types by the plugin
    std::vector<PicoScenesDeviceType> getSupportedDeviceTypes() override;

    // Perform initialization tasks for the plugin
    void initialization() override;

    // Get the options description for the plugin
    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    // Parse and execute commands for the plugin
    void parseAndExecuteCommands(const std::string &commandString) override;

    // Create an instance of the DemoPlugin
    static boost::shared_ptr<DemoPlugin> create();

    // Handle received frames in the plugin
    void rxHandle(const ModularPicoScenesRxFrame &rxframe) override;

    // Build a basic transmission frame for the plugin
    [[nodiscard]] std::shared_ptr<ModularPicoScenesTxFrame> buildBasicFrame(uint16_t taskId = 0) const;

private:

    // Options description for the plugin
    std::shared_ptr<po::options_description> options;
};

// Alias the create function to 'initPicoScenesPlugin' using BOOST_DLL_ALIAS
BOOST_DLL_ALIAS(DemoPlugin::create, initPicoScenesPlugin)

#endif //PICOSCENES_DEMOPLUGIN
