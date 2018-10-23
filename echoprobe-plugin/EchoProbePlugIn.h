//
// Created by Zhiping Jiang on 11/18/17.
//

#ifndef PICOSCENES_ECHOPROBEPLUGIN_H
#define PICOSCENES_ECHOPROBEPLUGIN_H

#include <PicoScenes/AbstractRXSPlugIn.h>
#include "EchoProbeInitiator.h"
#include "EchoProbeResponder.h"
#include "EchoProbeParameters.h"

class EchoProbePlugIn : public AbstractRXSPlugIn {
public:
    std::string pluginName() override;

    std::string pluginDescription() override;

    std::string pluginStatus() override;

    void initialization() override;

    std::shared_ptr<program_options::options_description> pluginOptionsDescription() override;

    bool handleCommandString(std::string commandString) override;

    bool RXSHandle(const struct RXS_enhanced *rxs) override;

    void finalize() override;

    ~EchoProbePlugIn() override {};

    void serialize() override;
private:
    std::shared_ptr<EchoProbeInitiator> initiator;
    std::shared_ptr<EchoProbeResponder> responder;
    std::shared_ptr<EchoProbeParameters> parameters;

    std::shared_ptr<po::options_description> echoProbeOptions;
    std::shared_ptr<po::options_description> injectionOptions;
    std::shared_ptr<po::options_description> echoOptions;
};

PLUGIN_INIT(EchoProbePlugIn);

#endif //PICOSCENES_ECHOPROBEPLUGIN_H
