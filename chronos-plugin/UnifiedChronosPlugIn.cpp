//
// Created by Zhiping Jiang on 11/18/17.
//

#include "UnifiedChronosPlugIn.h"

std::string UnifiedChronosPlugIn::pluginName() {
    return "Unified Chronos";
}

std::string UnifiedChronosPlugIn::pluginDescription() {
    return "Round-Trip Measurement";
}

std::string UnifiedChronosPlugIn::pluginStatus() {
    return "";
}

void UnifiedChronosPlugIn::initialization() {
    initiator = std::make_shared<UnifiedChronosInitiator>(hal);
    responder = std::make_shared<UnifiedChronosResponder>(hal);
    parameters = UnifiedChronosParameters::getInstance(hal->phyId);

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
            ("freq-repeat", po::value<std::string>(), "The repeating injection number for each CF, 1 as default")
            ("freq-range", po::value<std::string>(), "MATLAB-style setting for freq-begin/end/step")
            ("delay", po::value<std::string>(), "The delay between successive injections(unit in us, 1000000 as default)")
            ("delayed-start", po::value<uint32_t>(), "A one-time delay before injection(unit in second, 0 as default)")
            ("bw", po::value<uint32_t>(), "bandwidth for injection(unit in MHz) [20, 40]")
            ("sgi", po::value<uint32_t>(), "Short Guarding-Interval [1 for on, 0 for off], 1 as default")
            ("mcs", po::value<uint32_t>(), "mcs value [0-23]");

    chronosOptions = std::make_shared<po::options_description>("Chronos(Injection and Reply) Options");
    chronosOptions->add_options()
            ("ack-freq-gap", po::value<std::string>(), "The CF gap between Chronos Initiator and Responder(unit in Hz, 0 as default)")
            ("ack-additional-delay", po::value<uint32_t>(), "Additional delay between Rx and Tx in responder side(unit in us, 0 as default)")
            ("ack-mcs",  po::value<uint32_t>(), "mcs value for Chronos ACK [0-23], unspecified as default")
            ("ack-bw", po::value<uint32_t>(), "bandwidth for Chronos ACK(unit in MHz) [20, 40], unspecified as default")
            ("ack-sgi", po::value<uint32_t>(), "guarding-interval for Chronos ACK [1 for on, 0 for off], unspecified as default");

    unifiedChronosOptions = std::make_shared<program_options::options_description>("Chronos(Injection and Reply) Options");
    unifiedChronosOptions->add_options()
            ("mode", po::value<std::string>(), "Working mode [injector, chronos-responder, chronos-injector]");
    unifiedChronosOptions->add(*injectionOptions).add(*chronosOptions);
}

std::shared_ptr<program_options::options_description> UnifiedChronosPlugIn::pluginOptionsDescription() {
    return unifiedChronosOptions;
}

bool UnifiedChronosPlugIn::handleCommandString(std::string commandString) {
    po::variables_map vm;
    auto style = pos::allow_long | pos::allow_dash_for_short |
            pos::long_allow_adjacent | pos::long_allow_next |
            pos::short_allow_adjacent | pos::short_allow_next;

    po::store(po::command_line_parser(po::split_unix(commandString)).options(*unifiedChronosOptions).style(style).allow_unregistered().run(), vm);
    po::notify(vm);

    if(vm.count("mode")) {
        auto modeString = vm["mode"].as<std::string>();
        boost::algorithm::to_lower(modeString);
        boost::trim(modeString);

        if(modeString.find("injector") != std::string::npos) {
           hal->parameters->workingMode = Injector;
            hal->setRxChainStatus(false);
            hal->setTxChainStatus(true);
            hal->setTxSChainStatus(true);
        } else if(modeString.find("chronos-responder") != std::string::npos) {
           hal->parameters->workingMode = ChronosResponder;
            hal->setRxChainStatus(true);
            hal->setTxChainStatus(true);
            hal->setTxSChainStatus(true);
        } else if(modeString.find("chronos-initiator") != std::string::npos) {
           hal->parameters->workingMode = ChronosInitiator;
            hal->setRxChainStatus(true);
            hal->setTxChainStatus(true);
            hal->setTxSChainStatus(true);
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
       parameters->inj_freq_begin = boost::lexical_cast<double>(vm["freq-begin"].as<std::string>());
    }

    if (vm.count("freq-end")) {
       parameters->inj_freq_end = boost::lexical_cast<double>(vm["freq-end"].as<std::string>());
    }

    if (vm.count("freq-step")) {
       parameters->inj_freq_step = boost::lexical_cast<double>(vm["freq-step"].as<std::string>());
    }

    if (vm.count("freq-repeat")) {
       parameters->inj_freq_repeat = boost::lexical_cast<double>(vm["freq-repeat"].as<std::string>());
    }

    if (vm.count("delay")) {
       parameters->inj_delay_us = boost::lexical_cast<double>(vm["delay"].as<std::string>());
    }

    if (vm.count("delayed-start")) {
        parameters->inj_delayed_start_s = vm["delayed-start"].as<uint32_t>();
    }

    if (vm.count("bw")) {
        auto bwValue = vm["bw"].as<uint32_t>();
        if (bwValue == 20) {
            parameters->inj_bw = 20;
        } else if (bwValue == 40) {
            parameters->inj_bw = 40;
        } else 
            throw std::invalid_argument(fmt::format("[Chronos Plugin]: invalid bandwith value: {}.\n", bwValue));
    }

    if (vm.count("sgi")) {
        auto sgi = vm["sgi"].as<uint32_t>();
        if (sgi == 1 || sgi == 0)
            parameters->inj_sgi = sgi;
        else 
            throw std::invalid_argument(fmt::format("[Chronos Plugin]: invalid SGI value: {}.\n", sgi));
    }

    if (vm.count("mcs")) {
        auto mcs = vm["mcs"].as<uint32_t>();
        if (mcs < 23)
            parameters->inj_mcs = mcs;
        else 
            throw std::invalid_argument(fmt::format("[Chronos Plugin]: invalid MCS value: {}.\n", mcs));
    }

    if (vm.count("ack-freq-gap")) {
       parameters->chronos_inj_freq_gap = boost::lexical_cast<double>(vm["ack-freq-gap"].as<std::string>());
    }

    if (vm.count("ack-additional-delay")) {
       parameters->chronos_ack_additional_delay = vm["ack-additional-delay"].as<uint32_t>();
    }

    if (vm.count("ack-mcs")) {
        auto mcsValue = vm["ack-mcs"].as<uint32_t>();
        if (mcsValue <=23) {
            parameters->chronos_ack_mcs = mcsValue;
        } else 
            throw std::invalid_argument(fmt::format("[Chronos Plugin]: invalid ACK MCS value: {}.\n", mcsValue));
    }

    if (vm.count("ack-bw")) {
        auto ack_bw = vm["ack-bw"].as<uint32_t>();
        if (ack_bw == 20) {
           parameters->chronos_ack_bw = 20;
        } else if (ack_bw == 40) {
           parameters->chronos_ack_bw = 40;
        } else 
            throw std::invalid_argument(fmt::format("[Chronos Plugin]: invalid ACK bandwith value: {}.\n", ack_bw));
    }

    if (vm.count("ack-sgi")) {
        auto sgi = vm["ack-sgi"].as<uint32_t>();
        if (sgi == 1 || sgi == 0)
            parameters->chronos_ack_sgi = sgi;
        else 
            throw std::invalid_argument(fmt::format("[Chronos Plugin]: invalid SGI value: {}.\n", sgi));
    }

    if (vm.size() > 0)
        parameters->workingSessionId = uniformRandomNumberWithinRange<uint64_t>(0, UINT64_MAX);

        initiator->blockWait();

    return false;
}

bool UnifiedChronosPlugIn::RXSHandle(const struct RXS_enhanced *rxs) {
    hal->plugInManager->properties.put("bypass_platform_logging", true);
    return responder->handle(rxs);
}

void UnifiedChronosPlugIn::serialize() {
    propertyDescriptionTree.clear();
    propertyDescriptionTree.add_child("initiator", initiator->getBoostPropertyTree());
    propertyDescriptionTree.add_child("responder", responder->getBoostPropertyTree());
}