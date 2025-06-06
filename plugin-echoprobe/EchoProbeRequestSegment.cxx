//
// Created by Zhiping Jiang on 11/10/20.
//

#include "EchoProbeRequestSegment.hxx"

enum class EchoProbeReplyStrategyV1 : uint8_t {
    ReplyOnlyHeader = 0,
    ReplyWithExtraInfo,
    ReplyWithCSI,
    ReplyWithFullPayload,
};

PACKED(struct EchoProbeRequestV1 {
    bool deviceProbingStage = false;
    EchoProbeReplyStrategyV1 replyStrategy = EchoProbeReplyStrategyV1::ReplyWithCSI;
    uint16_t sessionId = 0;
    int16_t repeat = -1;
    int16_t ackMCS = -1;         // 0 to11 are OK, negative means use default (maybe mcs 0).
    int16_t ackNumSTS = -1;
    int16_t ackCBW = -1;   // -1, 20/40/80/160
    int16_t ackGI = -1;         // 0 for LGI, 1 for SGI, negative means use default (maybe LGI).
    int64_t cf = -1;
    int64_t sf = -1;
});


static auto v1Parser = [](const uint8_t *buffer, uint32_t bufferLength) -> EchoProbeRequest {
    uint32_t pos = 0;
    if (bufferLength < sizeof(EchoProbeRequestV1))
        throw std::runtime_error("EchoProbeSegment v1Parser cannot parse the segment with insufficient buffer length.");

    auto r = EchoProbeRequest();
    r.deviceProbingStage = *(bool *) (buffer + pos++);
    r.replyStrategy = *(EchoProbeReplyStrategy *) (buffer + pos++);
    r.sessionId = *(uint16_t *) (buffer + pos);
    pos += 2;
    r.repeat = *(int16_t *) (buffer + pos);
    pos += 2;
    r.ackMCS = *(int16_t *) (buffer + pos);
    pos += 2;
    r.ackNumSTS = *(int16_t *) (buffer + pos);
    pos += 2;
    r.ackCBW = *(int16_t *) (buffer + pos);
    pos += 2;
    r.ackGI = *(int16_t *) (buffer + pos);
    pos += 2;
    r.cf = *(int64_t *) (buffer + pos);
    pos += 8;
    r.sf = *(int64_t *) (buffer + pos);
    pos += 8;

    if (pos != bufferLength)
        throw std::runtime_error("EchoProbeSegment v1Parser cannot parse the segment with mismatched buffer length.");

    return r;
};

std::vector<uint8_t> EchoProbeRequest::toBuffer() {
    return std::vector<uint8_t>((uint8_t *) this, (uint8_t *) this + sizeof(EchoProbeRequest));
}

std::map<uint16_t, std::function<EchoProbeRequest(const uint8_t *, uint32_t)>> EchoProbeRequestSegment::versionedSolutionMap = initializeSolutionMap();

std::map<uint16_t, std::function<EchoProbeRequest(const uint8_t *, uint32_t)>> EchoProbeRequestSegment::initializeSolutionMap() noexcept {
    std::map<uint16_t, std::function<EchoProbeRequest(const uint8_t *, uint32_t)>> map;
    map.emplace(0x1U, v1Parser);
    return map;
}

EchoProbeRequestSegment::EchoProbeRequestSegment() : AbstractPicoScenesFrameSegment("EchoProbeRequest", 0x1U) {}

EchoProbeRequestSegment::EchoProbeRequestSegment(const EchoProbeRequest &echoProbeRequestV) : EchoProbeRequestSegment() {
    setEchoProbeRequest(echoProbeRequestV);
}

EchoProbeRequestSegment::EchoProbeRequestSegment(const uint8_t *buffer, uint32_t bufferLength) : AbstractPicoScenesFrameSegment(buffer, bufferLength) {
    if (segmentName != "EchoProbeRequest")
        throw std::runtime_error("EchoProbeRequestSegment cannot parse the segment named " + segmentName + ".");
    if (!versionedSolutionMap.contains(segmentVersionId))
        throw std::runtime_error("EchoProbeRequestSegment cannot parse the segment with version v" + std::to_string(segmentVersionId) + ".");

    echoProbeRequest = versionedSolutionMap.at(segmentVersionId)(segmentPayload.data(), segmentPayload.size());
}

void EchoProbeRequestSegment::setEchoProbeRequest(const EchoProbeRequest &probeRequest) {
    echoProbeRequest = probeRequest;
    setSegmentPayload(echoProbeRequest.toBuffer());
}

const EchoProbeRequest &EchoProbeRequestSegment::getEchoProbeRequest() const {
    return echoProbeRequest;
}