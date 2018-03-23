//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_TESTPLUGIN_H
#define PICOSCENES_TESTPLUGIN_H

#include <iostream>
#include <mutex>
#include <headers/hal/AbstractRXSPlugIn.h>

class DemoPlugIn : public AbstractRXSPlugIn {
public:
    std::string pluginName() override;

    std::string pluginDescription() override;

    std::string pluginStatus() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    bool handleCommandString(std::string commandString) override;

    void initialization() override;

    bool RXSHandle(const struct RXS_enhanced *rxs) override;
};

PLUGIN_INIT(DemoPlugIn)

#endif //PICOSCENES_TESTPLUGIN_H
