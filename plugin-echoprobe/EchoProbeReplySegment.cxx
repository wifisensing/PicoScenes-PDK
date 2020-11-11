//
// Created by csi on 11/10/20.
//

#include "EchoProbeReplySegment.hxx"


struct EchoProbeReplyV1 {
    bool deviceProbingStage = false;
    int8_t ackMCS = -1;         // 0 to11 are OK, negative means use default (maybe mcs 0).
    int8_t ackNumSTS = -1;
    int16_t ackCBW = -1;   // -1, 20/40/80/160
    int16_t ackGI = -1;         // 0 for LGI, 1 for SGI, negative means use default (maybe LGI).
    int64_t cf = -1;
    int64_t sf = -1;
} __attribute__ ((__packed__));


static auto v1Parser = [](const uint8_t *buffer, uint32_t bufferLength) -> EchoProbeReply {
    uint32_t pos = 0;

    auto r = EchoProbeReply();
    return r;
};

std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> EchoProbeReplySegment::versionedSolutionMap = initializeSolutionMap();

std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> EchoProbeReplySegment::initializeSolutionMap() noexcept {
    std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> map;
    map.emplace(0x1U, v1Parser);
    return map;
}


EchoProbeReplySegment::EchoProbeReplySegment(): AbstractPicoScenesFrameSegment("EchoProbeReply", 0x1U) {

}


EchoProbeReplySegment::EchoProbeReplySegment(const Uint8Vector &replyBuffer):  AbstractPicoScenesFrameSegment("EchoProbeReply", 0x1U) {
    reply.replyBuffer = replyBuffer;
}

void EchoProbeReplySegment::fromBuffer(const uint8_t *buffer, uint32_t bufferLength) {
    auto[segmentName, segmentLength, versionId, offset] = extractSegmentMetaData(buffer, bufferLength);
    if (segmentName != "EchoProbeReply")
        throw std::runtime_error("RxSBasicSegment cannot parse the segment named " + segmentName + ".");
    if (segmentLength + 4 > bufferLength)
        throw std::underflow_error("RxSBasicSegment cannot parse the segment with less than " + std::to_string(segmentLength + 4) + "B.");
    if (!versionedSolutionMap.contains(versionId)) {
        throw std::runtime_error("RxSBasicSegment cannot parse the segment with version v" + std::to_string(versionId) + ".");
    }

    reply = versionedSolutionMap.at(versionId)(buffer + offset, bufferLength - offset);
    rawBuffer.resize(bufferLength);
    std::copy(buffer, buffer + bufferLength, rawBuffer.begin());
}

void EchoProbeReplySegment::updateFieldMap() {
    AbstractPicoScenesFrameSegment::updateFieldMap();
}

