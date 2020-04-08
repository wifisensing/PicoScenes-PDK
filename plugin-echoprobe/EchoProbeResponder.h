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

    void startJob(const EchoProbeParameters &parameters);

    void handle(const struct PicoScenesRxFrameStructure &rxframe);

private:
    std::shared_ptr<AbstractNIC> nic;

    EchoProbeParameters parameters;

    std::vector<std::shared_ptr<PicoScenesFrameBuilder>> makePacket_EchoProbeWithACK(const PicoScenesRxFrameStructure &rxframe, const EchoProbeHeader &epHeader);
};


#endif //PICOSCENES_ECHOPROBERESPONDER_H
