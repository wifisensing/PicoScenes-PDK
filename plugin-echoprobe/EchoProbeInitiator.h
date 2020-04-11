//
// Created by Zhiping Jiang on 10/27/17.
//

#ifndef PICOSCENES_ECHOPROBEINITIATOR_H
#define PICOSCENES_ECHOPROBEINITIATOR_H

#include <unordered_map>
#include <shared_mutex>
#include <PicoScenes/AbstractNIC.hxx>
#include <PicoScenes/USRPFrontEnd.hxx>
#include <PicoScenes/PropertyJSONDescriptable.hxx>
#include <PicoScenes/RXSDumper.h>
#include "EchoProbe.h"
#include "EchoProbeParameters.h"
#include "EchoProbeHeader.hxx"


class EchoProbeInitiator {
public:

    explicit EchoProbeInitiator(const std::shared_ptr<AbstractNIC> &nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parameters);

private:
    std::shared_ptr<AbstractNIC> nic;
    std::condition_variable_any blockCV;
    std::condition_variable_any ctrlCCV;
    std::shared_mutex blockMutex;
    EchoProbeParameters parameters;

    void unifiedEchoProbeWork();

    std::vector<double> enumerateCarrierFrequencies();

    std::vector<double> enumerateArbitraryCarrierFrequencies();

    std::vector<double> enumerateIntelCarrierFrequencies();

    std::vector<double> enumerateSamplingRates();

    std::vector<double> enumerateArbitrarySamplingRates();

    void printDots(int count);

    std::tuple<std::optional<PicoScenesRxFrameStructure>, std::optional<PicoScenesRxFrameStructure>, int> transmitAndSyncRxUnified(const std::shared_ptr<PicoScenesFrameBuilder> &frameBuilder, std::optional<uint32_t> maxRetry = std::nullopt);

    [[nodiscard]] std::shared_ptr<PicoScenesFrameBuilder> buildBasicFrame(uint16_t taskId, const EchoProbePacketFrameType &frameType) const;
};

#endif //PICOSCENES_ECHOPROBEINITIATOR_H
