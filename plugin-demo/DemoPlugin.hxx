// DemoPlugin.hxx

#include <iostream>
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
    static std::shared_ptr<DemoPlugin> create()
    {
        return std::make_shared<DemoPlugin>();
    }

    void rxHandle(const ModularPicoScenesRxFrame &rxframe) override;

    [[nodiscard]] ModularPicoScenesTxFrame buildBasicFrame(uint16_t taskId = 0) const ;


private:

    // Options description for the plugin
    std::shared_ptr<po::options_description> options;
};

// Alias the create function to 'initPicoScenesPlugin' using BOOST_DLL_ALIAS
BOOST_DLL_ALIAS(DemoPlugin::create, initPicoScenesPlugin)