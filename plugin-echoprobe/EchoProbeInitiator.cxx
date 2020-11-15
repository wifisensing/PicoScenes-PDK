//
// Created by Zhiping Jiang on 10/27/17.
//

#include "EchoProbeInitiator.h"
#include "EchoProbeReplySegment.hxx"
#include "EchoProbeRequestSegment.hxx"


void EchoProbeInitiator::startJob(const EchoProbeParameters &parametersV) {
    this->parameters = parametersV;
    unifiedEchoProbeWork();
}

void EchoProbeInitiator::unifiedEchoProbeWork() {
    auto config = nic->getConfiguration();
    auto total_acked_count = 0, total_tx_count = 0;
    auto tx_count = 0, acked_count = 0;
    auto total_mean_delay = 0.0, mean_delay_single = 0.0;
    auto workingMode = parameters.workingMode;
    auto cf_repeat = parameters.cf_repeat.value_or(100);
    auto tx_delay_us = parameters.tx_delay_us;
    auto tx_delayed_start = parameters.delayed_start_seconds.value_or(0);

    auto sfList = enumerateSamplingRates();
    auto cfList = enumerateCarrierFrequencies();

    auto sessionId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
    LoggingService::info_print("EchoProbe job parameters: sf--> {} : {} : {}MHz, cf--> {} : {} : {}MHz, {}K repeats with {}us interval {}s delayed start.\n",
                               sfList.front() / 1e6, parameters.sf_step.value_or(0) / 1e6, sfList.back() / 1e6, cfList.front() / 1e6, parameters.cf_step.value_or(0) / 1e6, cfList.back() / 1e6, cf_repeat / 1e3, tx_delay_us, tx_delayed_start);

    if (tx_delayed_start > 0)
        std::this_thread::sleep_for(std::chrono::seconds(tx_delayed_start));

    for (const auto &sf_value: sfList) {
        auto dumperId = fmt::sprintf("EPI_%u_%s_bb%.1fM", sessionId, nic->getReferredInterfaceName(), sf_value / 1e6);
        for (const auto &cf_value: cfList) {
            if (workingMode == MODE_Injector) {
                if (sf_value != config->getSamplingRate()) {
                    LoggingService::info_print("EchoProbe injector shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), sf_value / 1e6);
                    config->setSamplingRate(sf_value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                }

                if (cf_value != config->getCarrierFreq()) {
                    LoggingService::info_print("EchoProbe injector shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), cf_value / 1e6);
                    config->setCarrierFreq(cf_value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                }
            } else if (workingMode == MODE_EchoProbeInitiator) {
                EchoProbeRequest echoProbeRequest;
                echoProbeRequest.sessionId = sessionId;
                bool shiftSF = false, shiftCF = false;
                if (sf_value != config->getSamplingRate()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), sf_value);
                    echoProbeRequest.sf = sf_value;
                    shiftSF = true;
                }
                if (cf_value != config->getCarrierFreq()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6);
                    echoProbeRequest.cf = cf_value;
                    shiftCF = true;
                }

                if (shiftCF || shiftSF) {
                    auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                    auto fp = buildBasicFrame(taskId, EchoProbeFreqChangeRequestFrameType, sessionId);
                    auto epSegment = std::make_shared<EchoProbeRequestSegment>(echoProbeRequest);
                    fp->addSegment(epSegment);
                    auto currentCF = config->getCarrierFreq();
                    auto currentSF = config->getSamplingRate();
                    auto nextCF = cf_value;
                    auto nextSF = sf_value;
                    auto connectionEstablished = false;
                    for (auto i = 0; i < parameters.tx_max_retry; i++) {
                        if (auto[rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(fp, 1); rxframe) {
                            LoggingService::info_print("EchoProbe responder confirms the channel changes.\n");
                            if (shiftSF) config->setSamplingRate(nextSF);
                            if (shiftCF) config->setCarrierFreq(nextCF);
                            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                            connectionEstablished = true;
                            break;
                        } else {
                            if (shiftSF) config->setSamplingRate(nextSF);
                            if (shiftCF) config->setCarrierFreq(nextCF);
                            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                            if (auto[rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(fp, 1); rxframe) {
                                LoggingService::info_print("EchoProbe responder confirms the channel changes.\n");
                                if (shiftSF) config->setSamplingRate(nextSF);
                                if (shiftCF) config->setCarrierFreq(nextCF);
                                std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                                connectionEstablished = true;
                                break;
                            } else {
                                if (shiftSF) config->setSamplingRate(currentSF);
                                if (shiftCF) config->setCarrierFreq(currentCF);
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
                auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                std::shared_ptr<PicoScenesFrameBuilder> fp = nullptr;

                if (workingMode == MODE_Injector) {
                    fp = buildBasicFrame(taskId, SimpleInjectionFrameType, sessionId);
                    fp->transmitSync();
                    tx_count++;
                    total_tx_count++;
                    if (LoggingService::localDisplayLevel == Trace)
                        printDots(tx_count);
                    std::this_thread::sleep_for(std::chrono::microseconds(parameters.tx_delay_us));
                } else if (workingMode == MODE_EchoProbeInitiator) {
                    fp = buildBasicFrame(taskId, EchoProbeRequestFrameType, sessionId);

                    auto[rxframe, ackframe, retryPerTx, rtDelay] = this->transmitAndSyncRxUnified(fp);
                    tx_count += retryPerTx;
                    total_tx_count += retryPerTx;
                    if (rxframe && ackframe) {
                        acked_count++;
                        total_acked_count++;
                        mean_delay_single += rtDelay / cf_repeat;
                        total_mean_delay += rtDelay / cf_repeat / cfList.size() / sfList.size();
                        RXSDumper::getInstance(dumperId).dumpRXS(&rxframe->rawBuffer[0], rxframe->rawBuffer.size());
                        LoggingService::detail_print("TaskId {} done!\n", int(rxframe->PicoScenesHeader->taskId));
                        if (LoggingService::localDisplayLevel == Trace)
                            printDots(acked_count);
                        std::this_thread::sleep_for(std::chrono::microseconds(parameters.tx_delay_us));
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
    auto taskId = frameBuilder->getFrame()->frameHeader.taskId;
    auto retryCount = 0;
    auto timeGap = -1.0;
    maxRetry = (maxRetry ? *maxRetry : parameters.tx_max_retry);

    while (retryCount++ < *maxRetry) {
        frameBuilder->getFrame()->frameHeader.txId = uniformRandomNumberWithinRange<uint16_t>(100, UINT16_MAX);
        auto tx_time = std::chrono::system_clock::now();
        frameBuilder->transmit();
        /*
        * Tx-Rx time grows non-li nearly in low sampling rate cases, enlarge the timeout to 11x.
        */
        auto timeout_us_scaling = nic->getConfiguration()->getSamplingRate() < 20e6 ? 6 : 1;
        auto totalTimeOut = timeout_us_scaling * *parameters.timeout_ms;
        if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
            totalTimeOut = 6;
        }
        if (nic->getDeviceType() == PicoScenesDeviceType::USRP)
            totalTimeOut += 20;
        if (!responderDeviceType || responderDeviceType == PicoScenesDeviceType::USRP)
            totalTimeOut += 100;
        auto replyFrame = nic->syncRxConditionally([=](const ModularPicoScenesRxFrame &rxframe) -> bool {
            return rxframe.PicoScenesHeader && (rxframe.PicoScenesHeader->frameType == EchoProbeReplyFrameType || rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeACKFrameType) && rxframe.PicoScenesHeader->taskId == taskId && rxframe.txUnknownSegmentMap.contains("EchoProbeReply");
        }, std::chrono::milliseconds(totalTimeOut), "taskId[" + std::to_string(taskId) + "]");

        if (replyFrame && replyFrame->PicoScenesHeader->frameType == EchoProbeReplyFrameType) {
            auto delayDuration = std::chrono::system_clock::now() - tx_time;
            timeGap = std::chrono::duration_cast<std::chrono::microseconds>(delayDuration).count() / 1000.0;
            responderDeviceType = (PicoScenesDeviceType) replyFrame->PicoScenesHeader->deviceType;
            const auto &replySegBytes = replyFrame->txUnknownSegmentMap.at("EchoProbeReply");
            EchoProbeReplySegment replySeg;
            replySeg.fromBuffer(&replySegBytes[0], replySegBytes.size());
            if (replySeg.echoProbeReply.replyStrategy == EchoProbeReplyStrategy::ReplyOnlyHeader || replySeg.echoProbeReply.replyStrategy == EchoProbeReplyStrategy::ReplyWithExtraInfo) {
                if (LoggingService::localDisplayLevel <= Debug) {
                    LoggingService::debug_printf("Round-trip delay %.3fms, only header", timeGap);
                }
                return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
            }

            if (replySeg.echoProbeReply.replyStrategy == EchoProbeReplyStrategy::ReplyWithCSI && replyFrame->txCSISegment) {
                LoggingService::debug_printf("Round-trip delay %.3fms, only CSI", timeGap);
                return std::make_tuple(replyFrame, replyFrame, retryCount, timeGap);
            }

            if (replySeg.echoProbeReply.replyStrategy == EchoProbeReplyStrategy::ReplyWithFullPayload) {
                if (auto ackFrame = ModularPicoScenesRxFrame::fromBuffer(&replySeg.echoProbeReply.replyBuffer[0], replySeg.echoProbeReply.replyBuffer.size())) {
                    replyFrame->txCSISegment = ackFrame->csiSegment;
                    if (LoggingService::localDisplayLevel <= Debug) {
                        LoggingService::debug_print("Raw ACK: {}\n", *replyFrame);
                        LoggingService::debug_print("ACKed Tx: {}\n", *ackFrame);
                        LoggingService::debug_printf("Round-trip delay %.3fms, full payload", timeGap);
                    }
                    return std::make_tuple(replyFrame, ackFrame, retryCount, timeGap);
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
    fp->makeFrame_HeaderOnly();
    fp->setTaskId(taskId);
    fp->setPicoScenesFrameType(frameType);

    if (frameType == SimpleInjectionFrameType) {
        fp->addExtraInfo();
    }

    if (frameType == EchoProbeRequestFrameType) {
        fp->addExtraInfo();
        EchoProbeRequest echoProbeRequest;
        echoProbeRequest.sessionId = sessionId;
        echoProbeRequest.replyStrategy = parameters.replyStrategy;
        echoProbeRequest.ackMCS = parameters.ack_mcs.value_or(-1);
        echoProbeRequest.ackNumSTS = parameters.ack_numSTS.value_or(-1);
        echoProbeRequest.ackCBW = parameters.ack_cbw ? (*parameters.ack_cbw == 40) : -1;
        echoProbeRequest.ackGI = parameters.ack_gi.value_or(-1);
        if (!responderDeviceType)
            echoProbeRequest.deviceProbingStage = true;
        fp->addSegment(std::make_shared<EchoProbeRequestSegment>(echoProbeRequest));
    }

    if (frameType == EchoProbeFreqChangeRequestFrameType) {
        EchoProbeRequest echoProbeRequest;
        echoProbeRequest.sessionId = sessionId;
        echoProbeRequest.replyStrategy = EchoProbeReplyStrategy::ReplyOnlyHeader;
        echoProbeRequest.repeat = 5;
        echoProbeRequest.ackMCS = 0;
        echoProbeRequest.ackNumSTS = 1;
        echoProbeRequest.ackCBW = -1;
        echoProbeRequest.ackGI = int16_t(GuardIntervalEnum::GI_800);
        if (!responderDeviceType)
            echoProbeRequest.deviceProbingStage = true;
        fp->addSegment(std::make_shared<EchoProbeRequestSegment>(echoProbeRequest));
    }

    fp->setMCS(parameters.mcs.value_or(0));
    fp->setNumSTS(parameters.numSTS.value_or(1));
    fp->setChannelBandwidth(ChannelBandwidthEnum(parameters.cbw.value_or(20)));
    fp->setGuardInterval(GuardIntervalEnum(parameters.gi.value_or(800)));
    fp->setNumberOfExtraSounding(parameters.ness.value_or(0));
    fp->setChannelCoding((ChannelCodingEnum) parameters.coding.value_or((uint32_t) ChannelCodingEnum::BCC));

    fp->setDestinationAddress(parameters.inj_target_mac_address->data());
    if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
        auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
        fp->setSourceAddress(picoScenesNIC->getMacAddressPhy().data());
        fp->set3rdAddress(picoScenesNIC->getMacAddressDev().data());
        if (parameters.inj_for_intel5300.value_or(false)) {
            fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            fp->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            fp->set3rdAddress(picoScenesNIC->getMacAddressPhy().data());
            fp->setForceSounding(false);
        }
    } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
        fp->setSourceAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
        fp->set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
        if (parameters.inj_for_intel5300.value_or(false)) {
            fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            fp->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            fp->set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
            fp->setForceSounding(false);
            fp->setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
        }
    } else if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
        auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
        fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        fp->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        fp->set3rdAddress(picoScenesNIC->getMacAddressPhy().data());
    }

    return fp;
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
            return std::vector<double>{nic->getConfiguration()->getSamplingRate()};
    }
}

std::vector<double> EchoProbeInitiator::enumerateArbitrarySamplingRates() {
    auto frequencies = std::vector<double>();
    auto sf_begin = parameters.sf_begin.value_or(nic->getConfiguration()->getSamplingRate());
    auto sf_end = parameters.sf_end.value_or(nic->getConfiguration()->getSamplingRate());
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
    return nic->getDeviceType() == PicoScenesDeviceType::IWL5300 ? enumerateIntelCarrierFrequencies() : enumerateArbitraryCarrierFrequencies();
}


std::vector<double> EchoProbeInitiator::enumerateArbitraryCarrierFrequencies() {
    auto frequencies = std::vector<double>();
    auto cf_begin = parameters.cf_begin.value_or(nic->getConfiguration()->getCarrierFreq());
    auto cf_end = parameters.cf_end.value_or(nic->getConfiguration()->getCarrierFreq());
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

static int closest(std::vector<int> const &vec, int value) {
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

std::vector<double> EchoProbeInitiator::enumerateIntelCarrierFrequencies() {
    auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
    auto frequencies = std::vector<double>();
    auto cf_begin = parameters.cf_begin.value_or(nic->getConfiguration()->getCarrierFreq());
    auto cf_end = parameters.cf_end.value_or(nic->getConfiguration()->getCarrierFreq());
    auto cf_step = parameters.cf_step.value_or(5e6);

    if (int(std::abs(cf_step)) % 5000000 != 0)
        throw std::invalid_argument("cf_step must be the multiply of 5MHz for Intel 5300AGN.");

    if (std::abs(cf_step) == 0)
        throw std::invalid_argument("cf_step must NOT be 0 for Intel 5300AGN.");

    if (cf_end < cf_begin && cf_step > 0)
        throw std::invalid_argument("cf_step > 0, however cf_end < cf_begin.\n");

    if (cf_end > cf_begin && cf_step < 0)
        throw std::invalid_argument("cf_step < 0, however cf_end > cf_begin.\n");

    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_PLUS)
        cf_begin -= 10e6;
    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_MINUS)
        cf_begin += 10e6;
    auto closestFreq = closest(picoScenesNIC->getConfiguration()->getSystemSupportedFrequencies(), int(cf_begin / 1e6));
    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_PLUS) {
        closestFreq += 10;
        cf_begin += 10e6;
    }
    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_MINUS) {
        closestFreq -= 10;
        cf_begin -= 10e6;
    }
    if (cf_begin / 1e6 != closestFreq) {
        LoggingService::warning_print("CF begin (desired {}) is forced to be {}MHz for Intel 5300 NIC.\n", cf_begin, closestFreq);
        cf_begin = (int64_t) closestFreq * 1e6;
    }
    auto cur_cf = cf_begin;

    closestFreq = closest(picoScenesNIC->getConfiguration()->getSystemSupportedFrequencies(), int(cf_end / 1e6));
    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_PLUS)
        closestFreq += 10;
    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_MINUS)
        closestFreq -= 10;
    if (cf_end / 1e6 != closestFreq) {
        LoggingService::warning_print("CF end (desired {}) is forced to be {}MHz for Intel 5300 NIC.\n", *parameters.cf_end, closestFreq);
        cf_end = (int64_t) closestFreq * 1e6;
    }

    do {
        frequencies.emplace_back(cur_cf);
        auto previous_closest = cur_cf / 1e6;
        do {
            cur_cf += cf_step;
            if ((cur_cf > 5825e6 && cf_step > 0) || (cur_cf < 2412e6 && cf_step < 0))
                break;
            closestFreq = closest(picoScenesNIC->getConfiguration()->getSystemSupportedFrequencies(), int(cur_cf / 1e6));
            if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_PLUS)
                closestFreq += 10;
            if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT40_MINUS)
                closestFreq -= 10;
        } while (closestFreq == previous_closest);
        cur_cf = closestFreq * 1e6;
        if (closestFreq == previous_closest)
            break;
    } while ((cf_step > 0 && cur_cf <= cf_end) || (cf_step < 0 && cur_cf >= cf_end));

    return frequencies;
}