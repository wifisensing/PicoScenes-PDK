//
// Created by Zhiping Jiang on 10/27/17.
//

#ifndef PICOSCENES_ECHOPROBEINITIATOR_H
#define PICOSCENES_ECHOPROBEINITIATOR_H

#include <unordered_map>
#include <shared_mutex>
#include <PicoScenes/AbstractNIC.hxx>
#include <PicoScenes/AbstractSDRFrontEnd.hxx>
#include <PicoScenes/FrameDumper.hxx>
#include "EchoProbe.hxx"
#include "EchoProbeParameters.h"


class EchoProbeInitiator {
public:

    explicit EchoProbeInitiator(const std::shared_ptr<AbstractNIC> &nic) : nic(nic) {}

    void startJob(const EchoProbeParameters &parametersV);

private:
    std::shared_ptr<AbstractNIC> nic;
    EchoProbeParameters parameters;
    std::optional<PicoScenesDeviceType> responderDeviceType;

    void unifiedEchoProbeWork();

    std::vector<double> enumerateCarrierFrequencies();

    std::vector<double> enumerateArbitraryCarrierFrequencies();

    std::vector<double> enumerateSamplingRates();

    std::vector<double> enumerateArbitrarySamplingRates();

    void printDots(int count) const;

    std::tuple<std::optional<ModularPicoScenesRxFrame>, std::optional<ModularPicoScenesRxFrame>, int, double> transmitAndSyncRxUnified(const std::shared_ptr<ModularPicoScenesTxFrame> &frameBuilder, std::optional<uint32_t> maxRetry = std::nullopt);

    [[nodiscard]] std::shared_ptr<ModularPicoScenesTxFrame> buildBasicFrame(uint16_t taskId, const EchoProbePacketFrameType &frameType, uint16_t sessionId) const;

    [[nodiscard]] std::vector<std::shared_ptr<ModularPicoScenesTxFrame>> buildBatchFrames(const EchoProbePacketFrameType &frameType) const;

    EchoProbeRequest makeRequestSegment(uint16_t sessionId, std::optional<double> newCF = std::nullopt, std::optional<double> newSF = std::nullopt);

    std::vector<double> enumerateIntelMVMCarrierFrequencies();
};

#endif //PICOSCENES_ECHOPROBEINITIATOR_H
