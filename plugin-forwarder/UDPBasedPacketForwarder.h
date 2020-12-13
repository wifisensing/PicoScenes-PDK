//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_RXSBROADCASTPLUGIN_H
#define PICOSCENES_RXSBROADCASTPLUGIN_H

#include <PicoScenes/AbstractPicoScenesPlugin.hxx>
#include <PicoScenes/UDPService.h>


class UDPBasedPacketForwarder : public AbstractPicoScenesPlugin {
public:
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    void rxHandle(const ModularPicoScenesRxFrame &rxframe) override;

private:
    std::string destinationIP = "127.0.0.1";
    uint16_t destinationPort = 50000;
};

PICOSCENES_PLUGIN_INIT(UDPBasedPacketForwarder)

#endif //PICOSCENES_RXSBROADCASTPLUGIN_H
