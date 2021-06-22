//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_DEMOPLUGIN
#define PICOSCENES_DEMOPLUGIN

#include <iostream>
#include <mutex>
#include <PicoScenes/AbstractPicoScenesPlugin.hxx>
#include <PicoScenes/PicoScenesNIC.hxx>

class DemoPlugin : public AbstractPicoScenesPlugin {
public:
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    static std::shared_ptr<DemoPlugin> create() {
        return std::make_shared<DemoPlugin>();
    }
};

BOOST_DLL_ALIAS(DemoPlugin::create, initPicoScenesPlugin)

#endif //PICOSCENES_DEMOPLUGIN
