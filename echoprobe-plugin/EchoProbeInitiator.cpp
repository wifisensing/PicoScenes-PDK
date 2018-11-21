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

    LoggingService::info_print("EchoProbe job parameters: pll--> {}:{}:{}, cf--> {}:{}:{} with {} repeat.\n",
            pll_begin, pll_step, pll_end, cf_begin, cf_step, cf_end, cf_repeat);

    parameters->continue2Work = true;
    do {
        if (cur_pll != hal->getPLLMultipler() && workingMode == MODE_Injector) {
            hal->setPLLMultipler(cur_pll);
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
        }

        if (cur_pll != hal->getPLLMultipler() && workingMode == MODE_EchoProbeInitiator) { // should initiate freq shifting work.
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
            auto fp = buildPacket(taskId, EchoProbeFreqChangeRequest);
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
            fp->echoProbeInfo->pll_rate = cur_pll;
            fp->echoProbeInfo->pll_refdiv = hal->getPLLRefDiv();
            fp->echoProbeInfo->pll_clock_select = hal->getPLLClockSelect();

            if (auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get()); rxs) {
                replyRXS = rxs;
                hal->setPLLMultipler(cur_pll);
            } else {
                LoggingService::warning_print("{} shift to next PLL freq to recovery connection.\n", hal->phyId);
                hal->setPLLMultipler(cur_pll);
                if (auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get()); rxs) {
                    replyRXS = rxs;
                }
            }

            if (!replyRXS) {
                LoggingService::warning_print("EchoProbe Job Error: max retry times reached in PLL frequency changing stage...\n");
                parameters->continue2Work = false;
                break;
            } else { // successfully changed the frequency
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            }
        }

        auto dumperId = fmt::sprintf("rxack_%s", hal->phyId);

        do {
            if (cur_cf != hal->getCarrierFreq() && workingMode == MODE_Injector) {
                hal->setCarrierFreq(cur_cf);
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            }

            if (cur_cf != hal->getCarrierFreq() && workingMode == MODE_EchoProbeInitiator) { // should initiate freq shifting work.
                auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
                auto fp = buildPacket(taskId, EchoProbeFreqChangeRequest);
                std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
                fp->echoProbeInfo->frequency = cur_cf;

                if (auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get()); rxs) {
                    replyRXS = rxs;
                    hal->setCarrierFreq(cur_cf);
                } else {
                    LoggingService::warning_print("{} shift to next freq to recovery connection.\n", hal->phyId);
                    hal->setCarrierFreq(cur_cf);
                    if (auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get()); rxs) {
                        replyRXS = rxs;
                    }
                }

                if (!replyRXS) {
                    LoggingService::warning_print("EchoProbe Job Error: max retry times reached in frequency changing stage...\n");
                    parameters->continue2Work = false;
                    break;
                } else { // successfully changed the frequency
                    std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
                }
            }

            auto acked_count = 0, tx_count = 0, continuousFailure = 0;
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
                        }
                        hal->setTxNotSounding(true);
                        fp->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                        fp->setSourceAddress(AthNicParameters::magicIntel123456.data());
                        fp->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
                        hal->transmitRawPacket(fp.get());
                    } else {
                        hal->setTxNotSounding(false);
                        if (hal->isAR9300 == false) {
                            fp->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                            fp->setSourceAddress(AthNicParameters::magicIntel123456.data());
                            fp->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
                        }
                        hal->transmitRawPacket(fp.get());
                    }

                    printDots(acked_count++);
                } else if (workingMode == MODE_EchoProbeInitiator) {
                    fp = buildPacket(taskId, EchoProbeRequest);
                    auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get());
                    tx_count += retryPerTx;

                    if (rxs) {
                        replyRXS = rxs;
                        acked_count++;
                        continuousFailure = 0;
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
                    } else {
                        if (++continuousFailure > *parameters->tx_max_retry) {
                            if (LoggingService::localDisplayLevel == Trace)
                                printf("\n");
                            LoggingService::warning_printf("EchoProbe Job Warning: max retry times reached during measurement @ %luHz...\n", cur_cf);
                            break;
                        }
                    }
                }
                if (parameters->tx_delay_us)
                    std::this_thread::sleep_for(std::chrono::microseconds(*parameters->tx_delay_us));

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
            LoggingService::info_print("EchoProbe @ cf={}MHz, bw={}MHz, #.tx = {}, #.acked = {}, success rate = {}\%.\n", (double)cur_cf / 1e6, (double)hal->getPLLRate() / 1e6, tx_count, acked_count, 100.0 * acked_count / tx_count);

            cur_cf += cf_step;
        } while (parameters->continue2Work && (cf_step < 0 ? cur_cf > cf_end + cf_step : cur_cf < cf_end + cf_step));
        RXSDumper::getInstance(dumperId).finishCurrentSession();

        cur_pll += pll_step;
        cur_cf = cf_begin;
    } while(parameters->continue2Work && (pll_step < 0 ? cur_pll > pll_end + pll_step : cur_pll < pll_end + pll_step));

    if (LoggingService::localDisplayLevel == Trace) {
        LoggingService::trace_print("Job done! #.total_tx = {}, #.total_acked = {}, success rate = {}\%.\n", total_tx_count, total_acked_count, 100.0 * total_acked_count / total_tx_count);
    }

    parameters->finishedSessionId = *parameters->workingSessionId;
    blockCV.notify_all();
}

std::tuple<std::shared_ptr<struct RXS_enhanced>, int> EchoProbeInitiator::transmitAndSyncRxUnified(
        PacketFabricator *packetFabricator, const std::chrono::steady_clock::time_point *txTime) {
    std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
    auto taskId = packetFabricator->packetHeader->header_info.taskId;
    auto retryCount = 0;
    uint8_t	 origin_addr1[6], origin_addr2[6], origin_addr3[6];
    memcpy(origin_addr1, packetFabricator->packetHeader->addr1, 6);
    memcpy(origin_addr2, packetFabricator->packetHeader->addr2, 6);
    memcpy(origin_addr3, packetFabricator->packetHeader->addr3, 6);

    while(retryCount++ < *parameters->tx_max_retry) {

        if (parameters->inj_for_intel5300.value_or(false) == true && hal->isAR9300) {
            if (hal->isAR9300) {
                hal->setTxNotSounding(false);
                packetFabricator->setDestinationAddress(origin_addr1);
                packetFabricator->setSourceAddress(origin_addr2);
                packetFabricator->set3rdAddress(origin_addr3);
                hal->transmitRawPacket(packetFabricator, txTime);

                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            }

            hal->setTxNotSounding(true);
            packetFabricator->setDestinationAddress(AthNicParameters::magicIntel123456.data());
            packetFabricator->setSourceAddress(AthNicParameters::magicIntel123456.data());
            packetFabricator->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
            hal->transmitRawPacket(packetFabricator, txTime);
        } else {
            hal->setTxNotSounding(false);
            if (hal->isAR9300 == false) {
                packetFabricator->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                packetFabricator->setSourceAddress(AthNicParameters::magicIntel123456.data());
                packetFabricator->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
            }
            hal->transmitRawPacket(packetFabricator, txTime);
        }
        
        replyRXS = hal->rxSyncWaitTaskId(taskId, *parameters->timeout_us);
        if (replyRXS)
            return std::make_tuple(replyRXS, retryCount);
        else
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->tx_delay_us / 2));
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
        if (count % numOfPacketsPerDotDisplay == 0) {
            printf(".");
            fflush(stdout);
        }
        if (count % (numOfPacketsPerDotDisplay * 50) == 0 && count > numOfPacketsPerDotDisplay)
            printf("\n");
    }
}
