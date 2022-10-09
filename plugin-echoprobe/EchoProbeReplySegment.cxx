//
// Created by Zhiping Jiang on 11/10/20.
//

#include "EchoProbeReplySegment.hxx"

struct EchoProbeReplyV1 {
    bool replyCarriesPayload = false;
    uint16_t sessionId;
    std::string payloadName;
};

EchoProbeReply EchoProbeReply::fromBuffer(const uint8_t *buffer, uint32_t length) {
    auto pos = 0;
    EchoProbeReply reply;
    reply.replyStrategy = *(EchoProbeReplyStrategy *) (buffer + pos);
    pos += sizeof(EchoProbeReplyStrategy);
    reply.sessionId = *(uint16_t *) (buffer + pos);
    pos += sizeof(uint16_t);
    auto payloadNameLength = *(uint8_t *) (buffer + pos++);
    reply.payloadName = std::string((char *) (buffer + pos), (char *) (buffer + pos + payloadNameLength));

    return reply;
}

std::vector<uint8_t> EchoProbeReply::toBuffer() const {
    auto buffer = std::vector<uint8_t>();
    std::copy((uint8_t *) &replyStrategy, (uint8_t *) &replyStrategy + sizeof(replyStrategy), std::back_inserter(buffer));
    std::copy((uint8_t *) &sessionId, (uint8_t *) &sessionId + sizeof(sessionId), std::back_inserter(buffer));
    uint8_t payloadNameLength = payloadName.length();
    std::copy((uint8_t *) &payloadNameLength, (uint8_t *) &payloadNameLength + sizeof(payloadNameLength), std::back_inserter(buffer));
    std::copy((uint8_t *) payloadName.data(), (uint8_t *) payloadName.data() + payloadName.length(), std::back_inserter(buffer));
    return buffer;
}

EchoProbeReply::EchoProbeReply() : replyStrategy(EchoProbeReplyStrategy::ReplyOnlyHeader), sessionId(0), payloadName("") {
}


static auto v1Parser = [](const uint8_t *buffer, uint32_t bufferLength) -> EchoProbeReply {
    return EchoProbeReply::fromBuffer(buffer, bufferLength);
};

std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> EchoProbeReplySegment::versionedSolutionMap = initializeSolutionMap();

std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> EchoProbeReplySegment::initializeSolutionMap() noexcept {
    std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> map;
    map.emplace(0x1U, v1Parser);
    return map;
}


EchoProbeReplySegment::EchoProbeReplySegment() : AbstractPicoScenesFrameSegment("EchoProbeReply", 0x1U) {}

EchoProbeReplySegment::EchoProbeReplySegment(const EchoProbeReply &reply) : EchoProbeReplySegment() {
    echoProbeReply = reply;
    setEchoProbeReply(reply);
}

EchoProbeReplySegment::EchoProbeReplySegment(const uint8_t *buffer, uint32_t bufferLength) : AbstractPicoScenesFrameSegment(buffer, bufferLength) {
    if (segmentName != "EchoProbeReply")
        throw std::runtime_error("EchoProbeReplySegment cannot parse the segment named " + segmentName + ".");
    if (!versionedSolutionMap.contains(segmentVersionId))
        throw std::runtime_error("EchoProbeReplySegment cannot parse the segment with version v" + std::to_string(segmentVersionId) + ".");

    echoProbeReply = versionedSolutionMap.at(segmentVersionId)(segmentPayload.data(), segmentPayload.size());
}

const EchoProbeReply &EchoProbeReplySegment::getEchoProbeReply() const {
    return echoProbeReply;
}

void EchoProbeReplySegment::setEchoProbeReply(const EchoProbeReply &probeReply) {
    echoProbeReply = probeReply;
    setSegmentPayload(probeReply.toBuffer());
}
