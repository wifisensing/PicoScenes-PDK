//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBEPARAMETERS_H
#define PICOSCENES_ECHOPROBEPARAMETERS_H

#include <array>
#include <memory>
#include <unordered_map>
#include <optional>
#include "EchoProbe.hxx"
#include "EchoProbeRequestSegment.hxx"

class EchoProbeParameters {
public:
    EchoProbeParameters();

    EchoProbeWorkingMode workingMode = EchoProbeWorkingMode::Standby;
    std::optional<std::array<uint8_t, 6>> inj_target_mac_address;
    std::optional<bool> inj_for_intel5300;
    uint32_t tx_delay_us;
    std::optional<uint32_t> delayed_start_seconds;
    bool useBatchAPI;
    uint32_t batchLength;

    std::optional<std::string> outputFileName;
    bool randomMAC;
    EchoProbeInjectionContent injectorContent;
    std::optional<uint32_t> randomPayloadLength;

    std::optional<double> cf_begin;
    std::optional<double> cf_end;
    std::optional<double> cf_step;
    std::optional<uint32_t> cf_repeat;

    std::optional<double> sf_begin;
    std::optional<double> sf_end;
    std::optional<double> sf_step;

    uint32_t tx_max_retry;
    EchoProbeReplyStrategy replyStrategy;

    std::optional<PacketFormatEnum> ack_format;
    std::optional<uint32_t> ack_cbw;
    std::optional<uint32_t> ack_mcs;
    std::optional<uint32_t> ack_numSTS;
    std::optional<uint32_t> ack_guardInterval;

    std::optional<uint32_t> timeout_ms;
    std::optional<uint32_t> delay_after_cf_change_ms;
    std::optional<uint32_t> numOfPacketsPerDotDisplay;
};


#endif //PICOSCENES_ECHOPROBEPARAMETERS_H
