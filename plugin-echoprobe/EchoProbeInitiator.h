//
// Created by Zhiping Jiang on 10/27/17.
//

#ifndef PICOSCENES_ECHOPROBEINITIATOR_H
#define PICOSCENES_ECHOPROBEINITIATOR_H

#include <unordered_map>
#include <shared_mutex>
#include <PicoScenes/PicoScenesNIC.hxx>
#include <PicoScenes/PropertyJSONDescriptable.hxx>
#include <PicoScenes/RXSDumper.h>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"


class EchoProbeInitiator {
public:

    EchoProbeInitiator(std::shared_ptr<PicoScenesNIC> nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parameters);

private:
    std::shared_ptr<PicoScenesNIC> nic;
    std::condition_variable_any blockCV;
    std::condition_variable_any ctrlCCV;
    std::shared_mutex blockMutex;
    EchoProbeParameters parameters;

    int daemonTask();

    void unifiedEchoProbeWork();

    std::vector<double> enumerateCarrierFrequencies();

    std::vector<double> enumerateAtherosCarrierFrequencies();

    std::vector<double> enumerateIntelCarrierFrequencies();

    std::vector<uint32_t> enumerateSamplingFrequencies();

    void printDots(int count);

    std::tuple<std::shared_ptr<PicoScenesRxFrameStructure>, int> transmitAndSyncRxUnified(
            PicoScenesFrameBuilder *frameBuilder, int maxRetry = 0, const std::chrono::steady_clock::time_point *txTime = nullptr);

    std::shared_ptr<PicoScenesFrameBuilder> buildPacket(uint16_t taskId, const EchoProbePacketFrameType &frameType) const;
};

#endif //PICOSCENES_ECHOPROBEINITIATOR_H
