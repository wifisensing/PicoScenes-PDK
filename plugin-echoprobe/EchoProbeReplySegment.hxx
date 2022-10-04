//
// Created by Zhiping Jiang on 11/10/20.
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
    std::string payloadName;

    EchoProbeReply();

    static EchoProbeReply fromBuffer(const uint8_t *buffer, uint32_t length);

    std::vector<uint8_t> toBuffer() const;
};

class EchoProbeReplySegment : public AbstractPicoScenesFrameSegment {
public:
    EchoProbeReplySegment();

    EchoProbeReplySegment(const EchoProbeReply &reply);

    EchoProbeReplySegment(const uint8_t *buffer, uint32_t bufferLength);

    const EchoProbeReply &getEchoProbeReply() const;

    void setEchoProbeReply(const EchoProbeReply &probeReply);

private:
    static std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> versionedSolutionMap;

    static std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> initializeSolutionMap() noexcept;

    EchoProbeReply echoProbeReply;
};


#endif //PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX
