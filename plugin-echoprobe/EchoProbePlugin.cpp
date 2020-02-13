//
// Created by Zhiping Jiang on 11/18/17.
//

#include "EchoProbePlugin.h"


std::string EchoProbePlugin::getPluginName() {
    return "Echo_Probe";
}

std::string EchoProbePlugin::getPluginDescription() {
    return "Round-Trip Measurement";
}

std::string EchoProbePlugin::pluginStatus() {
    return "";
}

void EchoProbePlugin::initialization() {
    initiator = std::make_shared<EchoProbeInitiator>(std::dynamic_pointer_cast<PicoScenesNIC>(nic));
    responder = std::make_shared<EchoProbeResponder>(std::dynamic_pointer_cast<PicoScenesNIC>(nic));


    injectionOptions = std::make_shared<po::options_description>("Frame Injection Options");
    injectionOptions->add_options()
            ("target-interface", po::value<std::string>(), "PhyId of the injection target")
            ("target-mac-address", po::value<std::string>(), "MAC address of the injection target [ magic Intel 00:16:ea:12:34:56 is default]")
            ("5300", "Both Destination and Source MAC addresses are set to 'magic Intel 00:16:ea:12:34:56'")

            ("cf", po::value<std::string>(), "MATLAB-style specification for carrier frequency scan range, format begin:step:end, e.g., 5200e6:20e6:5800e6")
            ("sf", po::value<std::string>(), "MATLAB-style specification for baseband sampling frequency multipler scan range, format begin:step:end, e.g., 11:11:88")
            ("repeat", po::value<std::string>(), "The injection number per cf/bw combination, 100 as default")
            ("delay", po::value<std::string>(), "The delay between successive injections(unit in us, 5e5 as default)")
            ("delayed-start", po::value<uint32_t>(), "A one-time delay before injection(unit in second, 0 as default)")

            ("bw", po::value<uint32_t>(), "bandwidth for injection(unit in MHz) [20, 40], 20 as default")
            ("sgi", po::value<uint32_t>(), "Short Guarding-Interval [1 for on, 0 for off], 1 as default")
            ("mcs", po::value<uint32_t>(), "mcs value [0-23]")
            ("gf", "Enable Green Field Tx mode (Intel 5300AGN Only)")
            ("dup", "Enable 20MHz channel duplication in HT40+/- mode (Intel 5300AGN Only)");

    echoOptions = std::make_shared<po::options_description>("Echo Responder Options");
    echoOptions->add_options()
            ("ack-mcs", po::value<uint32_t>(), "mcs value for ack packets [0-23], unspecified as default")
            ("ack-bw", po::value<uint32_t>(), "bandwidth for ack packets (unit in MHz) [20, 40], unspecified as default")
            ("ack-sgi", po::value<uint32_t>(), "guarding-interval for ack packets [1 for on, 0 for off], unspecified as default");

    echoProbeOptions = std::make_shared<po::options_description>("Echo Probe Options");
    echoProbeOptions->add_options()
            ("mode", po::value<std::string>(), "Working mode [injector, logger, initiator, responder]");
    echoProbeOptions->add(*injectionOptions).add(*echoOptions);
}

std::shared_ptr<po::options_description> EchoProbePlugin::pluginOptionsDescription() {
    return echoProbeOptions;
}

std::vector<PicoScenesDeviceType> EchoProbePlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300};
    return supportedDevices;
}

void EchoProbePlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    auto style = pos::allow_long | pos::allow_dash_for_short |
                 pos::long_allow_adjacent | pos::long_allow_next |
                 pos::short_allow_adjacent | pos::short_allow_next;

    po::store(po::command_line_parser(po::split_unix(commandString)).options(*echoProbeOptions).style(style).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("mode")) {
        auto modeString = vm["mode"].as<std::string>();
        boost::algorithm::to_lower(modeString);
        boost::trim(modeString);

        if (modeString.find("injector") != std::string::npos) {
            parameters.workingMode = MODE_Injector;
            nic->stopRxService();
            nic->startTxService();
        } else if (modeString.find("logger") != std::string::npos) {
            parameters.workingMode = MODE_Logger;
            nic->stopTxService();
            nic->startRxService();
        } else if (modeString.find("responder") != std::string::npos) {
            parameters.workingMode = MODE_EchoProbeResponder;
            nic->startRxService();
            nic->startTxService();
        } else if (modeString.find("initiator") != std::string::npos) {
            parameters.workingMode = MODE_EchoProbeInitiator;
            nic->startRxService();
            nic->startTxService();;
        }
    }

    if (vm.count("target-interface")) {
        auto interfaceName = vm["target-interface"].as<std::string>();
        boost::trim(interfaceName);
        parameters.inj_target_interface = interfaceName;
        auto targetHAL = NICPortal::getTypedNIC<PicoScenesNIC>(interfaceName);
        if (targetHAL)
            parameters.inj_target_mac_address = targetHAL->getMacAddressMon();
    }

    if (vm.count("target-mac-address")) {
        auto macAddressString = vm["target-mac-address"].as<std::string>();
        std::vector<std::string> eachHexs;
        boost::split(eachHexs, macAddressString, boost::is_any_of(":-"), boost::token_compress_on);
        std::array<uint8_t, 6> address;
        if (eachHexs.size() != 6)
            LoggingService::warning_print("[target-mac-address] Specified mac address has wrong number of digits.\n");
        else {
            for (auto i = 0; i < eachHexs.size() && i < 6; i++) {
                boost::trim(eachHexs[i]);
                auto hex = std::stod("0x" + eachHexs[i]);
                address[i] = hex;
            }

            parameters.inj_target_mac_address = address;
        }
    }

    if (vm.count("5300")) {
        parameters.inj_for_intel5300 = true;
    }

    if (vm.count("cf")) {
        auto rangeString = vm["cf"].as<std::string>();
        std::vector<std::string> rangeParts;
        boost::split(rangeParts, rangeString, boost::is_any_of(":"), boost::token_compress_on);
        if (!rangeParts[0].empty())
            parameters.cf_begin = boost::lexical_cast<double>(rangeParts[0]);
        if (!rangeParts[1].empty())
            parameters.cf_step = boost::lexical_cast<double>(rangeParts[1]);
        if (!rangeParts[2].empty())
            parameters.cf_end = boost::lexical_cast<double>(rangeParts[2]);
    }

    if (vm.count("sf")) {
        auto rangeString = vm["sf"].as<std::string>();
        std::vector<std::string> rangeParts;
        boost::split(rangeParts, rangeString, boost::is_any_of(":"), boost::token_compress_on);
        if (!rangeParts[0].empty())
            parameters.pll_rate_begin = boost::lexical_cast<double>(rangeParts[0]);
        if (!rangeParts[1].empty())
            parameters.pll_rate_step = boost::lexical_cast<double>(rangeParts[1]);
        if (!rangeParts[2].empty())
            parameters.pll_rate_end = boost::lexical_cast<double>(rangeParts[2]);

        if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
            LoggingService::warning_print("Intel 5300 NIC does not support sampling rate configuration.\n");
            parameters.pll_rate_begin = 0;
            parameters.pll_rate_end = 0;
            parameters.pll_rate_step = 0;
        }
    }

    if (vm.count("repeat")) {
        parameters.cf_repeat = boost::lexical_cast<double>(vm["repeat"].as<std::string>());
    }

    if (vm.count("delay")) {
        parameters.tx_delay_us = boost::lexical_cast<double>(vm["delay"].as<std::string>());
    }

    if (vm.count("delayed-start")) {
        parameters.delayed_start_seconds = vm["delayed-start"].as<uint32_t>();
    }

    if (vm.count("bw")) {
        auto bwValue = vm["bw"].as<uint32_t>();
        if (bwValue == 20) {
            parameters.bw = 20;
        } else if (bwValue == 40) {
            parameters.bw = 40;
        } else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid bandwith value: {}.\n", bwValue));
    }

    if (vm.count("sgi")) {
        auto sgiValue = vm["sgi"].as<uint32_t>();
        parameters.sgi = sgiValue;
    }

    if (vm.count("mcs")) {
        auto mcs = vm["mcs"].as<uint32_t>();
        if (mcs < 23)
            parameters.mcs = mcs;
        else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid MCS value: {}.\n", mcs));
    }

    if (vm.count("gf")) {
        parameters.inj_5300_gf = true;
    }

    if (vm.count("dup")) {
        parameters.inj_5300_duplication = true;
    }

    if (vm.count("ack-mcs")) {
        auto mcsValue = vm["ack-mcs"].as<uint32_t>();
        if (mcsValue <= 23) {
            parameters.ack_mcs = mcsValue;
        } else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid ACK MCS value: {}.\n", mcsValue));
    }

    if (vm.count("ack-bw")) {
        auto ack_bw = vm["ack-bw"].as<uint32_t>();
        if (ack_bw == 20) {
            parameters.ack_bw = 20;
        } else if (ack_bw == 40) {
            parameters.ack_bw = 40;
        } else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid ACK bandwidth value: {}.\n", ack_bw));
    }

    if (vm.count("ack-sgi")) {
        auto sgiValue = vm["sgi"].as<uint32_t>();
        parameters.ack_sgi = sgiValue;
    }

    if (vm.size() > 0)
        parameters.workingSessionId = uniformRandomNumberWithinRange<uint64_t>(0, UINT64_MAX);

    if (parameters.workingMode == MODE_EchoProbeInitiator || parameters.workingMode == MODE_Injector)
        initiator->startJob(parameters);
    else if (parameters.workingMode == MODE_EchoProbeResponder || parameters.workingMode == MODE_Logger)
        responder->startJob(parameters);
}

void EchoProbePlugin::rxHandle(const PicoScenesRxFrameStructure &rxframe) {
    if (parameters.workingMode == MODE_EchoProbeResponder || parameters.workingMode == MODE_Logger)
        responder->handle(rxframe);
}
