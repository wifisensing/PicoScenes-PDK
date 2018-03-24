//
// Created by Zhiping Jiang on 11/18/17.
//

#ifndef PICOSCENES_UNIFIEDCHRONOSPLUGIN_H
#define PICOSCENES_UNIFIEDCHRONOSPLUGIN_H

#include <PicoScenes/AbstractRXSPlugIn.h>
#include "UnifiedChronosInitiator.h"
#include "UnifiedChronosResponder.h"
#include "UnifiedChronosParameters.h"

class UnifiedChronosPlugIn : public AbstractRXSPlugIn {
public:
    std::string pluginName() override;

    std::string pluginDescription() override;

    std::string pluginStatus() override;

    void initialization() override;

    std::shared_ptr<program_options::options_description> pluginOptionsDescription() override;

    bool handleCommandString(std::string commandString) override;

    bool RXSHandle(const struct RXS_enhanced *rxs) override;

    ~UnifiedChronosPlugIn() override {};

private:
    std::shared_ptr<UnifiedChronosInitiator> initiator;
    std::shared_ptr<UnifiedChronosResponder> responder;
    std::shared_ptr<UnifiedChronosParameters> parameters;

    std::shared_ptr<po::options_description> unifiedChronosOptions;
    std::shared_ptr<po::options_description> injectionOptions;
    std::shared_ptr<po::options_description> chronosOptions;
};

PLUGIN_INIT(UnifiedChronosPlugIn);

#endif //PICOSCENES_UNIFIEDCHRONOSPLUGIN_H
