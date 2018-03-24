//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_RXSBROADCASTPLUGIN_H
#define PICOSCENES_RXSBROADCASTPLUGIN_H

#include <PicoScenes/AbstractRXSPlugIn.h>
#include <PicoScenes/UDPService.h>


class RXSBroadcasterPlugIn : public AbstractRXSPlugIn{
public:
    std::string pluginName() override;

    std::string pluginDescription() override;

    void initialization() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    bool handleCommandString(std::string commandString) override;

    std::string pluginStatus() override;

    bool RXSHandle(const struct RXS_enhanced *rxs) override;

private:
    std::string destinationIP = "127.0.0.1";
    uint16_t destinationPort = 50000;
};

PLUGIN_INIT(RXSBroadcasterPlugIn)

#endif //PICOSCENES_RXSBROADCASTPLUGIN_H
