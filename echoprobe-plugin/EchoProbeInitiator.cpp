//
// Created by Zhiping Jiang on 10/27/17.
//

#include "EchoProbeInitiator.h"

void EchoProbeInitiator::unifiedEchoProbeWork() {

    auto total_acked_count =0, total_tx_count = 0;
    auto workingMode = *hal->parameters->workingMode;

    auto pll_begin = parameters->pll_rate_begin.value_or(hal->getPLLMultipler());
    auto pll_end   = parameters->pll_rate_end.value_or(hal->getPLLMultipler());
    auto pll_step  = parameters->pll_rate_step.value_or(0);
    auto cur_pll   = pll_begin;

    auto cf_begin  = parameters->cf_begin.value_or(hal->getCarrierFreq());
    auto cf_end    = parameters->cf_end.value_or(hal->getCarrierFreq());
    auto cf_step   = parameters->cf_step.value_or(0);
    auto cf_repeat = parameters->cf_repeat.value_or(100);
    auto cur_cf    = cf_begin;
    auto tx_delay_us = *parameters->tx_delay_us;

    LoggingService::info_print("EchoProbe job parameters: sf--> {}:{}:{}, cf--> {}:{}:{} with {} repeats and {}us interval.\n",
            pll_begin, pll_step, pll_end, cf_begin, cf_step, cf_end, cf_repeat, tx_delay_us);

    if (cf_step == 0 && cf_begin == cf_end) {
        cf_step = 1;
    }

    if (pll_step == 0 && pll_begin == pll_end) {
        pll_step = 1;
    }

    parameters->continue2Work = true;
    auto dumperId = fmt::sprintf("rxack_%s", hal->referredInterfaceName);
    do {
        auto bb_rate_mhz = hal->getPLLRate() / 1e6;
        if (workingMode == MODE_Injector && (cur_pll != hal->getPLLMultipler() || cur_cf != hal->getCarrierFreq())) {
            if (cur_pll != hal->getPLLMultipler()) {
                LoggingService::info_print("EchoProbe injector shifting {}'s baseband sampling rate to {}MHz...\n", hal->referredInterfaceName, bb_rate_mhz);
                hal->setPLLMultipler(cur_pll);
            }
            if (cur_cf != hal->getCarrierFreq()) {
                LoggingService::info_print("EchoProbe injector shifting {}'s carrier frequency to {}MHz...\n", hal->referredInterfaceName, (double)cur_cf / 1e6);
                hal->setCarrierFreq(cur_cf);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
        }

        if (workingMode == MODE_EchoProbeInitiator && (cur_pll != hal->getPLLMultipler() || cur_cf != hal->getCarrierFreq())) {
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
            auto fp = buildPacket(taskId, EchoProbeFreqChangeRequest);
            auto shiftPLL = false;
            auto shiftCF = false;
            if (cur_pll != hal->getPLLMultipler()) {
                LoggingService::info_print("EchoProbe initiator shifting {}'s baseband sampling rate to {}MHz...\n", hal->referredInterfaceName, bb_rate_mhz);
                fp->echoProbeInfo->pll_rate = cur_pll;
                fp->echoProbeInfo->pll_refdiv = hal->getPLLRefDiv();
                fp->echoProbeInfo->pll_clock_select = hal->getPLLClockSelect();
                shiftPLL = true;
            }
            if (cur_cf != hal->getCarrierFreq()) {
                LoggingService::info_print("EchoProbe initiator shifting {}'s carrier frequency to {}MHz...\n", hal->referredInterfaceName, (double)cur_cf / 1e6);
                fp->echoProbeInfo->frequency = cur_cf;
                shiftCF = true;
            }

            if (auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get(), 500); rxs) {
                replyRXS = rxs;
                LoggingService::info_print("EchoProbe responder's confirmation received.\n");
                if (shiftPLL) hal->setPLLMultipler(cur_pll);
                if (shiftCF) hal->setCarrierFreq(cur_cf);
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            } else {
                LoggingService::warning_print("EchoProbe initiator shifting {} to next rate combination to recover the connection.\n", hal->referredInterfaceName);
                if (shiftPLL) hal->setPLLMultipler(cur_pll);
                if (shiftCF) hal->setCarrierFreq(cur_cf);
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
                if (auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get(), 500); rxs) {
                    replyRXS = rxs;
                } else { // still fails
                    LoggingService::warning_print("Job fails! EchoProbe initiator loses connection to the responder.\n");
                    parameters->continue2Work = false;
                    break;
                }
            }
        }

        auto acked_count = 0, tx_count = 0;
        do {
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
            std::shared_ptr<PacketFabricator> fp = nullptr;
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;

            if (workingMode == MODE_Injector) {
                fp = buildPacket(taskId, SimpleInjection);
                if (parameters->inj_for_intel5300.value_or(false) == true) {
                    if (hal->isAR9300) {
                        hal->setTxNotSounding(false);
                        hal->transmitRawPacket(fp.get());
                        std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
                        hal->setTxNotSounding(true);
                    }
                    fp->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                    fp->setSourceAddress(AthNicParameters::magicIntel123456.data());
                    fp->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
                    hal->transmitRawPacket(fp.get());
                } else {
                    if (hal->isAR9300) {
                        hal->setTxNotSounding(false);
                    } else {
                        fp->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                        fp->setSourceAddress(AthNicParameters::magicIntel123456.data());
                        fp->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
                    }
                    hal->transmitRawPacket(fp.get());
                }
                printDots(++acked_count);
                if (parameters->tx_delay_us)
                    std::this_thread::sleep_for(std::chrono::microseconds(*parameters->tx_delay_us));

            } else if (workingMode == MODE_EchoProbeInitiator) {
                fp = buildPacket(taskId, EchoProbeRequest);
                auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get());
                tx_count += retryPerTx;

                if (rxs) {
                    replyRXS = rxs;
                    acked_count++;
                    if (LoggingService::localDisplayLevel <= Debug) {
                        struct RXS_enhanced rxs_acked_tx;
                        parse_rxs_enhanced(replyRXS->chronosACKBody, &rxs_acked_tx, EXTRA_NOCSI);
                        LoggingService::debug_print("Raw ACK: {}\n", printRXS(*replyRXS.get()));
                        LoggingService::debug_print("ACKed Tx: {}\n", printRXS(rxs_acked_tx));
                    }
                    RXSDumper::getInstance(dumperId).dumpRXS(replyRXS->rawBuffer, replyRXS->rawBufferLength);
                    LoggingService::detail_print("TaskId {} done!\n", taskId);

                    if (LoggingService::localDisplayLevel == Trace) {
                        printDots(acked_count);
                    }

                    if (parameters->tx_delay_us)
                        std::this_thread::sleep_for(std::chrono::microseconds(*parameters->tx_delay_us));
                } else {
                    if (LoggingService::localDisplayLevel == Trace)
                        printf("\n");
                    LoggingService::warning_printf("EchoProbe Job Warning: max retry times reached during measurement @ %luHz...\n", cur_cf);
                    parameters->continue2Work = false;
                    break;
                }
            }
        } while(parameters->continue2Work && acked_count < cf_repeat);

        // tx/ack staticstics
        {
            if (*hal->parameters->workingMode == MODE_Injector) {
                std::swap(acked_count, tx_count);
            }
            total_acked_count += acked_count;
            total_tx_count    += tx_count;
        }

        if (LoggingService::localDisplayLevel == Trace) {
            printf("\n");
        }
        LoggingService::info_print("EchoProbe initiator {} @ cf={}MHz, sf={}MHz, #.tx = {}, #.acked = {}, success rate = {}\%.\n", hal->referredInterfaceName, (double)cur_cf / 1e6, bb_rate_mhz , tx_count, acked_count, 100.0 * acked_count / tx_count);

        cur_cf += cf_step;
        if (cf_step < 0 ? (cur_cf < cf_end) : (cur_cf > cf_end)) {
            
            cur_pll += pll_step;
            cur_cf = cf_begin;

            if (pll_step < 0 ? (cur_pll < pll_end) : (cur_pll > pll_end)) {
                parameters->continue2Work = false;
            } else
                RXSDumper::getInstance(dumperId).finishCurrentSession();
        }
    } while(parameters->continue2Work);

    if (LoggingService::localDisplayLevel == Trace) {
        LoggingService::trace_print("Job done! #.total_tx = {}, #.total_acked = {}, success rate = {}\%.\n", total_tx_count, total_acked_count, 100.0 * total_acked_count / total_tx_count);
    }

    parameters->finishedSessionId = *parameters->workingSessionId;
    blockCV.notify_all();
}

std::tuple<std::shared_ptr<struct RXS_enhanced>, int> EchoProbeInitiator::transmitAndSyncRxUnified(
        PacketFabricator *packetFabricator, int maxRetry, const std::chrono::steady_clock::time_point *txTime) {
    std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
    auto taskId = packetFabricator->packetHeader->header_info.taskId;
    auto retryCount = 0;
    uint8_t	 origin_addr1[6], origin_addr2[6], origin_addr3[6];
    memcpy(origin_addr1, packetFabricator->packetHeader->addr1, 6);
    memcpy(origin_addr2, packetFabricator->packetHeader->addr2, 6);
    memcpy(origin_addr3, packetFabricator->packetHeader->addr3, 6);
    maxRetry = (maxRetry == 0 ? *parameters->tx_max_retry : maxRetry);

    while(retryCount++ < maxRetry) {

        if (parameters->inj_for_intel5300.value_or(false) == true) {
            if (hal->isAR9300) {
                hal->setTxNotSounding(false);
                packetFabricator->setDestinationAddress(origin_addr1);
                packetFabricator->setSourceAddress(origin_addr2);
                packetFabricator->set3rdAddress(origin_addr3);
                hal->transmitRawPacket(packetFabricator, txTime);
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
                hal->setTxNotSounding(true);
            }
            packetFabricator->setDestinationAddress(AthNicParameters::magicIntel123456.data());
            packetFabricator->setSourceAddress(AthNicParameters::magicIntel123456.data());
            packetFabricator->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
            hal->transmitRawPacket(packetFabricator, txTime);
        } else {
            if (hal->isAR9300) {
                hal->setTxNotSounding(false);
            } else {
                packetFabricator->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                packetFabricator->setSourceAddress(AthNicParameters::magicIntel123456.data());
                packetFabricator->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
            }
            hal->transmitRawPacket(packetFabricator, txTime);
        }

        auto timeout_us_scaling = 20e6 / hal->getPLLRate();
        timeout_us_scaling = timeout_us_scaling < 1 ? 1 : timeout_us_scaling;
        LoggingService::debug_print("rate: {}, timeout_scale: {}, timeout: {}\n", hal->getPLLRate(), timeout_us_scaling, *parameters->timeout_us );
        replyRXS = hal->rxSyncWaitTaskId(taskId, uint32_t (timeout_us_scaling) * *parameters->timeout_us);
        if (replyRXS)
            return std::make_tuple(replyRXS, retryCount);
    }

    return std::make_tuple(nullptr, retryCount);
}

int EchoProbeInitiator::daemonTask() {
    while(true) {
        if (*parameters->workingSessionId != *parameters->finishedSessionId &&
            (*hal->parameters->workingMode == MODE_EchoProbeInitiator || *hal->parameters->workingMode == MODE_Injector)) {

            std::this_thread::sleep_for(std::chrono::seconds(parameters->delayed_start_seconds.value_or(0)));
            unifiedEchoProbeWork();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

void EchoProbeInitiator::startDaemonTask() {
    ThreadPoolSingleton::getInstance().AddJob([this] {
        this->daemonTask();
    });
}

void EchoProbeInitiator::blockWait() {
    if (*hal->parameters->workingMode == MODE_EchoProbeInitiator || *hal->parameters->workingMode == MODE_Injector) {
        std::shared_lock<std::shared_mutex> lock(blockMutex);
        blockCV.wait(lock, [&]()->bool {
            return *parameters->finishedSessionId == *parameters->workingSessionId;
        });
    }
}

std::shared_ptr<PacketFabricator> EchoProbeInitiator::buildPacket(uint16_t taskId, const EchoProbePacketFrameType & frameType) const {
    auto fp= hal->packetFabricator->makePacket_ExtraInfo();
    
    fp->setTaskId(taskId);
    fp->setFrameType(frameType);
    fp->setDestinationAddress(parameters->inj_target_mac_address->data());
    fp->setTxMCS(parameters->mcs.value_or(0));
    fp->setTxGreenField(parameters->inj_5300_gf.value_or(false));
    fp->setTxDuplicationOn40MHz(parameters->inj_5300_duplication.value_or(false));
    if(parameters->bw)
        fp->setTx40MHzBW((*parameters->bw == 40 ? true : false));
    if(hal->parameters->tx_power)
        fp->setTxpower(*hal->parameters->tx_power);
        fp->setTxSGI(parameters->sgi.value_or(false));

    if (*hal->parameters->workingMode == MODE_EchoProbeInitiator) {
        fp->addEchoProbeInfoWithData(0, nullptr, 0);
        fp->echoProbeInfo->ackMCS = parameters->ack_mcs.value_or(-1);
        fp->echoProbeInfo->ackBandWidth = parameters->ack_bw.value_or(-1);
        fp->echoProbeInfo->ackSGI = parameters->ack_sgi.value_or(-1);

        if (frameType == EchoProbeFreqChangeRequest) {
            fp->setTxMCS(0);
            fp->setTxpower(20);
            fp->setTxSGI(false);
//            fp->setTx40MHzBW(false);
        }
    }

    return fp;
}

void EchoProbeInitiator::serialize() {
    propertyDescriptionTree.clear();
    if (parameters->tx_delay_us) {
        propertyDescriptionTree.put("delay", *parameters->tx_delay_us);
    }

    if (parameters->mcs) {
        propertyDescriptionTree.put("mcs", *parameters->mcs);
    }

    if (parameters->bw) {
        propertyDescriptionTree.put("bw", *parameters->bw);
    }

    if (parameters->sgi) {
        propertyDescriptionTree.put("sgi", *parameters->sgi);
    }

    if (parameters->cf_begin) {
        propertyDescriptionTree.put("freq-begin", *parameters->cf_begin);
    }

    if (parameters->cf_end) {
        propertyDescriptionTree.put("freq-end", *parameters->cf_end);
    }

    if (parameters->cf_step) {
        propertyDescriptionTree.put("freq-step", *parameters->cf_step);
    }

    if (parameters->cf_repeat) {
        propertyDescriptionTree.put("freq-repeat", *parameters->cf_repeat);
    }

    if (parameters->inj_target_interface) {
        propertyDescriptionTree.put("target-interface", *parameters->inj_target_interface);
    }

    if (parameters->inj_target_mac_address) {
        propertyDescriptionTree.put("target-mac-address", macAddress2String(*parameters->inj_target_mac_address));
    }

    if (parameters->inj_for_intel5300) {
        propertyDescriptionTree.put("target-intel5300", *parameters->inj_for_intel5300);
    }

    if (parameters->delayed_start_seconds) {
        propertyDescriptionTree.put("delay-start", *parameters->delayed_start_seconds);
    }

    if (parameters->ack_mcs) {
        propertyDescriptionTree.put("ack-mcs", *parameters->ack_mcs);
    }

    if (parameters->ack_bw) {
        propertyDescriptionTree.put("ack-bw", *parameters->ack_bw);
    }

    if (parameters->ack_sgi) {
        propertyDescriptionTree.put("ack-sgi", *parameters->ack_sgi);
    }
}

void EchoProbeInitiator::finalize() {
    if (*parameters->workingSessionId != *parameters->finishedSessionId) {
        parameters->continue2Work = false;
        std::shared_lock<std::shared_mutex> lock(blockMutex);
        ctrlCCV.wait_for(lock, std::chrono::microseconds(1000 + *parameters->tx_delay_us), [&]()->bool {
            return *parameters->finishedSessionId == *parameters->workingSessionId;
        });
    }
}

void EchoProbeInitiator::printDots(int count) {

    if (auto numOfPacketsPerDotDisplay = parameters->numOfPacketsPerDotDisplay.value_or(10)) {
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
