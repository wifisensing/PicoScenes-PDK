//
// Created by Zhiping Jiang on 10/27/17.
//

#include "EchoProbeInitiator.h"


void EchoProbeInitiator::startJob(const EchoProbeParameters &parameters) {
    this->parameters = parameters;
    unifiedEchoProbeWork();
}

void EchoProbeInitiator::unifiedEchoProbeWork() {
    auto config = nic->getConfiguration();
    auto total_acked_count = 0, total_tx_count = 0;
    auto tx_count = 0, acked_count = 0;
    auto workingMode = parameters.workingMode;
    auto cf_repeat = parameters.cf_repeat.value_or(100);
    auto tx_delay_us = parameters.tx_delay_us;

    auto sfList = enumerateSamplingRates();
    auto cfList = enumerateCarrierFrequencies();

    LoggingService::info_print("EchoProbe job parameters: sf--> {} : {} : {}MHz, cf--> {} : {} : {}MHz, {}K repeats with {}us interval.\n",
                               sfList.front() / 1e6, parameters.sf_step.value_or(0) / 1e6, sfList.back() / 1e6, cfList.front() / 1e6, parameters.cf_step.value_or(0) / 1e6, cfList.back() / 1e6, cf_repeat / 1e3, tx_delay_us);

    for (const auto &sf_value: sfList) {
        auto dumperId = fmt::sprintf("rxack_%s_bb%.1fM", nic->getReferredInterfaceName(), sf_value);
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
                EchoProbeHeader epHeader{};
                bool shiftSF = false, shiftCF = false;
                if (sf_value != config->getSamplingRate()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), sf_value);
                    epHeader.sf = sf_value;
                    shiftSF = true;
                }
                if (cf_value != config->getCarrierFreq()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6);
                    epHeader.cf = cf_value;
                    shiftCF = true;
                }

                if (shiftCF || shiftSF) {
                    auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                    auto fp = buildBasicFrame(taskId, EchoProbeFreqChangeRequest);
                    fp->addSegment("EP", (uint8_t *) (&epHeader), sizeof(EchoProbeHeader));
                    if (auto[rxframe, ackframe, retryPerTx] = this->transmitAndSyncRxUnified(fp); rxframe) {
                        LoggingService::info_print("EchoProbe responder confirms the channel changes.\n");
                        if (shiftSF) config->setSamplingRate(sf_value);
                        if (shiftCF) config->setCarrierFreq(cf_value);
                        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                    } else {
                        LoggingService::warning_print("EchoProbe initiator shifting {} to next rate combination to recover the connection.\n", nic->getReferredInterfaceName());
                        if (shiftSF) config->setSamplingRate(sf_value);
                        if (shiftCF) config->setCarrierFreq(cf_value);
                        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                        if (auto[rxframe, ackframe, retryPerTx] = this->transmitAndSyncRxUnified(fp); !rxframe) { // still fails
                            LoggingService::warning_print("Job fails! EchoProbe initiator loses connection to the responder.\n");
                            goto failed;
                        }
                    }
                }
            }

            tx_count = 0;
            acked_count = 0;
            for (uint32_t i = 0; i < cf_repeat; ++i) {
                auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                std::shared_ptr<PicoScenesFrameBuilder> fp = nullptr;
                std::shared_ptr<PicoScenesRxFrameStructure> replyRXS = nullptr;

                if (workingMode == MODE_Injector) {
                    fp = buildBasicFrame(taskId, SimpleInjection);
                    fp->transmitSync();
                    tx_count++;
                    total_tx_count++;
                    if (LoggingService::localDisplayLevel == Trace)
                        printDots(tx_count);
                    std::this_thread::sleep_for(std::chrono::microseconds(parameters.tx_delay_us));
                } else if (workingMode == MODE_EchoProbeInitiator) {
                    fp = buildBasicFrame(taskId, EchoProbeRequest);
                    auto[rxframe, ackframe, retryPerTx] = this->transmitAndSyncRxUnified(fp);
                    tx_count += retryPerTx;
                    total_tx_count += retryPerTx;
                    if (rxframe && ackframe) {
                        acked_count++;
                        total_acked_count++;
                        RXSDumper::getInstance(dumperId).dumpRXS(rxframe->rawBuffer.get(), rxframe->rawBufferLength);
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
                LoggingService::info_print("EchoProbe injector {} @ cf={}MHz, sf={}MHz, #.tx = {}.\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6, (double) sf_value / 1e6, tx_count);
            else if (workingMode == MODE_EchoProbeInitiator)
                LoggingService::info_print("EchoProbe initiator {} @ cf={}MHz, sf={}MHz, #.tx = {}, #.acked = {}, success rate = {}\%.\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6, (double) sf_value / 1e6, tx_count, acked_count, 100.0 * acked_count / tx_count);
        }

        RXSDumper::getInstance(dumperId).finishCurrentSession();
        continue;

        failed:
        RXSDumper::getInstance(dumperId).finishCurrentSession();
        break;
    }

    if (LoggingService::localDisplayLevel == Trace) {
        if (workingMode == MODE_Injector)
            LoggingService::info_print("Job done! #.total_tx = {}.\n", total_tx_count);
        else if (workingMode == MODE_EchoProbeInitiator)
            LoggingService::trace_print("Job done! #.total_tx = {}, #.total_acked = {}, success rate = {}\%.\n", total_tx_count, total_acked_count, 100.0 * total_acked_count / total_tx_count);
    }
}

std::tuple<std::optional<PicoScenesRxFrameStructure>, std::optional<PicoScenesRxFrameStructure>, int> EchoProbeInitiator::transmitAndSyncRxUnified(const std::shared_ptr<PicoScenesFrameBuilder> &frameBuilder, std::optional<uint32_t> maxRetry) {
    auto taskId = frameBuilder->getFrame()->frameHeader.taskId;
    auto retryCount = 0;
    maxRetry = (maxRetry ? *maxRetry : parameters.tx_max_retry);

    while (retryCount++ < *maxRetry) {
        frameBuilder->transmit();
        /*
        * Tx-Rx time grows non-linearly in low sampling rate cases, enlarge the timeout to 11x.
        */
        auto timeout_us_scaling = nic->getConfiguration()->getSamplingRate() < 20e6 ? 6 : 1;
        auto totalTimeOut = timeout_us_scaling * *parameters.timeout_ms;
        if (!responderDeviceType || responderDeviceType == PicoScenesDeviceType::USRP)
            totalTimeOut = 200;
        auto replyFrame = nic->syncRxConditionally([=](const PicoScenesRxFrameStructure &rxframe) -> bool {
            return rxframe.PicoScenesHeader && (rxframe.PicoScenesHeader->frameType == EchoProbeReply || rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeACK) && rxframe.PicoScenesHeader->taskId == taskId;
        }, std::chrono::milliseconds(totalTimeOut), "taskId[" + std::to_string(taskId) + "]");
        if (replyFrame) {
            if (replyFrame->PicoScenesHeader && replyFrame->PicoScenesHeader->frameType == EchoProbeReply) {
                auto rxDeviceType = replyFrame->PicoScenesHeader->deviceType;
                responderDeviceType = rxDeviceType;
                auto segment = replyFrame->segmentMap->at("EP");
                if (auto ackFrame = PicoScenesRxFrameStructure::fromBuffer(segment.second.get(), segment.first)) {
                    if (LoggingService::localDisplayLevel <= Debug) {
                        LoggingService::debug_print("Raw ACK: {}\n", *replyFrame);
                        LoggingService::debug_print("ACKed Tx: {}\n", *ackFrame);
                    }
                    return std::make_tuple(replyFrame, ackFrame, retryCount);
                } else
                    LoggingService::debug_print("Corrupted EchoProbe ACK frame.\n");
            } else if (replyFrame->PicoScenesHeader && replyFrame->PicoScenesHeader->frameType == EchoProbeFreqChangeACK) {
                return std::make_tuple(replyFrame, std::nullopt, retryCount);
            }
        }
    }

    return std::make_tuple(std::nullopt, std::nullopt, 0);
}

std::shared_ptr<PicoScenesFrameBuilder> EchoProbeInitiator::buildBasicFrame(uint16_t taskId, const EchoProbePacketFrameType &frameType) const {
    auto fp = std::make_shared<PicoScenesFrameBuilder>(nic);
    fp->makeFrame_HeaderOnly();
    fp->setTaskId(taskId);
    fp->setPicoScenesFrameType(frameType);
    fp->setDestinationAddress(parameters.inj_target_mac_address->data());
    if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
        auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
        fp->setSourceAddress(picoScenesNIC->getMacAddressPhy().data());
        fp->set3rdAddress(picoScenesNIC->getMacAddressDev().data());
    } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
        fp->setSourceAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
        fp->set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
    } else if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
        // TODO add 5300 things
    }
    fp->setMCS(parameters.mcs.value_or(0));
    fp->setGreenField(parameters.inj_5300_gf.value_or(false));
    fp->setChannelBonding(parameters.bw.value_or(20) == 40);
    fp->setSGI(parameters.sgi.value_or(false));
    if (parameters.randomPayloadLength) {
        std::shared_ptr<uint8_t> randomPayload = std::shared_ptr<uint8_t>(new uint8_t[*parameters.randomPayloadLength], std::default_delete<uint8_t[]>());
        fp->addSegment("PL", randomPayload.get(), *parameters.randomPayloadLength);
    }

    if (frameType == SimpleInjection) {
        fp->addExtraInfo();
    }

    if (frameType == EchoProbeRequest) {
        fp->addExtraInfo();
        EchoProbeHeader epHeader;
        epHeader.ackMCS = parameters.ack_mcs.value_or(-1);
        epHeader.ackChannelBonding = parameters.ack_bw ? (*parameters.ack_bw == 40) : -1;
        epHeader.ackSGI = parameters.ack_sgi.value_or(-1);
        if (!responderDeviceType)
            epHeader.deviceProbingStage = 1;
        fp->addSegment("EP", reinterpret_cast<const uint8_t *>(&epHeader), sizeof(EchoProbeHeader));
    }

    if (frameType == EchoProbeFreqChangeRequest) {
        fp->setMCS(0);
        fp->setSGI(false);
        fp->setChannelBonding(parameters.bw.value_or(20) == 40);
    }

    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT20 && fp->getFrame()->txParameters.channelBonding)
        throw std::invalid_argument("bw=40 is invalid for 802.11n HT20 channel.");

    return fp;
}

void EchoProbeInitiator::printDots(int count) {

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
        default:
            return std::vector<double>{0};
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
    auto closestFreq = closest(picoScenesNIC->getConfiguration()->getSystemSupportedFrequencies(), cf_begin / 1e6);
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

    closestFreq = closest(picoScenesNIC->getConfiguration()->getSystemSupportedFrequencies(), cf_end / 1e6);
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
            closestFreq = closest(picoScenesNIC->getConfiguration()->getSystemSupportedFrequencies(), cur_cf / 1e6);
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