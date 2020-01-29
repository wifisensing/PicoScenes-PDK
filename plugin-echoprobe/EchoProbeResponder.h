//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBERESPONDER_H
#define PICOSCENES_ECHOPROBERESPONDER_H

#include <PicoScenes/PicoScenesNIC.hxx>
#include <PicoScenes/RXSDumper.h>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"
#include "EchoProbeHeader.hxx"

class PicoScenesNIC;

class EchoProbeResponder {
public:
    EchoProbeResponder(const std::shared_ptr<PicoScenesNIC> &nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parameters);

    void handle(const struct PicoScenesRxFrameStructure &rxframe);

private:
    std::shared_ptr<PicoScenesNIC> nic;

    EchoProbeParameters parameters;

    std::vector<std::shared_ptr<PicoScenesFrameBuilder>> makePacket_EchoProbeWithACK(const PicoScenesRxFrameStructure &rxframe, const EchoProbeHeader &epHeader);
};


#endif //PICOSCENES_ECHOPROBERESPONDER_H
