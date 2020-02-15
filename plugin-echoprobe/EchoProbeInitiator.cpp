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

    parameters.continue2Work = true;
    auto sfList = enumerateSamplingFrequencies();
    auto cfList = enumerateCarrierFrequencies();

    LoggingService::info_print("EchoProbe job parameters: sf--> {}:{}:{}, cf--> {}:{}:{}, {} repeats with {}us interval.\n",
                               sfList.front(), parameters.pll_rate_step.value_or(0), sfList.back(), cfList.front(), parameters.cf_step.value_or(0), cfList.back(), cf_repeat, tx_delay_us);

    for (const auto &pll_value: sfList) {
        auto bb_rate_mhz = channelFlags2ChannelMode(config->getChannelFlags()) == ChannelMode::HT20 ? 20 : 40;
        if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
            bb_rate_mhz = ath9kPLLBandwidthComputation(pll_value, config->getPLLRefDiv(), config->getPLLClockSelect(), !(channelFlags2ChannelMode(config->getChannelFlags()) == ChannelMode::HT20)) / 1e6;
        }
        auto dumperId = fmt::sprintf("rxack_%s_bb%u", nic->getReferredInterfaceName(), bb_rate_mhz);
        for (const auto &cf_value: cfList) {
            if (workingMode == MODE_Injector) {
                if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300 && pll_value != config->getPLLMultiplier()) {
                    LoggingService::info_print("EchoProbe injector shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), bb_rate_mhz);
                    config->setPLLMultipler(pll_value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                }
                if (cf_value != config->getCarrierFreq()) {
                    LoggingService::info_print("EchoProbe injector shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), cf_value / 1e6);
                    config->setCarrierFreq(cf_value);
                    std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                }
            } else if (workingMode == MODE_EchoProbeInitiator) {
                EchoProbeHeader epHeader{};
                bool shiftPLL = false, shiftCF = false;
                if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300 && pll_value != config->getPLLMultiplier()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s baseband sampling rate to {}MHz...\n", nic->getReferredInterfaceName(), bb_rate_mhz);
                    epHeader.pll_rate = pll_value;
                    epHeader.pll_refdiv = config->getPLLRefDiv();
                    epHeader.pll_clock_select = config->getPLLClockSelect();
                    shiftPLL = true;
                }
                if (cf_value != config->getCarrierFreq()) {
                    LoggingService::info_print("EchoProbe initiator shifting {}'s carrier frequency to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6);
                    epHeader.frequency = cf_value;
                    shiftCF = true;
                }

                if (shiftCF || shiftPLL) {
                    auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                    auto fp = buildBasicFrame(taskId, EchoProbeFreqChangeRequest);
                    fp->addSegment("EP", (uint8_t *) (&epHeader), sizeof(EchoProbeHeader));
                    if (auto[rxframe, ackframe, retryPerTx] = this->transmitAndSyncRxUnified(fp.get()); rxframe) {
                        LoggingService::info_print("EchoProbe responder confirms the channel changes.\n");
                        if (shiftPLL) config->setPLLMultipler(pll_value);
                        if (shiftCF) config->setCarrierFreq(cf_value);
                        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                    } else {
                        LoggingService::warning_print("EchoProbe initiator shifting {} to next rate combination to recover the connection.\n", nic->getReferredInterfaceName());
                        if (shiftPLL) config->setPLLMultipler(pll_value);
                        if (shiftCF) config->setCarrierFreq(cf_value);
                        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                        if (auto[rxframe, ackframe, retryPerTx] = this->transmitAndSyncRxUnified(fp.get()); !rxframe) { // still fails
                            LoggingService::warning_print("Job fails! EchoProbe initiator loses connection to the responder.\n");
                            parameters.continue2Work = false;
                            break;
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
                    if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
                        fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                        fp->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                        fp->set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
                        fp->transmit();
                    } else {
                        config->setTxNotSounding(false);
                        fp->transmit();
                        if (parameters.inj_for_intel5300.value_or(false)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                            config->setTxNotSounding(true);
                            fp->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                            fp->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                            fp->set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
                            fp->transmit();
                        }
                    }
                    tx_count++;
                    total_tx_count++;
                    if (LoggingService::localDisplayLevel == Trace)
                        printDots(tx_count);
                    std::this_thread::sleep_for(std::chrono::microseconds(parameters.tx_delay_us));
                } else if (workingMode == MODE_EchoProbeInitiator) {
                    fp = buildBasicFrame(taskId, EchoProbeRequest);
                    auto[rxframe, ackframe, retryPerTx] = this->transmitAndSyncRxUnified(fp.get());
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
                        parameters.continue2Work = false;
                        break;
                    }
                }
            }
            if (LoggingService::localDisplayLevel == Trace)
                printf("\n");
            if (workingMode == MODE_Injector)
                LoggingService::info_print("EchoProbe injector {} @ cf={}MHz, sf={}MHz, #.tx = {}.\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6, bb_rate_mhz, tx_count);
            else if (workingMode == MODE_EchoProbeInitiator)
                LoggingService::info_print("EchoProbe initiator {} @ cf={}MHz, sf={}MHz, #.tx = {}, #.acked = {}, success rate = {}\%.\n", nic->getReferredInterfaceName(), (double) cf_value / 1e6, bb_rate_mhz, tx_count, acked_count, 100.0 * acked_count / tx_count);

            if (!parameters.continue2Work)
                break;
        }
        RXSDumper::getInstance(dumperId).finishCurrentSession();
        if (!parameters.continue2Work)
            break;
    }

    if (LoggingService::localDisplayLevel == Trace) {
        if (workingMode == MODE_Injector)
            LoggingService::info_print("Job done! #.total_tx = {}.\n", total_tx_count);
        else if (workingMode == MODE_EchoProbeInitiator)
            LoggingService::trace_print("Job done! #.total_tx = {}, #.total_acked = {}, success rate = {}\%.\n", total_tx_count, total_acked_count, 100.0 * total_acked_count / total_tx_count);
    }

    parameters.finishedSessionId = *parameters.workingSessionId;
    blockCV.notify_all();
}

std::tuple<std::optional<PicoScenesRxFrameStructure>, std::optional<PicoScenesRxFrameStructure>, int> EchoProbeInitiator::transmitAndSyncRxUnified(PicoScenesFrameBuilder *frameBuilder, std::optional<uint32_t> maxRetry) {
    auto taskId = frameBuilder->getFrame()->frameHeader.taskId;
    uint8_t origin_addr1[6], origin_addr2[6], origin_addr3[6];
    memcpy(origin_addr1, frameBuilder->getFrame()->standardHeader.addr1, 6);
    memcpy(origin_addr2, frameBuilder->getFrame()->standardHeader.addr2, 6);
    memcpy(origin_addr3, frameBuilder->getFrame()->standardHeader.addr3, 6);
    auto retryCount = 0;
    maxRetry = (maxRetry ? *maxRetry : parameters.tx_max_retry);

    while (retryCount++ < maxRetry) {
        if (parameters.inj_for_intel5300.value_or(false)) {
            if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
                nic->getConfiguration()->setTxNotSounding(false);
                frameBuilder->setDestinationAddress(origin_addr1);
                frameBuilder->setSourceAddress(origin_addr2);
                frameBuilder->set3rdAddress(origin_addr3);
                frameBuilder->transmit();
                std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
                nic->getConfiguration()->setTxNotSounding(true);
            }
            frameBuilder->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            frameBuilder->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            frameBuilder->set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
            frameBuilder->transmit();
        } else {
            if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
                frameBuilder->setDestinationAddress(parameters.inj_target_mac_address->data());
                frameBuilder->setSourceAddress(nic->getMacAddressPhy().data());
                frameBuilder->set3rdAddress(nic->getMacAddressDev().data());
                nic->getConfiguration()->setTxNotSounding(false);
            } else {
                frameBuilder->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                frameBuilder->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                frameBuilder->set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
            }
            frameBuilder->transmit();
        }

        /*
        * Tx-Rx time grows non-linearly in low PLL rate case, so enlarge the timeout to 11x.
        */
        auto timeout_us_scaling = nic->getConfiguration()->getPLLRate() < 20e6 ? 6 : 1;
        if (auto replyFrame = nic->syncRxWaitTaskId(taskId, timeout_us_scaling * *parameters.timeout_ms)) {
            if (replyFrame->PicoScenesHeader && replyFrame->PicoScenesHeader->frameType == EchoProbeReply) {
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
    fp->setSourceAddress(nic->getMacAddressPhy().data());
    fp->set3rdAddress(nic->getMacAddressDev().data());
    fp->setMCS(parameters.mcs.value_or(0));
    fp->setGreenField(parameters.inj_5300_gf.value_or(false));
    fp->setChannelBonding(parameters.bw.value_or(20) == 40);
    fp->setSGI(parameters.sgi.value_or(false));

    if (frameType == EchoProbeRequest) {
        fp->addExtraInfo();
        EchoProbeHeader epHeader;
        epHeader.ackMCS = parameters.ack_mcs.value_or(-1);
        epHeader.ackChannelBonding = parameters.ack_bw ? (*parameters.ack_bw == 40) : -1;
        epHeader.ackSGI = parameters.ack_sgi.value_or(-1);
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

std::vector<double> EchoProbeInitiator::enumerateCarrierFrequencies() {
    return nic->getDeviceType() == PicoScenesDeviceType::QCA9300 ? enumerateAtherosCarrierFrequencies() : enumerateIntelCarrierFrequencies();
}

std::vector<uint32_t> EchoProbeInitiator::enumerateSamplingFrequencies() {
    auto frequencies = std::vector<uint32_t>();

    if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
        frequencies.emplace_back(0);
        return frequencies;
    }

    auto pll_begin = parameters.pll_rate_begin.value_or(nic->getConfiguration()->getPLLMultiplier());
    auto pll_end = parameters.pll_rate_end.value_or(nic->getConfiguration()->getPLLMultiplier());
    auto pll_step = parameters.pll_rate_step.value_or(1);
    auto cur_pll = pll_begin;

    if (pll_end < pll_begin && pll_step > 0)
        throw std::invalid_argument("pll_step > 0, however pll_end < pll_begin.\n");

    if (pll_end > pll_begin && pll_step < 0)
        throw std::invalid_argument("pll_step < 0, however pll_end > pll_begin.\n");

    do {
        frequencies.emplace_back(cur_pll);
        cur_pll += pll_step;
    } while ((pll_step > 0 && cur_pll <= pll_end) || (pll_step < 0 && cur_pll >= pll_end));

    return frequencies;
}

std::vector<double> EchoProbeInitiator::enumerateAtherosCarrierFrequencies() {
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

    auto frequencies = std::vector<double>();
    auto cf_begin = parameters.cf_begin.value_or(nic->getConfiguration()->getCarrierFreq());
    auto cf_end = parameters.cf_end.value_or(nic->getConfiguration()->getCarrierFreq());
    auto cf_step = parameters.cf_step.value_or(5e6);

    if (std::abs(cf_step) % 5000000 != 0)
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
    auto closestFreq = closest(nic->getConfiguration()->getSystemSupportedFrequencies(), cf_begin / 1e6);
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

    closestFreq = closest(nic->getConfiguration()->getSystemSupportedFrequencies(), cf_end / 1e6);
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
            closestFreq = closest(nic->getConfiguration()->getSystemSupportedFrequencies(), cur_cf / 1e6);
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