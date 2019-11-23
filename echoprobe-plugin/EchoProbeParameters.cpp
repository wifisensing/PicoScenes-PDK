//
// Created by Zhiping Jiang on 11/20/17.
//


#include "EchoProbeParameters.h"

std::shared_ptr<EchoProbeParameters> EchoProbeParameters::sharedParameters = std::make_shared<EchoProbeParameters>();

std::shared_ptr<EchoProbeParameters> EchoProbeParameters::getInstance(const std::string &phyId) {
    static std::unordered_map<std::string, std::shared_ptr<EchoProbeParameters>> instanceMap;
    static bool initializationFlag = false;
    if(!initializationFlag) {
        EchoProbeParameters::initializeSharedParameters();
        initializationFlag = true;
    }
    if (instanceMap.find(phyId) == instanceMap.end()) {
        auto instance = std::make_shared<EchoProbeParameters>();
        *instance = *EchoProbeParameters::sharedParameters;
        instanceMap[phyId] = instance;
    }
    return instanceMap[phyId];
}

void EchoProbeParameters::initializeSharedParameters() {
    sharedParameters->inj_target_mac_address = AthNicParameters::magicIntel123456;
    sharedParameters->mcs = 0;
    sharedParameters->sgi = 0;
    sharedParameters->bw = 20;
    sharedParameters->tx_delay_us = 5e5;
    sharedParameters->finishedSessionId = UINT64_MAX;
    sharedParameters->workingSessionId = UINT64_MAX;
    sharedParameters->timeout_us = 8e3;
    sharedParameters->tx_max_retry = 100;
    sharedParameters->ack_maxLengthPerPacket = 1200;
    sharedParameters->delay_after_cf_change_us = 5e3;
    sharedParameters->numOfPacketsPerDotDisplay = 10;
}