//
// Created by csi on 11/10/20.
//

#include "EchoProbeSegment.hxx"


struct EchoProbeV1 {
    bool deviceProbingStage = false;
    bool replyCarriesPayload = true;
    int8_t ackMCS = -1;         // 0 to11 are OK, negative means use default (maybe mcs 0).
    int8_t ackNumSTS = -1;
    int16_t ackCBW = -1;   // -1, 20/40/80/160
    int16_t ackGI = -1;         // 0 for LGI, 1 for SGI, negative means use default (maybe LGI).
    int64_t cf = -1;
    int64_t sf = -1;
} __attribute__ ((__packed__));


static auto v1Parser = [](const uint8_t *buffer, uint32_t bufferLength) -> EchoProbe {
    uint32_t pos = 0;
    if (bufferLength < sizeof(EchoProbeV1))
        throw std::runtime_error("EchoProbeSegment v1Parser cannot parse the segment with insufficient buffer length.");

    auto r = EchoProbe();
    r.deviceProbingStage = *(bool *) (buffer + pos++);
    r.replyCarriesPayload = *(bool *) (buffer + pos++);
    r.ackMCS = *(int8_t *) (buffer + pos++);
    r.ackNumSTS = *(int8_t *) (buffer + pos++);
    r.ackCBW = *(int16_t *) (buffer + pos);
    pos += 2;
    r.ackGI = *(int16_t *) (buffer + pos);
    pos +=2;
    r.cf = *(int64_t *) (buffer + pos);
    pos +=8;
    r.sf = *(int64_t *) (buffer + pos);
    pos +=8;
    return r;
};

std::map<uint16_t, std::function<EchoProbe(const uint8_t *, uint32_t)>> EchoProbeSegment::versionedSolutionMap = initializeSolutionMap();

std::map<uint16_t, std::function<EchoProbe(const uint8_t *, uint32_t)>> EchoProbeSegment::initializeSolutionMap() noexcept {
    return std::map<uint16_t, std::function<EchoProbe(const uint8_t *, uint32_t)>>();
}

EchoProbeSegment::EchoProbeSegment() : AbstractPicoScenesFrameSegment("EchoProbe", 0x1U) {

}

void EchoProbeSegment::fromBuffer(const uint8_t *buffer, uint32_t bufferLength) {
    auto[segmentName, segmentLength, versionId, offset] = extractSegmentMetaData(buffer, bufferLength);
    if (segmentName != "EchoProbe")
        throw std::runtime_error("RxSBasicSegment cannot parse the segment named " + segmentName + ".");
    if (segmentLength + 4 > bufferLength)
        throw std::underflow_error("RxSBasicSegment cannot parse the segment with less than " + std::to_string(segmentLength + 4) + "B.");
    if (!versionedSolutionMap.contains(versionId)) {
        throw std::runtime_error("RxSBasicSegment cannot parse the segment with version v" + std::to_string(versionId) + ".");
    }

    echoProbe = versionedSolutionMap.at(versionId)(buffer + offset, bufferLength - offset);
    rawBuffer.resize(bufferLength);
    std::copy(buffer, buffer + bufferLength, rawBuffer.begin());
}

