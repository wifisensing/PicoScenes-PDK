//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBEPARAMETERS_H
#define PICOSCENES_ECHOPROBEPARAMETERS_H

#include <array>
#include <memory>
#include <unordered_map>
#include <optional>
#include <PicoScenes/PicoScenesNIC.hxx>
#include "EchoProbe.h"

class EchoProbeParameters {
public:
    EchoProbeParameters();

    EchoProbeWorkingMode workingMode = MODE_Standby;
    bool continue2Work = false;
    std::optional<uint64_t> workingSessionId;
    std::optional<uint64_t> finishedSessionId;
    std::optional<std::string> inj_target_interface;
    std::optional<std::array<uint8_t, 6>> inj_target_mac_address;
    std::optional<bool> inj_for_intel5300;
    std::optional<bool> inj_5300_gf;
    std::optional<bool> inj_5300_duplication;
    uint32_t tx_delay_us;
    std::optional<uint32_t> delayed_start_seconds;
    std::optional<uint8_t> mcs;
    std::optional<uint8_t> bw;
    std::optional<bool> sgi;

    std::optional<int64_t> cf_begin;
    std::optional<int64_t> cf_end;
    std::optional<int64_t> cf_step;
    std::optional<uint32_t> cf_repeat;

    std::optional<int32_t> pll_rate_begin;
    std::optional<int32_t> pll_rate_end;
    std::optional<int32_t> pll_rate_step;

    uint32_t tx_max_retry;
    std::optional<uint8_t> ack_mcs;
    std::optional<uint8_t> ack_bw;
    std::optional<bool> ack_sgi;
    std::optional<uint32_t> timeout_ms;
    std::optional<uint32_t> ack_maxLengthPerPacket;
    std::optional<uint32_t> delay_after_cf_change_ms;
    std::optional<uint32_t> numOfPacketsPerDotDisplay;
};


#endif //PICOSCENES_ECHOPROBEPARAMETERS_H
