//
// Created by csi on 11/10/20.
//

#ifndef PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX
#define PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX

#include <functional>
#include <map>
#include <PicoScenes/PicoScenesCommons.hxx>
#include <PicoScenes/AbstractPicoScenesFrameSegment.hxx>
#include "EchoProbeRequestSegment.hxx"

class EchoProbeReply {
public:
    EchoProbeReplyStrategy replyStrategy;
    uint16_t sessionId;
    Uint8Vector replyBuffer;

    std::vector<uint8_t> toBuffer();
};

class EchoProbeReplySegment : public AbstractPicoScenesFrameSegment {
public:
    EchoProbeReplySegment();

    EchoProbeReplySegment(const EchoProbeReply & reply);

    void fromBuffer(const uint8_t *buffer, uint32_t bufferLength) override;

    uint32_t toBuffer(bool totalLengthIncluded, uint8_t *buffer, std::optional<uint32_t> capacity) const override;

    std::vector<uint8_t> toBuffer() const override;

    const EchoProbeReply &getEchoProbeReply() const;

    void setEchoProbeReply(const EchoProbeReply & probeReply);

private:
    static std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> versionedSolutionMap;

    static std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> initializeSolutionMap() noexcept;

    EchoProbeReply echoProbeReply;
};


#endif //PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX
