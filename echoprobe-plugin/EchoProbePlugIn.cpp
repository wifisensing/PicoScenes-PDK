//
// Created by Zhiping Jiang on 11/18/17.
//

#include "EchoProbePlugIn.h"

std::string EchoProbePlugIn::pluginName() {
    return "Echo_Probe";
}

std::string EchoProbePlugIn::pluginDescription() {
    return "Round-Trip Measurement";
}

std::string EchoProbePlugIn::pluginStatus() {
    return "";
}

void EchoProbePlugIn::initialization() {
    initiator = std::make_shared<EchoProbeInitiator>(hal);
    responder = std::make_shared<EchoProbeResponder>(hal);
    parameters = EchoProbeParameters::getInstance(hal->phyId);

    initiator->parameters = parameters;
    responder->parameters = parameters;

    initiator->startDaemonTask();

    injectionOptions = std::make_shared<po::options_description>("Frame Injection Options");
    injectionOptions->add_options()
            ("target-interface", po::value<std::string>(), "PhyId of the injection target")
            ("target-mac-address", po::value<std::string>(), "MAC address of the injection target [ magic Intel 00:16:ea:12:34:56 is default]")
            ("target-intel5300", "Both Destination and Source MAC addresses are set to 'magic Intel 00:16:ea:12:34:56'")
            ("freq-begin", po::value<std::string>(), "The starting CF of a scan(unit in Hz, working CF as default)")
            ("freq-end", po::value<std::string>(), "The ending CF of a scan(unit in Hz, working CF as default)")
            ("freq-step", po::value<std::string>(), "The freq step length for CF tuning(unit in Hz, 0 as default)")
            ("repeat", po::value<std::string>(), "The injection number per freq, 10 as default")
            ("freq-range", po::value<std::string>(), "MATLAB-style setting for freq-begin/end/step")
            ("delay", po::value<std::string>(), "The delay between successive injections(unit in us, 5e5 as default)")
            ("delayed-start", po::value<uint32_t>(), "A one-time delay before injection(unit in second, 0 as default)")
            ("bw", po::value<uint32_t>(), "bandwidth for injection(unit in MHz) [20, 40], 20 as default")
            ("sgi", po::value<uint32_t>(), "Short Guarding-Interval [1 for on, 0 for off], 1 as default")
            ("mcs", po::value<uint32_t>(), "mcs value [0-23]");

    echoOptions = std::make_shared<po::options_description>("Echo Responder Options");
    echoOptions->add_options()
            ("ack-mcs",  po::value<uint32_t>(), "mcs value for ack packets [0-23], unspecified as default")
            ("ack-bw", po::value<uint32_t>(), "bandwidth for ack packets (unit in MHz) [20, 40], unspecified as default")
            ("ack-sgi", po::value<uint32_t>(), "guarding-interval for ack packets [1 for on, 0 for off], unspecified as default");

    echoProbeOptions = std::make_shared<program_options::options_description>("Echo Probe Options");
    echoProbeOptions->add_options()
            ("mode", po::value<std::string>(), "Working mode [injector, chronos-responder, chronos-injector]");
    echoProbeOptions->add(*injectionOptions).add(*echoOptions);
}

std::shared_ptr<program_options::options_description> EchoProbePlugIn::pluginOptionsDescription() {
    return echoProbeOptions;
}

bool EchoProbePlugIn::handleCommandString(std::string commandString) {
    po::variables_map vm;
    auto style = pos::allow_long | pos::allow_dash_for_short |
            pos::long_allow_adjacent | pos::long_allow_next |
            pos::short_allow_adjacent | pos::short_allow_next;

    po::store(po::command_line_parser(po::split_unix(commandString)).options(*echoProbeOptions).style(style).allow_unregistered().run(), vm);
    po::notify(vm);

    if(vm.count("mode")) {
        auto modeString = vm["mode"].as<std::string>();
        boost::algorithm::to_lower(modeString);
        boost::trim(modeString);

        if(modeString.find("injector") != std::string::npos) {
           hal->parameters->workingMode = MODE_Injector;
            hal->setRxChainStatus(false);
            hal->setTxChainStatus(true);
            hal->setTxSChainStatus(true);
        } else if(modeString.find("responder") != std::string::npos) {
           hal->parameters->workingMode = MODE_EchoProbeResponder;
            hal->setRxChainStatus(true);
            hal->setTxChainStatus(true);
            hal->setTxSChainStatus(true);
            hal->setDefaultLoggerStatus(true);
        } else if(modeString.find("initiator") != std::string::npos) {
           hal->parameters->workingMode = MODE_EchoProbeInitiator;
            hal->setRxChainStatus(true);
            hal->setTxChainStatus(true);
            hal->setTxSChainStatus(true);
            hal->setDefaultLoggerStatus(false);
        }
    }

    if (vm.count("target-interface")) {
        auto interfaceName = vm["target-interface"].as<std::string>();
        boost::trim(interfaceName);
        parameters->inj_target_interface = interfaceName;
        auto targetHAL = AtherosNicHAL::halForInterface(interfaceName);
        if (targetHAL)
            parameters->inj_target_mac_address = targetHAL->macAddress_MON;
    }

    if (vm.count("target-mac-address")) {
        auto macAddressString = vm["target-mac-address"].as<std::string>();
        std::vector<std::string> eachHexs;
        boost::split(eachHexs, macAddressString, boost::is_any_of(":-"), boost::token_compress_on);
        std::array<uint8_t, 6> address;
        if (eachHexs.size() != 6)
            LoggingService::warning_print("[target-mac-address] Specified mac address has wrong number of digits.\n");
        else {
            for(auto i = 0 ; i < eachHexs.size() && i < 6; i++) {
                boost::trim(eachHexs[i]);
                auto hex = std::stod("0x"+eachHexs[i]);
                address[i] = hex;
            }

            parameters->inj_target_mac_address = address;
        }
    }

    if (vm.count("target-intel5300")) {
        parameters->inj_for_intel5300 = true;
    }

    if (vm.count("freq-begin")) {
       parameters->cf_begin = boost::lexical_cast<double>(vm["freq-begin"].as<std::string>());
    }

    if (vm.count("freq-end")) {
       parameters->cf_end = boost::lexical_cast<double>(vm["freq-end"].as<std::string>());
    }

    if (vm.count("freq-step")) {
       parameters->cf_step = boost::lexical_cast<double>(vm["freq-step"].as<std::string>());
    }

    if (vm.count("repeat")) {
       parameters->cf_repeat = boost::lexical_cast<double>(vm["repeat"].as<std::string>());
    }

    if (vm.count("delay")) {
       parameters->tx_delay_us = boost::lexical_cast<double>(vm["delay"].as<std::string>());
    }

    if (vm.count("delayed-start")) {
        parameters->delayed_start_seconds = vm["delayed-start"].as<uint32_t>();
    }

    if (vm.count("bw")) {
        auto bwValue = vm["bw"].as<uint32_t>();
        if (bwValue == 20) {
            parameters->bw = 20;
        } else if (bwValue == 40) {
            parameters->bw = 40;
        } else 
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid bandwith value: {}.\n", bwValue));
    }

    if (vm.count("sgi")) {
        auto sgi = vm["sgi"].as<uint32_t>();
        if (sgi == 1 || sgi == 0)
            parameters->sgi = sgi;
        else 
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid SGI value: {}.\n", sgi));
    }

    if (vm.count("mcs")) {
        auto mcs = vm["mcs"].as<uint32_t>();
        if (mcs < 23)
            parameters->mcs = mcs;
        else 
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid MCS value: {}.\n", mcs));
    }

    if (vm.count("ack-mcs")) {
        auto mcsValue = vm["ack-mcs"].as<uint32_t>();
        if (mcsValue <=23) {
            parameters->ack_mcs = mcsValue;
        } else 
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid ACK MCS value: {}.\n", mcsValue));
    }

    if (vm.count("ack-bw")) {
        auto ack_bw = vm["ack-bw"].as<uint32_t>();
        if (ack_bw == 20) {
           parameters->ack_bw = 20;
        } else if (ack_bw == 40) {
           parameters->ack_bw = 40;
        } else 
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid ACK bandwith value: {}.\n", ack_bw));
    }

    if (vm.count("ack-sgi")) {
        auto sgi = vm["ack-sgi"].as<uint32_t>();
        if (sgi == 1 || sgi == 0)
            parameters->ack_sgi = sgi;
        else 
            throw std::invalid_argument(fmt::format("[EchoProbe Plugin]: invalid SGI value: {}.\n", sgi));
    }

    if (vm.size() > 0)
        parameters->workingSessionId = uniformRandomNumberWithinRange<uint64_t>(0, UINT64_MAX);

        initiator->blockWait();

    return false;
}

bool EchoProbePlugIn::RXSHandle(const struct RXS_enhanced *rxs) {
    return responder->handle(rxs);
}

void EchoProbePlugIn::serialize() {
    propertyDescriptionTree.clear();
    propertyDescriptionTree.add_child("initiator", initiator->getBoostPropertyTree());
    propertyDescriptionTree.add_child("responder", responder->getBoostPropertyTree());
}

void EchoProbePlugIn::finalize() {

}