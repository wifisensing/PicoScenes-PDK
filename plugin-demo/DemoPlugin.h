//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_TESTPLUGIN_H
#define PICOSCENES_TESTPLUGIN_H

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
};

PICOSCENES_PLUGIN_INIT(DemoPlugin)

#endif //PICOSCENES_TESTPLUGIN_H
