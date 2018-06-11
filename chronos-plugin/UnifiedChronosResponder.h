//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_UNIFIEDCHRONOSRESPONDER_H
#define PICOSCENES_UNIFIEDCHRONOSRESPONDER_H

#include <PicoScenes/AtherosNicHAL.h>
#include "UnifiedChronos.h"
#include "UnifiedChronosParameters.h"

class AtherosNicHAL;

class UnifiedChronosResponder : public PropertyJSONDescriptable{
public:
    UnifiedChronosResponder(const std::shared_ptr<AtherosNicHAL> &hal): hal(hal) {}
    bool handle(const struct RXS_enhanced * rxs);

    std::shared_ptr<UnifiedChronosParameters> parameters;

    void serialize() override;

private:
    std::shared_ptr<AtherosNicHAL> hal;

    std::vector<std::shared_ptr<PacketFabricator>> makePacket_chronosWithACK(const struct RXS_enhanced * rxs);

};




#endif //PICOSCENES_UNIFIEDCHRONOSRESPONDER_H
