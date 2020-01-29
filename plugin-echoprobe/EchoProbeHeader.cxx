//
// Created by 蒋志平 on 2020/1/29.
//

#include "EchoProbeHeader.hxx"

std::optional<EchoProbeHeader> EchoProbeHeader::fromBuffer(const uint8_t *buffer, std::optional<uint32_t> bufferLength) {
    if (bufferLength != sizeof(EchoProbeHeader))
        return std::nullopt;

    EchoProbeHeader echoProbeHeader = *((EchoProbeHeader *) (buffer));
    return echoProbeHeader;
}
