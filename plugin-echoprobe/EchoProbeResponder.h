//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBERESPONDER_H
#define PICOSCENES_ECHOPROBERESPONDER_H

#include <PicoScenes/AbstractNIC.hxx>
#include <PicoScenes/RXSDumper.h>
#include <PicoScenes/USRPFrontEnd.hxx>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"
#include "EchoProbeHeader.hxx"

class PicoScenesNIC;

class EchoProbeResponder {
public:
    EchoProbeResponder(const std::shared_ptr<AbstractNIC> &nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parametersV);

    void handle(const struct ModularPicoScenesRxFrame &rxframe);

private:
    std::shared_ptr<AbstractNIC> nic;
    EchoProbeParameters parameters;
    std::optional<PicoScenesDeviceType> initiatorDeviceType;

    std::vector<PicoScenesFrameBuilder> makeReplies(const ModularPicoScenesRxFrame &rxframe, const EchoProbeHeader &epHeader);

    std::vector<PicoScenesFrameBuilder> makeRepliesForEchoProbeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbeHeader &epHeader);

    std::vector<PicoScenesFrameBuilder> makeRepliesForEchoProbeFreqChangeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbeHeader &epHeader);
};


#endif //PICOSCENES_ECHOPROBERESPONDER_H
