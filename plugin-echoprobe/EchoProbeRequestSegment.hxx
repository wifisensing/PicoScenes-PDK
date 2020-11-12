//
// Created by csi on 11/10/20.
//

#ifndef PICOSCENES_THREE_PLUGINS_ECHOPROBEREQUESTSEGMENT_HXX
#define PICOSCENES_THREE_PLUGINS_ECHOPROBEREQUESTSEGMENT_HXX

#include <functional>
#include <map>
#include <PicoScenes/PicoScenesCommons.hxx>
#include <PicoScenes/AbstractPicoScenesFrameSegment.hxx>

enum class EchoProbeReplyStrategy: uint8_t {
    ReplyOnlyHeader = 0,
    ReplyWithExtraInfo,
    ReplyWithCSI,
    ReplyWithFullPayload,
};

struct EchoProbeRequest {
    bool deviceProbingStage = false;
    EchoProbeReplyStrategy replyStrategy = EchoProbeReplyStrategy::ReplyWithCSI;
    int16_t ackMCS = -1;         // 0 to11 are OK, negative means use default (maybe mcs 0).
    int16_t ackNumSTS = -1;
    int16_t ackCBW = -1;   // -1, 20/40/80/160
    int16_t ackGI = -1;         // 0 for LGI, 1 for SGI, negative means use default (maybe LGI).
    int64_t cf = -1;
    int64_t sf = -1;
} __attribute__ ((__packed__));

class EchoProbeRequestSegment : public AbstractPicoScenesFrameSegment {
public:
    EchoProbeRequestSegment();

    explicit EchoProbeRequestSegment(const EchoProbeRequest &echoProbeRequest);

    void fromBuffer(const uint8_t *buffer, uint32_t bufferLength) override;

    EchoProbeRequest echoProbeRequest;

private:
    static std::map<uint16_t, std::function<EchoProbeRequest(const uint8_t *, uint32_t)>> versionedSolutionMap;

    static std::map<uint16_t, std::function<EchoProbeRequest(const uint8_t *, uint32_t)>> initializeSolutionMap() noexcept;
};


#endif //PICOSCENES_THREE_PLUGINS_ECHOPROBEREQUESTSEGMENT_HXX
