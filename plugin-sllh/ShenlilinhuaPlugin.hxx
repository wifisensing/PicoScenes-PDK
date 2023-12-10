//
// Created by tt on 23-12-10.
//

#ifndef SHENLILINHUAPLUGIN_HXX
#define SHENLILINHUAPLUGIN_HXX


#include <PicoScenes/AbstractPicoScenesPlugin.hxx>
#include <PicoScenes/SystemTools.hxx>


class ShenLiLingHuaPlugin : public AbstractPicoScenesPlugin {
public:
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::vector<PicoScenesDeviceType> getSupportedDeviceTypes() override;

    void initialization() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    static boost::shared_ptr<ShenLiLingHuaPlugin> create() {
        return boost::make_shared<ShenLiLingHuaPlugin>();
    }

private:
    std::optional<std::string> destinationIP;
    std::optional<uint16_t> destinationPort;
    std::shared_ptr<po::options_description> options;
};

BOOST_DLL_ALIAS(ShenLiLingHuaPlugin::create, initPicoScenesPlugin)

#endif //SHENLILINHUAPLUGIN_HXX
