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
    std::optional<std::array<uint8_t, 6>> inj_target_mac_address;
    std::optional<bool> inj_for_intel5300;
    uint32_t tx_delay_us;
    std::optional<uint32_t> delayed_start_seconds;
    std::optional<uint32_t> mcs;
    std::optional<uint32_t> numSTS;
    std::optional<uint32_t> cbw;
    std::optional<uint32_t> gi;
    std::optional<uint32_t> ness;

    std::optional<double> cf_begin;
    std::optional<double> cf_end;
    std::optional<double> cf_step;
    std::optional<uint32_t> cf_repeat;

    std::optional<double> sf_begin;
    std::optional<double> sf_end;
    std::optional<double> sf_step;

    uint32_t tx_max_retry;
    bool ack_no_payload;
    std::optional<uint32_t> ack_mcs;
    std::optional<uint32_t> ack_numSTS;
    std::optional<uint32_t> ack_cbw;
    std::optional<uint32_t> ack_gi;
    std::optional<uint32_t> timeout_ms;
    std::optional<uint32_t> ack_maxLengthPerPacket;
    std::optional<uint32_t> delay_after_cf_change_ms;
    std::optional<uint32_t> numOfPacketsPerDotDisplay;
};


#endif //PICOSCENES_ECHOPROBEPARAMETERS_H
