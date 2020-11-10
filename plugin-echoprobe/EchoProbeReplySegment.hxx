//
// Created by csi on 11/10/20.
//

#ifndef PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX
#define PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX

#include <functional>
#include <map>
#include <PicoScenes/PicoScenesCommons.hxx>
#include <PicoScenes/AbstractPicoScenesFrameSegment.hxx>

class EchoProbeReply {
public:
    bool replyCarriesPayload = false;
    Uint8Vector replyBuffer;
};

class EchoProbeReplySegment : AbstractPicoScenesFrameSegment {
public:
    EchoProbeReplySegment();

    EchoProbeReplySegment(const Uint8Vector &replyBuffer);

    void fromBuffer(const uint8_t *buffer, uint32_t bufferLength) override;

    EchoProbeReply reply;

private:

    void updateFieldMap() override;

    static std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> versionedSolutionMap;

    static std::map<uint16_t, std::function<EchoProbeReply(const uint8_t *, uint32_t)>> initializeSolutionMap() noexcept;
};


#endif //PICOSCENES_THREE_PLUGINS_ECHOPROBEREPLYSEGMENT_HXX
