//
// Created by Zhiping Jiang on 10/27/17.
//

#include <PicoScenes/SystemTools.hxx>
#include <PicoScenes/MAC80211CSIExtractableNIC.hxx>
#include <cstdint>
#include "EchoProbeInitiator.hxx"
#include "EchoProbeReplySegment.hxx"
#include "EchoProbeRequestSegment.hxx"


void EchoProbeInitiator::startJob(const EchoProbeParameters& parametersV) {
    this->parameters = parametersV;
    unifiedEchoProbeWork();
}

void EchoProbeInitiator::unifiedEchoProbeWork() {
    auto frontEnd = nic->getFrontEnd();
    auto total_acked_count = 0, total_tx_count = 0;
    auto tx_count = 0, acked_count = 0;
    auto total_mean_delay = 0.0, mean_delay_single = 0.0;
    auto workingMode = parameters.workingMode;
    auto round_repeat = parameters.round_repeat.value_or(1);
    auto cf_repeat = parameters.cf_repeat.value_or(100);
    auto tx_delay_us = parameters.tx_delay_us;
    auto tx_delayed_start = parameters.delayed_start_seconds.value_or(0);

    auto sfList = enumerateSamplingRates();
    auto cfList = enumerateCarrierFrequencies();

    // save the prebuilt frames
    std::vector<ModularPicoScenesTxFrame> prebuiltFrames;

    auto sessionId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
    LoggingService_Plugin_info_print("EchoProbe job parameters: sf--> {} : {} : {}MHz, cf--> {} : {} : {}MHz, {}K repeats with {}us interval {}s delayed start.",
                                     sfList.front() / 1e6, parameters.sf_step.value_or(0) / 1e6, sfList.back() / 1e6, cfList.front() / 1e6, parameters.cf_step.value_or(0) / 1e6, cfList.back() / 1e6, cf_repeat / 1e3, tx_delay_us, tx_delayed_start);

    if (tx_delayed_start > 0)
        std::this_thread::sleep_for(std::chrono::seconds(tx_delayed_start));

    // Prebuilt all test frames to save PPDU generation time
    if (parameters.useBatchAPI) {
        prebuiltFrames = buildBatchFrames(EchoProbePacketFrameType::SimpleInjectionFrameType);
    }

    for (auto ri = 0; ri < round_repeat; ri++) {
        for (const auto& sf_value: sfList) {
            auto dumperId = fmt::sprintf("EPI_%s_%u_bb%.1fM", nic->getReferredInterfaceName(), sessionId, sf_value / 1e6);
            for (const auto& cf_value: cfList) {
                if (workingMode == EchoProbeWorkingMode::Injector || workingMode == EchoProbeWorkingMode::Radar) {
                    if (sf_value != frontEnd->getSamplingRate()) {
                        LoggingService_Plugin_info_print("EchoProbe injector shifting {}'s baseband sampling rate to {}MHz...", nic->getReferredInterfaceName(), sf_value / 1e6);
                        frontEnd->setSamplingRate(sf_value);
                        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                    }

                    if (cf_value != frontEnd->getCarrierFrequency()) {
                        LoggingService_Plugin_info_print("EchoProbe injector shifting {}'s carrier frequency to {}MHz...", nic->getReferredInterfaceName(), cf_value / 1e6);
                        frontEnd->setCarrierFrequency(cf_value);
                        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                    }
                } else if (workingMode == EchoProbeWorkingMode::EchoProbeInitiator) {
                    bool shiftSF = false, shiftCF = false;
                    if (sf_value != frontEnd->getSamplingRate()) {
                        LoggingService_Plugin_info_print("EchoProbe initiator shifting {}'s baseband sampling rate to {}MHz...", nic->getReferredInterfaceName(), sf_value);
                        shiftSF = true;
                    }
                    if (cf_value != frontEnd->getCarrierFrequency()) {
                        LoggingService_Plugin_info_print("EchoProbe initiator shifting {}'s carrier frequency to {}MHz...", nic->getReferredInterfaceName(), (double) cf_value / 1e6);
                        shiftCF = true;
                    }

                    if (shiftCF || shiftSF) {
                        auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                        auto txframe = buildBasicFrame(taskId, EchoProbePacketFrameType::EchoProbeFreqChangeRequestFrameType, sessionId);
                        txframe.addSegment(std::make_shared<EchoProbeRequestSegment>(makeRequestSegment(sessionId, shiftCF ? std::optional<double>(cf_value) : std::nullopt, shiftSF ? std::optional<double>(sf_value) : std::nullopt)));
                        auto currentCF = frontEnd->getCarrierFrequency();
                        auto currentSF = frontEnd->getSamplingRate();
                        auto nextCF = cf_value;
                        auto nextSF = sf_value;
                        auto connectionEstablished = false;
                        for (auto i = 0; i < parameters.tx_max_retry; i++) {
                            if (auto [rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(txframe, 2); rxframe) {
                                LoggingService_Plugin_info_print("EchoProbe responder confirms the channel changes.");
                                if (shiftSF) frontEnd->setSamplingRate(nextSF);
                                if (shiftCF) frontEnd->setCarrierFrequency(nextCF);
                                std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                                connectionEstablished = true;
                                break;
                            } else {
                                if (shiftSF) frontEnd->setSamplingRate(nextSF);
                                if (shiftCF) frontEnd->setCarrierFrequency(nextCF);
                                std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                                if (auto [rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(txframe, 2); rxframe) {
                                    LoggingService_Plugin_info_print("EchoProbe responder confirms the channel changes.");
                                    if (shiftSF) frontEnd->setSamplingRate(nextSF);
                                    if (shiftCF) frontEnd->setCarrierFrequency(nextCF);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                                    connectionEstablished = true;
                                    break;
                                } else {
                                    if (shiftSF) frontEnd->setSamplingRate(currentSF);
                                    if (shiftCF) frontEnd->setCarrierFrequency(currentCF);
                                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                                }
                            }
                        }

                        if (!connectionEstablished)
                            goto failed;
                    }
                }

                tx_count = 0;
                acked_count = 0;
                mean_delay_single = 0.0;

                if (workingMode == EchoProbeWorkingMode::Injector || workingMode == EchoProbeWorkingMode::Radar) {
                    // In batch mode, Tx directly the gen-ed PPDU, saving quite a lot of PPDU gen time.
                    if (parameters.useBatchAPI) {
                        std::vector<const ModularPicoScenesTxFrame *> framePoints;
                        for (const auto& frame: prebuiltFrames) {
                            framePoints.emplace_back(&frame);
                        }
                        auto repeats = std::ceil(1.0f * cf_repeat / framePoints.size());
                        nic->getFrontEnd()->transmitFramesInBatch(framePoints, repeats);
                    } else {
                        for (uint32_t i = 0; i < cf_repeat; ++i) {
                            auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                            auto txframe = buildBasicFrame(taskId, EchoProbePacketFrameType::SimpleInjectionFrameType, sessionId);
                            if (parameters.napa.value_or(false)){
                                ModularPicoScenesTxFrame ndpa_frame;
                                U8Vector mpduData = {
                                    0x54, 0x00, 0x64, 0x00, 0x00, 0x16, 0xea, 0x12,
                                    0x34, 0x56, 0x49, 0x5f, 0x08, 0x22, 0xc2, 0x00,
                                    0xe6, 0x08, 0x00, 0x24, 0x09, 0x88, 0x1d, 0x9c,
                                    0x46
                                };
                                std::vector<U8Vector> ampduContent;
                                ampduContent.emplace_back(mpduData);
                                ndpa_frame.arbitraryAMPDUContent = ampduContent;
                                ndpa_frame.txParameters.frameType = PacketFormatEnum::PacketFormat_VHT;
                                ndpa_frame.txParameters.postfixPaddingTime = 16.0e-6;//对于802.11a/g/n/ac/ax, SIFS的典型值为16us; 对于802.11b/g/n, SIFS的典型值为10us; 对于802.11ad, SIFS的典型值为3us
                                std::vector<ModularPicoScenesTxFrame> ndpa_ndp_frame{ndpa_frame, txframe};

                                nic->transmitFramesInBatch(ndpa_ndp_frame, 1);


                            }else{
                                nic->transmitPicoScenesFrameSync(txframe);
                            }
                            tx_count++;
                            total_tx_count++;
                            printDots(tx_count);
                            SystemTools::Time::delay_periodic(parameters.tx_delay_us);
                        }
                    }

                    LoggingService_Plugin_info_printf("\nEchoProbe injector %s @ cf=%.3fMHz, sf=%.3fMHz, #.tx=%d.", nic->getReferredInterfaceName(), (double) cf_value / 1e6, (double) sf_value / 1e6, tx_count);
                }

                if (workingMode == EchoProbeWorkingMode::EchoProbeInitiator) {
                    for (uint32_t i = 0; i < cf_repeat; ++i) {
                        auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                        auto txframe = buildBasicFrame(taskId, EchoProbePacketFrameType::EchoProbeRequestFrameType, sessionId);
                        txframe.addSegment(std::make_shared<EchoProbeRequestSegment>(makeRequestSegment(sessionId)));
                        auto [rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(txframe);
                        tx_count += retryPerTx;
                        total_tx_count += retryPerTx;
                        if (rxframe && ackframe) {
                            acked_count++;
                            total_acked_count++;
                            mean_delay_single += rtDelay / cf_repeat;
                            total_mean_delay += rtDelay / cf_repeat / cfList.size() / sfList.size();
                            if (parameters.outputFileName)
                                FrameDumper::getInstanceWithoutTime(*parameters.outputFileName)->dumpRxFrame(rxframe.value());
                            else
                                FrameDumper::getInstance(dumperId)->dumpRxFrame(rxframe.value());
                            LoggingService_Plugin_detail_print("TaskId {} done!", int(rxframe->PicoScenesHeader->taskId));
                            printDots(acked_count);
                            SystemTools::Time::delay_periodic(parameters.tx_delay_us);
                        } else {
                            printf("\n");
                            LoggingService_Plugin_warning_print("EchoProbe Job Warning: max retry times reached during measurement @ {}Hz...", cf_value);
                            goto failed;
                        }
                    }

                    LoggingService_Plugin_info_printf("\nEchoProbe initiator %s @ cf=%.3fMHz, sf=%.3fMHz, #.tx=%d, #.acked=%d, echo_delay=%.1fms, success_rate=%.1f%%.", nic->getReferredInterfaceName(), (double) cf_value / 1e6, (double) sf_value / 1e6, tx_count, acked_count, mean_delay_single, double(100.0 * acked_count / tx_count));
                }
            }

            FrameDumper::getInstance(dumperId)->finishCurrentSession();
            continue;

        failed:
            FrameDumper::getInstance(dumperId)->finishCurrentSession();
            break;
        }
    }

    if (workingMode == EchoProbeWorkingMode::Injector)
        LoggingService_Plugin_info_print("Job done! #.total_tx=%d.", total_tx_count);
    else if (workingMode == EchoProbeWorkingMode::EchoProbeInitiator)
        LoggingService_Plugin_info_print("Job done! #.total_tx=%d #.total_acked=%d, echo_delay=%.1fms, success_rate =%.1f%%.", total_tx_count, total_acked_count, total_mean_delay, 100.0 * total_acked_count / total_tx_count);
}

std::tuple<std::optional<ModularPicoScenesRxFrame>, std::optional<ModularPicoScenesRxFrame>, int, double> EchoProbeInitiator::transmitAndSyncRxUnified(ModularPicoScenesTxFrame& frame, std::optional<uint32_t> maxRetry) {
    auto taskId = frame.frameHeader->taskId;
    auto retryCount = 0;
    auto timeGap = -1.0;
    maxRetry = (maxRetry ? *maxRetry : parameters.tx_max_retry);

    while (retryCount++ < *maxRetry) {
        frame.frameHeader->txId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(100, UINT16_MAX);
        /*
        * Tx-Rx time grows non-linearly in low sampling rate cases, enlarge the timeout to 11x.
        */
        auto timeout_us_scaling = nic->getFrontEnd()->getSamplingRate() < 20e6 ? 6 : 1;
        auto totalTimeOut = timeout_us_scaling * *parameters.timeout_ms;
        if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
            totalTimeOut += 50;
        }

        if (!responderDeviceType || responderDeviceType == PicoScenesDeviceType::USRP)
            totalTimeOut += 50;

        totalTimeOut += 50 * (int(frame.txParameters.cbw) / 20);
        auto tx_time = std::chrono::system_clock::now();
        nic->transmitPicoScenesFrame(frame);
        auto replyFrame = nic->syncRxConditionally([=](const ModularPicoScenesRxFrame& rxframe) -> bool {
            return rxframe.PicoScenesHeader && (rxframe.PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeReplyFrameType) || rxframe.PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeFreqChangeACKFrameType)) && rxframe.PicoScenesHeader->taskId == taskId;
        }, std::chrono::milliseconds(totalTimeOut), "taskId[" + std::to_string(taskId) + "]");

        if (replyFrame && replyFrame->PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeReplyFrameType)) {
            auto delayDuration = std::chrono::system_clock::now() - tx_time;
            timeGap = double(std::chrono::duration_cast<std::chrono::microseconds>(delayDuration).count()) / 1000.0;
            responderDeviceType = (PicoScenesDeviceType)replyFrame->PicoScenesHeader->deviceType;
            const auto echoProbeReplySegment = replyFrame->txUnknownSegments.at("EchoProbeReply");
            EchoProbeReplySegment replySeg(echoProbeReplySegment->getSyncedRawBuffer().data(), echoProbeReplySegment->getSyncedRawBuffer().size());
            if (replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyOnlyHeader || replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyWithExtraInfo) {
                LoggingService_Plugin_debug_printf("Round-trip delay %.3fms, only header", timeGap);
                return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
            }

            if (replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyWithCSI) {
                const auto payloadName = replySeg.getEchoProbeReply().payloadName;
                if (auto foundIt = std::find_if(replyFrame->payloadSegments.cbegin(), replyFrame->payloadSegments.cend(), [payloadName](const std::shared_ptr<PayloadSegment>& payloadSegment) {
                    return payloadSegment->getPayloadData().payloadDescription == payloadName;
                }); foundIt != replyFrame->payloadSegments.cend()) {
                    LoggingService_Plugin_debug_printf("Round-trip delay %.3fms, only CSI", timeGap);
                    return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
                }
            }

            if (replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyWithFullPayload) {
                const auto payloadName = replySeg.getEchoProbeReply().payloadName;
                if (auto foundIt = std::find_if(replyFrame->payloadSegments.cbegin(), replyFrame->payloadSegments.cend(), [payloadName](const std::shared_ptr<PayloadSegment>& payloadSegment) {
                    return payloadSegment->getPayloadData().payloadDescription == payloadName;
                }); foundIt != replyFrame->payloadSegments.cend()) {
                    if (auto ackFrame = ModularPicoScenesRxFrame::fromBuffer(foundIt->get()->getPayloadData().payloadData.data(), foundIt->get()->getPayloadData().payloadData.size())) {
                        LoggingService_Plugin_debug_print("Raw ACK: {}", replyFrame->toString());
                        LoggingService_Plugin_debug_print("ACKed Tx: {}", ackFrame->toString());
                        LoggingService_Plugin_debug_printf("Round-trip delay %.3fms, full payload", timeGap);
                        return std::make_tuple(replyFrame, ackFrame, retryCount, timeGap);
                    }
                }
            }
        }

        if (replyFrame && replyFrame->PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeFreqChangeACKFrameType)) {
            return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
        }
    }

    return std::make_tuple(std::nullopt, std::nullopt, 0, 0);
}

ModularPicoScenesTxFrame EchoProbeInitiator::buildBasicFrame(uint16_t taskId, const EchoProbePacketFrameType& frameType, uint16_t sessionId) const {
    auto frame = nic->initializeTxFrame();

    if (frameType == EchoProbePacketFrameType::SimpleInjectionFrameType && parameters.injectorContent == EchoProbeInjectionContent::NDP) {
        /**
         * @brief PicoScenes Platform CLI parser has *absorbed* the common Tx parameters.
         * The platform parser will parse the Tx parameters options and store the results in AbstractNIC.
         * Plugin developers now can access the parameters via a new method nic->getUserSpecifiedTxParameters().
         */
        frame.setTxParameters(nic->getUserSpecifiedTxParameters()).txParameters.NDPFrame = true;
    } else {
        /**
         * @brief PicoScenes Platform CLI parser has *absorbed* the common Tx parameters.
         * The platform parser will parse the Tx parameters options and store the results in AbstractNIC.
         * Plugin developers now can access the parameters via a new method nic->getUserSpecifiedTxParameters().
         */
        frame.setTxParameters(nic->getUserSpecifiedTxParameters());
        frame.setTaskId(taskId);
        frame.setPicoScenesFrameType(static_cast<uint8_t>(frameType));

        if (frameType == EchoProbePacketFrameType::SimpleInjectionFrameType && parameters.injectorContent == EchoProbeInjectionContent::Full) {
            frame.addSegment(std::make_shared<ExtraInfoSegment>(nic->getFrontEnd()->buildExtraInfo()));
        }

        if (frameType == EchoProbePacketFrameType::EchoProbeRequestFrameType)
            frame.addSegment(std::make_shared<ExtraInfoSegment>(nic->getFrontEnd()->buildExtraInfo()));

        if (parameters.randomPayloadLength) {
            std::vector<uint8_t> vec;
            vec.reserve(*parameters.randomPayloadLength);
            for (auto i = 0; i < *parameters.randomPayloadLength; i++)
                vec.emplace_back(i % 256);
            auto segment = std::make_shared<PayloadSegment>("RandomPayload", vec, PayloadDataType::RawData);
            frame.addSegment(segment);
        }
    }

    auto sourceAddr = nic->getFrontEnd()->getMacAddressPhy();
    if (parameters.randomMAC) {
        #ifdef _WIN32
        uint16_t randAddr = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(0, UINT16_MAX);
        sourceAddr[0] = randAddr & UINT8_MAX;
        sourceAddr[1] = (randAddr >> 8) & UINT8_MAX;
        #else
        sourceAddr[0] = SystemTools::Math::uniformRandomNumberWithinRange<uint8_t>(0, UINT8_MAX);
        sourceAddr[1] = SystemTools::Math::uniformRandomNumberWithinRange<uint8_t>(0, UINT8_MAX);
        #endif
    }
    frame.setSourceAddress(sourceAddr.data());
    frame.setDestinationAddress(parameters.inj_target_mac_address ? parameters.inj_target_mac_address->data() : MagicIntel123456.data());
    frame.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());
    frame.txParameters.forceSounding = true;

    if (parameters.inj_for_intel5300.value_or(false)) {
        frame.setSourceAddress(MagicIntel123456.data());
        frame.setDestinationAddress(MagicIntel123456.data());
        frame.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());
        frame.setForceSounding(false);
        frame.setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
    }

    if (parameters.napa.value_or(false)) {
        auto frame = nic->initializeTxFrame();
        // frame.setTxParameters(nic->getUserSpecifiedTxParameters()).txParameters.NDPFrame = true;
        frame.setTxParameters(nic->getUserSpecifiedTxParameters());
        frame.txParameters.frameType = PacketFormatEnum::PacketFormat_VHT;
        frame.txParameters.NDPFrame = true;
        auto sourceAddr = nic->getFrontEnd()->getMacAddressPhy();
        frame.setSourceAddress(sourceAddr.data());
        frame.setDestinationAddress(parameters.inj_target_mac_address ? parameters.inj_target_mac_address->data() : MagicIntel123456.data());
        frame.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());
        frame.txParameters.forceSounding = true;
        return frame;
    }

    return frame;
}

std::vector<ModularPicoScenesTxFrame> EchoProbeInitiator::buildBatchFrames(const EchoProbePacketFrameType& frameType) const {
    std::vector<ModularPicoScenesTxFrame> frameBatches;

    auto batchLength = *parameters.cf_repeat > parameters.batchLength ? parameters.batchLength : *parameters.cf_repeat;
    LoggingService_Plugin_info_print("Building {} EchoProbe frames spaced by {} us...", batchLength, parameters.tx_delay_us);
    for (uint32_t frameIndex = 0; frameIndex < batchLength; frameIndex++) {
        auto frame = nic->initializeTxFrame();
        frame.setTaskId(SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, 30000))
                .setPicoScenesFrameType(static_cast<uint8_t>(frameType))
                .addSegment(std::make_shared<ExtraInfoSegment>(nic->getFrontEnd()->buildExtraInfo()))
                .setDestinationAddress(MagicIntel123456.data())
                .setSourceAddress(parameters.inj_for_intel5300 ? MagicIntel123456.data() : nic->getFrontEnd()->getMacAddressPhy().data())
                .set3rdAddress(parameters.inj_for_intel5300 ? MagicIntel123456.data() : nic->getFrontEnd()->getMacAddressPhy().data())
                .setTxParameters(nic->getUserSpecifiedTxParameters())
                .setForceSounding(!parameters.inj_for_intel5300)
                .setChannelCoding(parameters.inj_for_intel5300 ? ChannelCodingEnum::BCC : frame.txParameters.coding[0]);
        frame.txParameters.NDPFrame = frameType == EchoProbePacketFrameType::SimpleInjectionFrameType && parameters.injectorContent == EchoProbeInjectionContent::NDP;
        if (parameters.randomPayloadLength) {
            std::vector<uint8_t> vec(*parameters.randomPayloadLength);
            std::generate(vec.begin(), vec.end(), []() { return rand() % 256; });
            auto segment = std::make_shared<PayloadSegment>("RandomPayload", vec, PayloadDataType::RawData);
            frame.addSegment(segment);
        }

        auto splitFrames = frame.autoSplit(1350);

        if (isSDR(nic->getFrontEnd()->getFrontEndType()) && !splitFrames.empty()) {
            auto signals = nic->getTypedFrontEnd<AbstractSDRFrontEnd>()->generateMultiChannelSignals(splitFrames[0], nic->getFrontEnd()->getTxChannels().size());
            auto signalLength = signals[0].size();
            auto perPacketDurationUs = static_cast<double>(signalLength) * 1e6 / nic->getTypedFrontEnd<AbstractSDRFrontEnd>()->getTxSamplingRate();

            std::for_each(splitFrames.begin(), splitFrames.end(), [&](ModularPicoScenesTxFrame& currentFrame) {
                auto actualIdleTimePerFrameUs = parameters.tx_delay_us - perPacketDurationUs;
                currentFrame.txParameters.postfixPaddingTime = actualIdleTimePerFrameUs / 1e6;
                nic->getTypedFrontEnd<AbstractSDRFrontEnd>()->prebuildSignals(currentFrame, nic->getFrontEnd()->getTxChannels().size());
            });
        }

        std::copy(splitFrames.cbegin(), splitFrames.cend(), std::back_inserter(frameBatches));
    }

    return frameBatches;
}

EchoProbeRequest EchoProbeInitiator::makeRequestSegment(uint16_t sessionId, std::optional<double> newCF, std::optional<double> newSF) {
    EchoProbeRequest echoProbeRequest;
    echoProbeRequest.sessionId = sessionId;
    if (!responderDeviceType)
        echoProbeRequest.deviceProbingStage = true;

    if (newCF || newSF) {
        echoProbeRequest.replyStrategy = EchoProbeReplyStrategy::ReplyOnlyHeader;
        echoProbeRequest.repeat = 5;
        echoProbeRequest.ackMCS = 0;
        echoProbeRequest.ackNumSTS = 1;
        echoProbeRequest.ackCBW = -1;
        echoProbeRequest.ackGI = int16_t(GuardIntervalEnum::GI_800);
        if (newCF)
            echoProbeRequest.cf = *newCF;
        if (newSF)
            echoProbeRequest.sf = *newSF;
    } else {
        echoProbeRequest.replyStrategy = parameters.replyStrategy;
        echoProbeRequest.ackMCS = parameters.ack_mcs.value_or(-1);
        echoProbeRequest.ackNumSTS = parameters.ack_numSTS.value_or(-1);
        echoProbeRequest.ackCBW = parameters.ack_cbw ? (*parameters.ack_cbw == 40) : -1;
        echoProbeRequest.ackGI = parameters.ack_guardInterval.value_or(-1);
    }

    return echoProbeRequest;
}

void EchoProbeInitiator::printDots(int count) const {
    if (auto numOfPacketsPerDotDisplay = parameters.numOfPacketsPerDotDisplay.value_or(10)) {
        if (count == 1) {
            printf("*");
            fflush(stdout);
        }
        if (count % (numOfPacketsPerDotDisplay * 50) == 1 && count > numOfPacketsPerDotDisplay)
            printf("\n ");

        if (count % numOfPacketsPerDotDisplay == 0) {
            printf(".");
            fflush(stdout);
        }
    }
}

std::vector<double> EchoProbeInitiator::enumerateSamplingRates() {
    switch (nic->getDeviceType()) {
        case PicoScenesDeviceType::QCA9300:
        case PicoScenesDeviceType::USRP:
            return enumerateArbitrarySamplingRates();
        case PicoScenesDeviceType::IWL5300:
        default:
            return std::vector<double>{nic->getFrontEnd()->getSamplingRate()};
    }
}

std::vector<double> EchoProbeInitiator::enumerateArbitrarySamplingRates() {
    auto frequencies = std::vector<double>();
    auto sf_begin = parameters.sf_begin.value_or(nic->getFrontEnd()->getSamplingRate());
    auto sf_end = parameters.sf_end.value_or(nic->getFrontEnd()->getSamplingRate());
    auto sf_step = parameters.sf_step.value_or(5e6);
    auto cur_sf = sf_begin;

    if (sf_step == 0)
        throw std::invalid_argument("sf_step = 0");

    if (sf_end < sf_begin && sf_step > 0)
        throw std::invalid_argument("sf_step > 0, however sf_end < sf_begin.");

    if (sf_end > sf_begin && sf_step < 0)
        throw std::invalid_argument("sf_step < 0, however sf_end > sf_begin.");

    do {
        frequencies.emplace_back(cur_sf);
        cur_sf += sf_step;
    } while ((sf_step > 0 && cur_sf <= sf_end) || (sf_step < 0 && cur_sf >= sf_end));

    return frequencies;
}

std::vector<double> EchoProbeInitiator::enumerateCarrierFrequencies() {
    #ifndef _WIN32
    if (false && isIntelMVMTypeNIC(nic->getFrontEnd()->getFrontEndType())) {
        return enumerateIntelMVMCarrierFrequencies();
    }
    #endif
    return enumerateArbitraryCarrierFrequencies();
}

std::vector<double> EchoProbeInitiator::enumerateArbitraryCarrierFrequencies() {
    auto frequencies = std::vector<double>();
    auto cf_begin = parameters.cf_begin.value_or(nic->getFrontEnd()->getCarrierFrequency());
    auto cf_end = parameters.cf_end.value_or(nic->getFrontEnd()->getCarrierFrequency());
    auto cf_step = parameters.cf_step.value_or(5e6);
    auto cur_cf = cf_begin;

    if (cf_step == 0)
        throw std::invalid_argument("cf_step = 0");

    if (cf_end < cf_begin && cf_step > 0)
        throw std::invalid_argument("cf_step > 0, however cf_end < cf_begin.");

    if (cf_end > cf_begin && cf_step < 0)
        throw std::invalid_argument("cf_step < 0, however cf_end > cf_begin.");

    do {
        frequencies.emplace_back(cur_cf);
        cur_cf += cf_step;
    } while ((cf_step > 0 && cur_cf <= cf_end) || (cf_step < 0 && cur_cf >= cf_end));

    return frequencies;
}

#ifndef _WIN32
std::vector<double> EchoProbeInitiator::enumerateIntelMVMCarrierFrequencies() {
    auto cf_begin = parameters.cf_begin.value_or(nic->getFrontEnd()->getCarrierFrequency());
    auto cf_end = parameters.cf_end.value_or(nic->getFrontEnd()->getCarrierFrequency());
    auto cf_step = parameters.cf_step.value_or(20e6);

    auto frequencies = std::vector<double>();
    auto availableChannels = MAC80211FrontEndUtils::standardChannelsIn2_4_5_6GHzBandUpTo160MHzBW();
    auto uniqueFrequencies = std::set<double>();
    cf_step /= 1e6;
    for (auto i = 0; i < availableChannels.size(); i++) {
        if (std::get<1>(availableChannels[i]) == cf_step)
            uniqueFrequencies.insert((double)std::get<2>(availableChannels[i]) * 1e6);
    }
    for (auto freq: uniqueFrequencies) {
        if (freq >= cf_begin && freq <= cf_end)
            frequencies.emplace_back(freq);
    }

    return frequencies;
}
#endif

static double closest(std::vector<double> const& vec, double value) {
    if (value <= vec[0])
        return vec[0];

    if (value >= vec[vec.size() - 1])
        return vec[vec.size() - 1];


    auto const it = std::lower_bound(vec.begin(), vec.end(), value);
    if (it == vec.end()) {
        auto const it2 = std::upper_bound(vec.begin(), vec.end(), value);
        if (it2 == vec.end()) {
            return -1;
        }
        return *it2;
    }
    return *it;
}
