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
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::vector<PicoScenesDeviceType> getSupportedDeviceTypes() override;

    void initialization() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    static boost::shared_ptr<DemoPlugin> create() {
        return boost::make_shared<DemoPlugin>();
    }

    void rxHandle(const ModularPicoScenesRxFrame &rxframe) override;

    [[nodiscard]] std::shared_ptr<ModularPicoScenesTxFrame> buildBasicFrame(uint16_t taskId = 0) const ;

private:
    std::shared_ptr<po::options_description> options;
};

BOOST_DLL_ALIAS(DemoPlugin::create, initPicoScenesPlugin)

#endif //PICOSCENES_DEMOPLUGIN
