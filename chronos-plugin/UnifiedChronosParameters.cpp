//
// Created by Zhiping Jiang on 11/20/17.
//


#include "UnifiedChronosParameters.h"

std::shared_ptr<UnifiedChronosParameters> UnifiedChronosParameters::sharedParameters = std::make_shared<UnifiedChronosParameters>();
const std::array<uint8_t, 6> UnifiedChronosParameters::magicIntel123456{{0x00, 0x16, 0xea, 0x12, 0x34, 0x56}};
const std::array<uint8_t, 6> UnifiedChronosParameters::fullFF{{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

std::shared_ptr<UnifiedChronosParameters> UnifiedChronosParameters::getInstance(const std::string &phyId) {
    static std::unordered_map<std::string, std::shared_ptr<UnifiedChronosParameters>> instanceMap;
    static bool initializationFlag = false;
    if(!initializationFlag) {
        UnifiedChronosParameters::initializeSharedParameters();
        initializationFlag = true;
    }
    if (instanceMap.find(phyId) == instanceMap.end()) {
        auto instance = std::make_shared<UnifiedChronosParameters>();
        *instance = *UnifiedChronosParameters::sharedParameters;
        instanceMap[phyId] = instance;
    }
    return instanceMap[phyId];
}

void UnifiedChronosParameters::initializeSharedParameters() {
    sharedParameters->inj_target_mac_address = UnifiedChronosParameters::magicIntel123456;
    sharedParameters->inj_mcs = 2;
    sharedParameters->inj_sgi = 1;
    sharedParameters->inj_freq_repeat = 10;
    sharedParameters->inj_freq_step = 1e3;
    sharedParameters->inj_bw = 20;
    sharedParameters->inj_delay_us = 1e6;
    sharedParameters->finishedSessionId = UINT64_MAX;
    sharedParameters->workingSessionId = UINT64_MAX;
    sharedParameters->chronos_timeout_us = 20e3;
    sharedParameters->chronos_ack_maxLengthPerPacket = 1200;
    sharedParameters->delay_after_freq_change_us = 50;
}
