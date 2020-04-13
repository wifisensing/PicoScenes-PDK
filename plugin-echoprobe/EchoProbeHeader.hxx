//
// Created by 蒋志平 on 2020/1/29.
//

#ifndef PICOSCENES_THREE_PLUGINS_ECHOPROBEHEADER_HXX
#define PICOSCENES_THREE_PLUGINS_ECHOPROBEHEADER_HXX

#include <optional>
#include <memory>

struct EchoProbeHeader {
    uint8_t deviceProbingStage = 0;
    int8_t ackMCS = -1;         // 0 to 23 are OK, negative means use default (maybe mcs 0).
    int8_t ackChannelBonding = -1;   // 0 for 20MHz, 1 for 40MHz, negative means use default (maybe 20MHz).
    int8_t ackSGI = -1;         // 0 for LGI, 1 for SGI, negative means use default (maybe LGI).
    int64_t cf = -1;
    int64_t sf = -1;

    static std::optional<EchoProbeHeader> fromBuffer(const uint8_t *buffer, std::optional<uint32_t> bufferLength);
} __attribute__ ((__packed__));


#endif //PICOSCENES_THREE_PLUGINS_ECHOPROBEHEADER_HXX
