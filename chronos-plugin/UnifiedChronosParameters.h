//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_UNIFIEDCHRONOSPARAMETERS_H
#define PICOSCENES_UNIFIEDCHRONOSPARAMETERS_H

#include <array>
#include <memory>
#include <unordered_map>
#include <boost/optional.hpp>
#include <PicoScenes/BaseHeader.h>
#include "UnifiedChronos.h"
using namespace boost;

class UnifiedChronosParameters {
public:
    static std::shared_ptr<UnifiedChronosParameters> sharedParameters;
    static std::shared_ptr<UnifiedChronosParameters> getInstance(const std::string &phyId);

    static const std::array<uint8_t, 6> magicIntel123456;
    static const std::array<uint8_t, 6> fullFF;

    optional<uint64_t> workingSessionId;
    optional<uint64_t> finishedSessionId;
    optional<std::string> inj_target_interface;
    optional<std::array<uint8_t, 6>> inj_target_mac_address;
    optional<bool>     inj_for_intel5300;
    optional<uint32_t> inj_delay_us;
    optional<uint32_t> inj_delayed_start_s;
    optional<uint8_t>  inj_mcs;
    optional<uint8_t>  inj_bw;
    optional<uint8_t>  inj_sgi;
    optional<int64_t>  inj_freq_begin;
    optional<int64_t>  inj_freq_end;
    optional<int64_t>  inj_freq_step;
    optional<uint32_t> inj_freq_repeat;

    optional<int64_t> chronos_inj_freq_gap;
    optional<uint8_t>  chronos_ack_mcs;
    optional<uint8_t>  chronos_ack_bw;
    optional<uint8_t>  chronos_ack_sgi;
    optional<uint32_t> chronos_ack_additional_delay;
    optional<uint32_t> chronos_timeout_us;
    optional<uint32_t> chronos_ack_maxLengthPerPacket;
    optional<uint32_t> delay_after_freq_change_us;
    optional<uint32_t> numOfPacketsPerDotDisplay;

private:
    static void initializeSharedParameters();
};


#endif //PICOSCENES_UNIFIEDCHRONOSPARAMETERS_H
