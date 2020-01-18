//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBERESPONDER_H
#define PICOSCENES_ECHOPROBERESPONDER_H

#include <PicoScenes/PicoScenesNIC.hxx>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"

class PicoScenesNIC;

class EchoProbeResponder : public PropertyJSONDescriptable{
public:
    EchoProbeResponder(const std::shared_ptr<PicoScenesNIC> &hal): hal(hal) {}
    bool handle(const struct RXS_enhanced * rxs);

    std::shared_ptr<EchoProbeParameters> parameters;

    void serialize() override;

    void finalize();

private:
    std::shared_ptr<PicoScenesNIC> hal;

    std::vector<std::shared_ptr<PacketFabricator>> makePacket_EchoProbeWithACK(const struct RXS_enhanced *rxs);

};




#endif //PICOSCENES_ECHOPROBERESPONDER_H
