//
// Created by Zhiping Jiang on 10/27/17.
//

#ifndef PICOSCENES_UNIFIEDCHRONOSACTIONRULE_H
#define PICOSCENES_UNIFIEDCHRONOSACTIONRULE_H

#include <unordered_map>
#include <headers/hal/AtherosNicHAL.h>
#include "UnifiedChronos.h"
#include "UnifiedChronosParameters.h"


class UnifiedChronosInitiator {
public:
    UnifiedChronosInitiator(std::shared_ptr<AtherosNicHAL> hal): hal(hal) {}
    void startDaemonTask();
    void blockWait();

    std::shared_ptr<UnifiedChronosParameters> parameters;
private:
    std::shared_ptr<AtherosNicHAL> hal;
    std::condition_variable blockCV;
    std::mutex blockMutex;

    int daemonTask();
    void unifiedChronosWork();

    std::shared_ptr<struct RXS_enhanced> transmitAndSyncRxUnified(
            const PacketFabricator *packetFabricator, const std::chrono::steady_clock::time_point *txTime = nullptr);

    std::shared_ptr<PacketFabricator> buildPacket(uint16_t taskId, const ChronosPacketFrameType & frameType) const;
};

#endif //PICOSCENES_UNIFIEDCHRONOSACTIONRULE_H
