//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBERESPONDER_H
#define PICOSCENES_ECHOPROBERESPONDER_H

#include <PicoScenes/PicoScenesNIC.hxx>
#include <PicoScenes/RXSDumper.h>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"

class PicoScenesNIC;

class EchoProbeResponder {
public:
    EchoProbeResponder(const std::shared_ptr<PicoScenesNIC> &nic) : nic(nic) {}

    void handle(const struct PicoScenesRxFrameStructure &rxframe);

    std::shared_ptr<EchoProbeParameters> parameters;

    void serialize();

    void finalize();

private:
    std::shared_ptr<PicoScenesNIC> nic;

//    std::vector<std::shared_ptr<PacketFabricator>> makePacket_EchoProbeWithACK(const struct RXS_enhanced *rxs);

};


#endif //PICOSCENES_ECHOPROBERESPONDER_H
