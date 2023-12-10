//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBERESPONDER_H
#define PICOSCENES_ECHOPROBERESPONDER_H

#include <PicoScenes/AbstractNIC.hxx>
#include <PicoScenes/FrameDumper.hxx>
#include "EchoProbe.hxx"
#include "EchoProbeParameters.h"
#include "EchoProbeRequestSegment.hxx"
#include "EchoProbeReplySegment.hxx"

class EchoProbeResponder {
public:
    EchoProbeResponder(const std::shared_ptr<AbstractNIC> &nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parametersV);

    void handle(const struct ModularPicoScenesRxFrame &rxframe);

private:
    std::shared_ptr<AbstractNIC> nic;
    EchoProbeParameters parameters;
    std::optional<PicoScenesDeviceType> initiatorDeviceType;

    std::vector<std::shared_ptr<ModularPicoScenesTxFrame>> makeRepliesFrames(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq);

    std::vector<std::shared_ptr<ModularPicoScenesTxFrame>> makeRepliesForEchoProbeRequestFrames(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq);

    std::vector<std::shared_ptr<ModularPicoScenesTxFrame>> makeRepliesForEchoProbeFreqChangeRequestFrames(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq);
};


#endif //PICOSCENES_ECHOPROBERESPONDER_H
