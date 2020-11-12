//
// Created by csi on 11/10/20.
//

#include "EchoProbeReplySegment.hxx"

struct EchoProbeReplyV1 {
    bool replyCarriesPayload = false;
    Uint8Vector replyBuffer;
};


static auto v1Parser = [](const uint8_t *buffer, uint32_t bufferLength) -> EchoProbeReply {
    uint32_t pos = 0;
    auto r = EchoProbeReply();
    r.replyCarriesPayload = *(bool *) (buffer + pos++);
    r.replyBuffer.resize(bufferLength - pos);
    std::copy(buffer + pos , buffer + bufferLength, r.replyBuffer.begin());
    return r;
};

std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> EchoProbeReplySegment::versionedSolutionMap = initializeSolutionMap();

std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> EchoProbeReplySegment::initializeSolutionMap() noexcept {
    std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> map;
    map.emplace(0x1U, v1Parser);
    return map;
}


EchoProbeReplySegment::EchoProbeReplySegment(): AbstractPicoScenesFrameSegment("EchoProbeReply", 0x1U) {}

EchoProbeReplySegment::EchoProbeReplySegment(const EchoProbeReply &reply) : EchoProbeReplySegment() {
    echoProbeReply = reply;
    addField("replyWithPayload", uint8_t(echoProbeReply.replyCarriesPayload));
    addField("payload", echoProbeReply.replyBuffer);
}

void EchoProbeReplySegment::fromBuffer(const uint8_t *buffer, uint32_t bufferLength) {
    auto[segmentName, segmentLength, versionId, offset] = extractSegmentMetaData(buffer, bufferLength);
    if (segmentName != "EchoProbeReply")
        throw std::runtime_error("EchoProbeReplySegment cannot parse the segment named " + segmentName + ".");
    if (segmentLength + 4 > bufferLength)
        throw std::underflow_error("EchoProbeReplySegment cannot parse the segment with less than " + std::to_string(segmentLength + 4) + "B.");
    if (!versionedSolutionMap.contains(versionId)) {
        throw std::runtime_error("EchoProbeReplySegment cannot parse the segment with version v" + std::to_string(versionId) + ".");
    }

    echoProbeReply = versionedSolutionMap.at(versionId)(buffer + offset, bufferLength - offset);
    rawBuffer.resize(bufferLength);
    std::copy(buffer, buffer + bufferLength, rawBuffer.begin());
}

uint32_t EchoProbeReplySegment::toBuffer(bool totalLengthIncluded, uint8_t *buffer, std::optional<uint32_t> capacity) const {
    return AbstractPicoScenesFrameSegment::toBuffer(totalLengthIncluded, buffer, capacity);
}


