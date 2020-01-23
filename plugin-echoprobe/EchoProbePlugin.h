//
// Created by Zhiping Jiang on 11/18/17.
//

#ifndef PICOSCENES_ECHOPROBEPLUGIN_H
#define PICOSCENES_ECHOPROBEPLUGIN_H

#include <PicoScenes/AbstractPicoScenesPlugin.hxx>
#include <PicoScenes/PicoScenesNIC.hxx>
#include "EchoProbeInitiator.h"
//#include "EchoProbeResponder.h"
#include "EchoProbeParameters.h"

class EchoProbePlugin : public AbstractPicoScenesPlugin {
public:
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::vector<PicoScenesDeviceType> getSupportedDeviceTypes() override;

    void initialization() override;

    std::shared_ptr<po::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    void rxHandle(const PicoScenesRxFrameStructure &rxframe) override;

private:
    std::shared_ptr<EchoProbeInitiator> initiator;
//    std::shared_ptr<EchoProbeResponder> responder;
    std::shared_ptr<EchoProbeParameters> parameters;

    std::shared_ptr<po::options_description> echoProbeOptions;
    std::shared_ptr<po::options_description> injectionOptions;
    std::shared_ptr<po::options_description> echoOptions;
};

PICOSCENES_PLUGIN_INIT(EchoProbePlugin);

#endif //PICOSCENES_ECHOPROBEPLUGIN_H
