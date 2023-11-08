//
// Created by Zhiping Jiang on 10/20/17.
//

#ifndef PICOSCENES_RXSBROADCASTPLUGIN_H
#define PICOSCENES_RXSBROADCASTPLUGIN_H

#include <PicoScenes/AbstractPicoScenesPlugin.hxx>
#include <PicoScenes/SystemTools.hxx>


class UDPForwarderPlugin : public AbstractPicoScenesPlugin {
public:
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::vector<PicoScenesDeviceType> getSupportedDeviceTypes() override;

    void initialization() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    void rxHandle(const ModularPicoScenesRxFrame &rxframe) override;

    static boost::shared_ptr<UDPForwarderPlugin> create() {
        return boost::make_shared<UDPForwarderPlugin>();
    }

private:
    std::optional<std::string> destinationIP;
    std::optional<uint16_t> destinationPort;
    std::shared_ptr<po::options_description> options;
    int counter = 0;
};

BOOST_DLL_ALIAS(UDPForwarderPlugin::create, initPicoScenesPlugin)

#endif //PICOSCENES_RXSBROADCASTPLUGIN_H
