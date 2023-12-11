//
// Created by tt on 23-12-11.
//

#ifndef GENSHINPLUGIN_HXX
#define GENSHINPLUGIN_HXX


#include <PicoScenes/AbstractPicoScenesPlugin.hxx>


class GenshinPlugin : public AbstractPicoScenesPlugin {
public:
    std::string getPluginName() override;

    std::string getPluginDescription() override;

    std::string pluginStatus() override;

    std::vector<PicoScenesDeviceType> getSupportedDeviceTypes() override;

    void initialization() override;

    std::shared_ptr<boost::program_options::options_description> pluginOptionsDescription() override;

    void parseAndExecuteCommands(const std::string &commandString) override;

    static boost::shared_ptr<GenshinPlugin> create() {
        return boost::make_shared<GenshinPlugin>();
    }

private:
    std::shared_ptr<po::options_description> options;
};

BOOST_DLL_ALIAS(GenshinPlugin::create, initPicoScenesPlugin)



#endif //GENSHINPLUGIN_HXX
