//
// Created by Zhiping Jiang on 10/27/17.
//

#include <PicoScenes/SystemTools.hxx>
#include <PicoScenes/MAC80211CSIExtractableNIC.hxx>
#include "EchoProbeInitiator.hxx"
#include "EchoProbeReplySegment.hxx"
#include "EchoProbeRequestSegment.hxx"


void EchoProbeInitiator::startJob(const EchoProbeParameters &parametersV) {
    this->parameters = parametersV;
    unifiedEchoProbeWork();
}

void EchoProbeInitiator::unifiedEchoProbeWork() {
    auto frontEnd = nic->getFrontEnd();
    auto total_acked_count = 0, total_tx_count = 0;
    auto tx_count = 0, acked_count = 0;
    auto total_mean_delay = 0.0, mean_delay_single = 0.0;
    auto workingMode = parameters.workingMode;
    auto cf_repeat = parameters.cf_repeat.value_or(100);
    auto tx_delay_us = parameters.tx_delay_us;
    auto tx_delayed_start = parameters.delayed_start_seconds.value_or(0);

    auto sfList = enumerateSamplingRates();
    auto cfList = enumerateCarrierFrequencies();

    auto sessionId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
    LoggingService::info_print("EchoProbe job parameters: sf--> {} : {} : {}MHz, cf--> {} : {} : {}MHz, {}K repeats with {}us interval {}s delayed start.\n",
                               sfList.front() / 1e6, parameters.sf_step.value_or(0) / 1e6, sfList.back() / 1e6, cfList.front() / 1e6, parameters.cf_step.value_or(0) / 1e6, cfList.back() / 1e6, cf_repeat / 1e3, tx_delay_us, tx_delayed_start);

    if (tx_delayed_start > 0)
        std::this_thread::sleep_for(std::chrono::seconds(tx_delayed_start));

    for (const auto &sf_value: sfList) {
        auto dumperId = fmt::sprintf("EPI_%s_%u_bb%.1fM", nic->getReferredInterfaceName(), sessionId, sf_value / 1e6);
        if (parameters.outputFileName)
            dumperId = *parameters.outputFileName;
        for (const auto &cf_value: cfList) {
            if (workingMode == MODE_Injector) {
                if (sf_value != frontEnd->getSamplingRate()) {
                    LoggingService::info_print("EchoProbe injector shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), sf_value / 1e6);
                    frontEnd->setSamplingRate(sf_value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                }

                if (cf_value != frontEnd->getCarrierFrequency()) {
                    LoggingService::info_print("EchoProbe injector shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), cf_value / 1e6);
                    frontEnd->setCarrierFrequency(cf_value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                }
            } else if (workingMode == MODE_EchoProbeInitiator) {
                bool shiftSF = false, shiftCF = false;
                if (sf_value != frontEnd->getSamplingRate()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), sf_value);
                    shiftSF = true;
                }
                if (cf_value != frontEnd->getCarrierFrequency()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6);
                    shiftCF = true;
                }

                if (shiftCF || shiftSF) {
                    auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                    auto fp = buildBasicFrame(taskId, EchoProbeFreqChangeRequestFrameType, sessionId);
                    fp->addSegment(std::make_shared<EchoProbeRequestSegment>(makeRequestSegment(sessionId, shiftCF ? std::optional<double>(cf_value) : std::nullopt, shiftSF ? std::optional<double>(sf_value) : std::nullopt)));
                    auto currentCF = frontEnd->getCarrierFrequency();
                    auto currentSF = frontEnd->getSamplingRate();
                    auto nextCF = cf_value;
                    auto nextSF = sf_value;
                    auto connectionEstablished = false;
                    for (auto i = 0; i < parameters.tx_max_retry; i++) {
                        if (auto [rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(fp, 1); rxframe) {
                            LoggingService::info_print("EchoProbe responder confirms the channel changes.\n");
                            if (shiftSF) frontEnd->setSamplingRate(nextSF);
                            if (shiftCF) frontEnd->setCarrierFrequency(nextCF);
                            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                            connectionEstablished = true;
                            break;
                        } else {
                            if (shiftSF) frontEnd->setSamplingRate(nextSF);
                            if (shiftCF) frontEnd->setCarrierFrequency(nextCF);
                            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                            if (auto [rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(fp, 1); rxframe) {
                                LoggingService::info_print("EchoProbe responder confirms the channel changes.\n");
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
            for (uint32_t i = 0; i < cf_repeat; ++i) {
                auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                std::shared_ptr<PicoScenesFrameBuilder> fp = nullptr;

                if (workingMode == MODE_Injector) {
                    fp = buildBasicFrame(taskId, SimpleInjectionFrameType, sessionId);
                    fp->transmitSync();
                    tx_count++;
                    total_tx_count++;
                    if (LoggingService::localDisplayLevel == Trace)
                        printDots(tx_count);
                    SystemTools::Time::delay_periodic(parameters.tx_delay_us);
                } else if (workingMode == MODE_EchoProbeInitiator) {
                    fp = buildBasicFrame(taskId, EchoProbeRequestFrameType, sessionId);
                    fp->addSegment(std::make_shared<EchoProbeRequestSegment>(makeRequestSegment(sessionId)));
                    auto [rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(fp);
                    tx_count += retryPerTx;
                    total_tx_count += retryPerTx;
                    if (rxframe && ackframe) {
                        acked_count++;
                        total_acked_count++;
                        mean_delay_single += rtDelay / cf_repeat;
                        total_mean_delay += rtDelay / cf_repeat / cfList.size() / sfList.size();
                        auto frameBuffer = rxframe->toBuffer();
                        RXSDumper::getInstance(dumperId).dumpRXS(frameBuffer.data(), frameBuffer.size());
                        LoggingService::detail_print("TaskId {} done!\n", int(rxframe->PicoScenesHeader->taskId));
                        if (LoggingService::localDisplayLevel == Trace)
                            printDots(acked_count);
                        SystemTools::Time::delay_periodic(parameters.tx_delay_us);
                    } else {
                        if (LoggingService::localDisplayLevel == Trace)
                            printf("\n");
                        LoggingService::warning_print("EchoProbe Job Warning: max retry times reached during measurement @ {}Hz...\n", cf_value);
                        goto failed;
                    }
                }
            }
            if (LoggingService::localDisplayLevel == Trace)
                printf("\n");
            if (workingMode == MODE_Injector)
                LoggingService::info_printf("EchoProbe injector %s @ cf=%.3fMHz, sf=%.3fMHz, #.tx=%d.\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6, (double) sf_value / 1e6, tx_count);
            else if (workingMode == MODE_EchoProbeInitiator)
                LoggingService::info_printf("EchoProbe initiator %s @ cf=%.3fMHz, sf=%.3fMHz, #.tx=%d, #.acked=%d, echo_delay=%.1fms, success_rate=%.1f%%.\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6, (double) sf_value / 1e6, tx_count, acked_count, mean_delay_single, double(100.0 * acked_count / tx_count));
        }

        RXSDumper::getInstance(dumperId).finishCurrentSession();
        continue;

        failed:
        RXSDumper::getInstance(dumperId).finishCurrentSession();
        break;
    }

    if (LoggingService::localDisplayLevel == Trace) {
        if (workingMode == MODE_Injector)
            LoggingService::info_printf("Job done! #.total_tx=%d.\n", total_tx_count);
        else if (workingMode == MODE_EchoProbeInitiator)
            LoggingService::trace_printf("Job done! #.total_tx=%d #.total_acked=%d, echo_delay=%.1fms, success_rate =%.1f%%.\n", total_tx_count, total_acked_count, total_mean_delay, 100.0 * total_acked_count / total_tx_count);
    }
}

std::tuple<std::optional<ModularPicoScenesRxFrame>, std::optional<ModularPicoScenesRxFrame>, int, double> EchoProbeInitiator::transmitAndSyncRxUnified(const std::shared_ptr<PicoScenesFrameBuilder> &frameBuilder, std::optional<uint32_t> maxRetry) {
    auto taskId = frameBuilder->getFrame()->frameHeader->taskId;
    auto retryCount = 0;
    auto timeGap = -1.0;
    maxRetry = (maxRetry ? *maxRetry : parameters.tx_max_retry);

    while (retryCount++ < *maxRetry) {
        frameBuilder->getFrame()->frameHeader->txId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(100, UINT16_MAX);
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

        auto tx_time = std::chrono::system_clock::now();
        frameBuilder->transmit();
        auto replyFrame = nic->syncRxConditionally([=](const ModularPicoScenesRxFrame &rxframe) -> bool {
            return rxframe.PicoScenesHeader && (rxframe.PicoScenesHeader->frameType == EchoProbeReplyFrameType || rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeACKFrameType) && rxframe.PicoScenesHeader->taskId == taskId;
        }, std::chrono::milliseconds(totalTimeOut), "taskId[" + std::to_string(taskId) + "]");

        if (replyFrame && replyFrame->PicoScenesHeader->frameType == EchoProbeReplyFrameType) {
            auto delayDuration = std::chrono::system_clock::now() - tx_time;
            timeGap = double(std::chrono::duration_cast<std::chrono::microseconds>(delayDuration).count()) / 1000.0;
            responderDeviceType = (PicoScenesDeviceType) replyFrame->PicoScenesHeader->deviceType;
            const auto echoProbeReplySegment = replyFrame->txUnknownSegments.at("EchoProbeReply");
            EchoProbeReplySegment replySeg(echoProbeReplySegment.rawBuffer.data(), echoProbeReplySegment.rawBuffer.size());
            if (replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyOnlyHeader || replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyWithExtraInfo) {
                if (LoggingService::localDisplayLevel <= Debug) {
                    LoggingService::debug_printf("Round-trip delay %.3fms, only header", timeGap);
                }
                return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
            }

            if (replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyWithCSI) {
                const auto payloadName = replySeg.getEchoProbeReply().payloadName;
                auto foundIt = std::find_if(replyFrame->payloadSegments.cbegin(), replyFrame->payloadSegments.cend(), [payloadName](const PayloadSegment &payloadSegment) {
                    return payloadSegment.getPayloadData().payloadDescription == payloadName;
                });
                if (foundIt != replyFrame->payloadSegments.cend()) {
                    LoggingService::debug_printf("Round-trip delay %.3fms, only CSI", timeGap);
                    return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
                }
            }

            if (replySeg.getEchoProbeReply().replyStrategy == EchoProbeReplyStrategy::ReplyWithFullPayload) {
                const auto payloadName = replySeg.getEchoProbeReply().payloadName;
                auto foundIt = std::find_if(replyFrame->payloadSegments.cbegin(), replyFrame->payloadSegments.cend(), [payloadName](const PayloadSegment &payloadSegment) {
                    return payloadSegment.getPayloadData().payloadDescription == payloadName;
                });
                if (foundIt != replyFrame->payloadSegments.cend()) {
                    if (auto ackFrame = ModularPicoScenesRxFrame::fromBuffer(foundIt->getPayloadData().payloadData.data(), foundIt->getPayloadData().payloadData.size())) {
                        if (LoggingService::localDisplayLevel <= Debug) {
                            LoggingService::debug_print("Raw ACK: {}\n", *replyFrame);
                            LoggingService::debug_print("ACKed Tx: {}\n", *ackFrame);
                            LoggingService::debug_printf("Round-trip delay %.3fms, full payload", timeGap);
                        }
                        return std::make_tuple(replyFrame, ackFrame, retryCount, timeGap);
                    }
                }
            }
        }

        if (replyFrame && replyFrame->PicoScenesHeader->frameType == EchoProbeFreqChangeACKFrameType) {
            return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
        }
    }

    return std::make_tuple(std::nullopt, std::nullopt, 0, 0);
}

std::shared_ptr<PicoScenesFrameBuilder> EchoProbeInitiator::buildBasicFrame(uint16_t taskId, const EchoProbePacketFrameType &frameType, uint16_t sessionId) const {
    auto fp = std::make_shared<PicoScenesFrameBuilder>(nic);

    if (frameType == SimpleInjectionFrameType && parameters.injectorContent == EchoProbeInjectionContent::NDP) {
        fp->makeFrame_NDP();
        /**
         * @brief PicoScenes Platform CLI parser has *absorbed* the common Tx parameters.
         * The platform parser will parse the Tx parameters options and store the results in AbstractNIC.
         * Plugin developers now can access the parameters via a new method nic->getUserSpecifiedTxParameters().
         */
        fp->setTxParameters(nic->getUserSpecifiedTxParameters());
        return fp;
    } else {
        fp->makeFrame_HeaderOnly();
        /**
         * @brief PicoScenes Platform CLI parser has *absorbed* the common Tx parameters.
         * The platform parser will parse the Tx parameters options and store the results in AbstractNIC.
         * Plugin developers now can access the parameters via a new method nic->getUserSpecifiedTxParameters().
         */
        fp->setTxParameters(nic->getUserSpecifiedTxParameters());
        fp->setTaskId(taskId);
        fp->setPicoScenesFrameType(frameType);

        if (frameType == SimpleInjectionFrameType && parameters.injectorContent == EchoProbeInjectionContent::Full) {
            fp->addExtraInfo();
        }

        if (frameType == EchoProbeRequestFrameType)
            fp->addExtraInfo();
    }

    auto sourceAddr = PicoScenesFrameBuilder::magicIntel123456;
    if (parameters.randomMAC) {
        sourceAddr[0] = SystemTools::Math::uniformRandomNumberWithinRange<uint8_t>(0, UINT8_MAX);
        sourceAddr[1] = SystemTools::Math::uniformRandomNumberWithinRange<uint8_t>(0, UINT8_MAX);
    }
    fp->setSourceAddress(sourceAddr.data());
    fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
    fp->set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    if (parameters.inj_for_intel5300.value_or(false)) {
        fp->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        fp->set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());
        fp->setForceSounding(false);
        fp->setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
    }

    return fp;
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
        throw std::invalid_argument("sf_step > 0, however sf_end < sf_begin.\n");

    if (sf_end > sf_begin && sf_step < 0)
        throw std::invalid_argument("sf_step < 0, however sf_end > sf_begin.\n");

    do {
        frequencies.emplace_back(cur_sf);
        cur_sf += sf_step;
    } while ((sf_step > 0 && cur_sf <= sf_end) || (sf_step < 0 && cur_sf >= sf_end));

    return frequencies;
}

std::vector<double> EchoProbeInitiator::enumerateCarrierFrequencies() {
    if (false && isIntelMVMTypeNIC(nic->getFrontEnd()->getFrontEndType())) {
        return enumerateIntelMVMCarrierFrequencies();
    }
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
        throw std::invalid_argument("cf_step > 0, however cf_end < cf_begin.\n");

    if (cf_end > cf_begin && cf_step < 0)
        throw std::invalid_argument("cf_step < 0, however cf_end > cf_begin.\n");

    do {
        frequencies.emplace_back(cur_cf);
        cur_cf += cf_step;
    } while ((cf_step > 0 && cur_cf <= cf_end) || (cf_step < 0 && cur_cf >= cf_end));

    return frequencies;
}

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
            uniqueFrequencies.insert((double) std::get<2>(availableChannels[i]) * 1e6);
    }
    for (auto freq: uniqueFrequencies) {
        if (freq >= cf_begin && freq <= cf_end)
            frequencies.emplace_back(freq);
    }

    return frequencies;
}

static double closest(std::vector<double> const &vec, double value) {
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
