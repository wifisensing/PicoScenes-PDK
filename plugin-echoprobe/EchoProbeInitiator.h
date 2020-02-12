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
#include "EchoProbeHeader.hxx"


class EchoProbeInitiator {
public:

    explicit EchoProbeInitiator(const std::shared_ptr<PicoScenesNIC> &nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parameters);

private:
    std::shared_ptr<PicoScenesNIC> nic;
    std::condition_variable_any blockCV;
    std::condition_variable_any ctrlCCV;
    std::shared_mutex blockMutex;
    EchoProbeParameters parameters;

    void unifiedEchoProbeWork();

    std::vector<double> enumerateCarrierFrequencies();

    std::vector<double> enumerateAtherosCarrierFrequencies();

    std::vector<double> enumerateIntelCarrierFrequencies();

    std::vector<uint32_t> enumerateSamplingFrequencies();

    void printDots(int count);

    std::tuple<std::optional<PicoScenesRxFrameStructure>, int> transmitAndSyncRxUnified(PicoScenesFrameBuilder *frameBuilder, std::optional<uint32_t> maxRetry = std::nullopt);

    [[nodiscard]] std::shared_ptr<PicoScenesFrameBuilder> buildBasicFrame(uint16_t taskId, const EchoProbePacketFrameType &frameType) const;
};

#endif //PICOSCENES_ECHOPROBEINITIATOR_H
