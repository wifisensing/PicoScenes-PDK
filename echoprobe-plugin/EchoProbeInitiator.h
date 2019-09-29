//
// Created by Zhiping Jiang on 10/27/17.
//

#ifndef PICOSCENES_ECHOPROBEINITIATOR_H
#define PICOSCENES_ECHOPROBEINITIATOR_H

#include <unordered_map>
#include <shared_mutex>
#include <PicoScenes/AtherosNicHAL.h>
#include <PicoScenes/PropertyJSONDescriptable.hxx>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"


class EchoProbeInitiator : public PropertyJSONDescriptable {
public:
    EchoProbeInitiator(std::shared_ptr<AtherosNicHAL> hal): hal(hal) {}
    void startDaemonTask();
    void blockWait();
    void printDots(int count);
    std::shared_ptr<EchoProbeParameters> parameters;

    void serialize() override;

    void finalize();

private:
    std::shared_ptr<AtherosNicHAL> hal;
    std::condition_variable_any blockCV;
    std::condition_variable_any ctrlCCV;
    std::shared_mutex blockMutex;

    int daemonTask();
    void unifiedEchoProbeWork();

    std::vector<double> enumerateCarrierFrequencies();
    std::vector<double> enumerateAtherosCarrierFrequencies();
    std::vector<double> enumerateIntelCarrierFrequencies();
    std::vector<uint32_t> enumerateSamplingFrequencies();

    std::tuple<std::shared_ptr<struct RXS_enhanced>, int> transmitAndSyncRxUnified(
            PacketFabricator *packetFabricator, int maxRetry = 0, const std::chrono::steady_clock::time_point *txTime = nullptr);

    std::shared_ptr<PacketFabricator> buildPacket(uint16_t taskId, const EchoProbePacketFrameType & frameType) const;
};

#endif //PICOSCENES_ECHOPROBEINITIATOR_H
