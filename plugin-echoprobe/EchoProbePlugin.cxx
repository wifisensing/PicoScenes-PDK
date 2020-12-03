//
// Created by Zhiping Jiang on 11/18/17.
//

#include <boost/algorithm/string/predicate.hpp>
#include "EchoProbePlugin.h"


std::string EchoProbePlugin::getPluginName() {
    return "Echo_Probe";
}

std::string EchoProbePlugin::getPluginDescription() {
    return "Echo Probe, a round-trip CSI measurement plugin.";
}

std::string EchoProbePlugin::pluginStatus() {
    return "";
}

void EchoProbePlugin::initialization() {
    initiator = std::make_shared<EchoProbeInitiator>(std::dynamic_pointer_cast<AbstractNIC>(nic));
    responder = std::make_shared<EchoProbeResponder>(std::dynamic_pointer_cast<AbstractNIC>(nic));


    injectionOptions = std::make_shared<po::options_description>("EchoProbe Initiator Options");
    injectionOptions->add_options()
            ("target-mac-address", po::value<std::string>(), "MAC address of the injection target [ magic Intel 00:16:ea:12:34:56 is default]")
            ("5300", "Both Destination and Source MAC addresses are set to 'magic Intel 00:16:ea:12:34:56'")

            ("cf", po::value<std::string>(), "MATLAB-style specification for carrier frequency scan range, format begin:step:end, e.g., 5200e6:20e6:5800e6")
            ("sf", po::value<std::string>(), "MATLAB-style specification for baseband sampling frequency multipler scan range, format begin:step:end, e.g., 11:11:88")
            ("repeat", po::value<std::string>(), "The injection number per cf/bw combination, 100 as default")
            ("delay", po::value<std::string>(), "The delay between successive injections(unit in us, 5e5 as default)")
            ("delayed-start", po::value<uint32_t>(), "A one-time delay before injection(unit in us, 0 as default)")

            ("format", po::value<std::string>(), "802.11 frame format [nonHT, HT, VHT, HESU]")
            ("cbw", po::value<uint32_t>(), "Channel Bandwidth (CBW) for injection(unit in MHz) [20, 40, 80, 160], 20 as default")
            ("mcs", po::value<uint32_t>(), "MCS value [0-11], the MCS index for one single spatial stream")
            ("sts", po::value<uint32_t>(), "Number of spatial time stream (STS) [0-4], 0 as default")
            ("ess", po::value<uint32_t>(), "Number of Extension Spatial Stream for TX [ 0 as default, 1, 2, 3]")
            ("gi", po::value<uint32_t>(), "Guarding Interval [400, 800, 1600, 3200], 800 as default")
            ("coding", po::value<std::string>(), "Code scheme [LDPC, BCC], BCC as default");

    echoOptions = std::make_shared<po::options_description>("Echo Responder Options");
    echoOptions->add_options()
            ("ack-type", po::value<std::string>(), "EchoProbe reply strategy [full, csi, extra, header], full as default")
            ("ack-mcs", po::value<uint32_t>(), "mcs value (for one single spatial stream) for ack packets [0-11], unspecified as default")
            ("ack-sts", po::value<uint32_t>(), "the number of spatial time stream (STS) for ack packets [0-23], unspecified as default")
            ("ack-cbw", po::value<uint32_t>(), "bandwidth for ack packets (unit in MHz) [20, 40, 80, 160], unspecified as default")
            ("ack-gi", po::value<uint32_t>(), "guarding-interval for ack packets [400, 800, 1600, 3200], unspecified as default");

    echoProbeOptions = std::make_shared<po::options_description>("Echo Probe Options");
    echoProbeOptions->add_options()
            ("mode", po::value<std::string>(), "Working mode [injector, logger, initiator, responder]");
    echoProbeOptions->add(*injectionOptions).add(*echoOptions);
}

std::shared_ptr<po::options_description> EchoProbePlugin::pluginOptionsDescription() {
    return echoProbeOptions;
}

std::vector<PicoScenesDeviceType> EchoProbePlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300, PicoScenesDeviceType::USRP};
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
            nic->startTxService();
        }
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
            parameters.sf_begin = boost::lexical_cast<double>(rangeParts[0]);
        if (!rangeParts[1].empty())
            parameters.sf_step = boost::lexical_cast<double>(rangeParts[1]);
        if (!rangeParts[2].empty())
            parameters.sf_end = boost::lexical_cast<double>(rangeParts[2]);

        if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
            LoggingService::warning_print("Intel 5300 NIC does not support sampling rate configuration.\n");
            parameters.sf_begin = 0;
            parameters.sf_step = 0;
            parameters.sf_end = 0;
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

    if (vm.count("format")) {
        auto format = vm["format"].as<std::string>();
        if (boost::iequals(format, "nonHT")) {
            parameters.format = PacketFormatEnum::PacketFormat_NonHT;
        } else if (boost::iequals(format, "HT")) {
            parameters.format = PacketFormatEnum::PacketFormat_HT;
        } else if (boost::iequals(format, "VHT")) {
            parameters.format = PacketFormatEnum::PacketFormat_VHT;
        } else if (boost::iequals(format, "HESU")) {
            parameters.format = PacketFormatEnum::PacketFormat_HESU;
        } else if (boost::iequals(format, "HEMU")) {
            parameters.format = PacketFormatEnum::PacketFormat_HEMU;
        }
    }

    if (vm.count("cbw")) {
        auto bwValue = vm["cbw"].as<uint32_t>();
        parameters.cbw = bwValue;
    }

    if (vm.count("gi")) {
        auto sgiValue = vm["gi"].as<uint32_t>();
        parameters.guardInterval = sgiValue;
    }

    if (vm.count("mcs")) {
        auto mcs = vm["mcs"].as<uint32_t>();
        if (mcs < 11)
            parameters.mcs = mcs;
        else
            throw std::invalid_argument(fmt::format("[EchoProbe]: invalid MCS value: {}.\n", mcs));
    }

    if (vm.count("sts")) {
        auto numSTS = vm["sts"].as<uint32_t>();
        if (numSTS < 5)
            parameters.numSTS = numSTS;
        else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid STS value: {}.\n", numSTS));
    }

    if (vm.count("ess")) {
        auto ness = vm["ess"].as<uint32_t>();
        if (ness < 4)
            parameters.numESS = ness;
        else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid number of extension spatial stream (NESS) value: {}.\n", ness));
    }

    if (vm.count("coding")) {
        auto codingStr = vm["coding"].as<std::string>();
        if (boost::iequals(codingStr, "LDPC"))
            parameters.coding = (uint32_t) ChannelCodingEnum::LDPC;
        else if (boost::iequals(codingStr, "BCC"))
            parameters.coding = (uint32_t) ChannelCodingEnum::BCC;
    }


    if (vm.count("ack-type")) {
        auto ackType = vm["ack-type"].as<std::string>();
        if (boost::iequals(ackType, "full"))
            parameters.replyStrategy = EchoProbeReplyStrategy::ReplyWithFullPayload;
        else if (boost::iequals(ackType, "csi"))
            parameters.replyStrategy = EchoProbeReplyStrategy::ReplyWithCSI;
        else if (boost::iequals(ackType, "extra"))
            parameters.replyStrategy = EchoProbeReplyStrategy::ReplyWithExtraInfo;
        else if (boost::iequals(ackType, "header"))
            parameters.replyStrategy = EchoProbeReplyStrategy::ReplyOnlyHeader;
    }

    if (vm.count("ack-mcs")) {
        auto mcsValue = vm["ack-mcs"].as<uint32_t>();
        if (mcsValue <= 11) {
            parameters.ack_mcs = mcsValue;
        } else
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid ACK MCS value: {}.\n", mcsValue));
    }

    if (vm.count("ack-sts")) {
        auto ack_sts = vm["ack-sts"].as<uint32_t>();
        parameters.ack_cbw = ack_sts;
    }

    if (vm.count("ack-cbw")) {
        auto ack_bw = vm["ack-cbw"].as<uint32_t>();
        parameters.ack_cbw = ack_bw;
    }

    if (vm.count("ack-gi")) {
        auto giValue = vm["ack-gi"].as<uint32_t>();
        parameters.ack_guardInterval = giValue;
    }

    if (parameters.workingMode == MODE_EchoProbeInitiator || parameters.workingMode == MODE_Injector)
        initiator->startJob(parameters);
    else if (parameters.workingMode == MODE_EchoProbeResponder || parameters.workingMode == MODE_Logger)
        responder->startJob(parameters);
}

void EchoProbePlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    if (parameters.workingMode == MODE_EchoProbeResponder || parameters.workingMode == MODE_Logger)
        responder->handle(rxframe);
}
