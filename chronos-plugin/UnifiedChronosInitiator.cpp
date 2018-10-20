//
// Created by Zhiping Jiang on 10/27/17.
//

#include "UnifiedChronosInitiator.h"

void UnifiedChronosInitiator::unifiedChronosWork() {
    { // initialization if necessary
        if (!parameters->inj_freq_begin)
            parameters->inj_freq_begin = hal->getCarrierFreq();
        if (!parameters->inj_freq_end)
            parameters->inj_freq_end = parameters->inj_freq_begin;
    }

    if (parameters->inj_delayed_start_s)
        std::this_thread::sleep_for(std::chrono::seconds(*parameters->inj_delayed_start_s));

    auto total_acked_count =0, total_tx_count = 0;
    auto freqGrowthDirection = *parameters->inj_freq_begin > *parameters->inj_freq_end;
    auto continue2Work = true;
    for(auto curFreq = *parameters->inj_freq_begin;(freqGrowthDirection ? curFreq >= *parameters->inj_freq_end : curFreq <=*parameters->inj_freq_end); curFreq += (freqGrowthDirection ? -1 : 1) * std::labs(*parameters->inj_freq_step)) {
        if (curFreq != hal->getCarrierFreq() && *hal->parameters->workingMode == Injector) {
            hal->setCarrierFreq(curFreq);
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_freq_change_us));
        }

        if (curFreq != hal->getCarrierFreq() && *hal->parameters->workingMode == ChronosInitiator) { // should initiate freq shifting work.
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
            auto fp = buildPacket(taskId, UnifiedChronosFreqChangeRequest);
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
            fp->chronosInfo->frequency = curFreq;

            {
                auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get());
                if (rxs) {
                    replyRXS = rxs;
                    hal->setCarrierFreq(curFreq);
                }
            }
            
            {
                if (replyRXS==nullptr) {
                    LoggingService::warning_print("{} shift to next freq to recovery connection.\n", hal->phyId);
                    hal->setCarrierFreq(curFreq);
                    auto [rxs, retryPerTx] = this->transmitAndSyncRxUnified(fp.get());
                    if (rxs) {
                        replyRXS = rxs;
                        hal->setCarrierFreq(curFreq);
                    }
                }
            }

            if (!replyRXS) {
                LoggingService::warning_print("Chronos Job Error: max retry times reached in frequency changing stage...\n");
                continue2Work = false;
                break;
            } else { // successfully changed the frequency
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_freq_change_us));
            }
        }

        auto acked_count = 0, tx_count = 0, countPerDot = 0, continuousFailure = 0;
        for(;continue2Work && acked_count < *parameters->inj_freq_repeat;) {
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(0, UINT16_MAX);
            std::shared_ptr<PacketFabricator> fp = nullptr;
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;

            if (*hal->parameters->workingMode == Injector) {
                fp = buildPacket(taskId, SimpleInjection);
                hal->transmitRawPacket(fp.get());
                acked_count++; // this is an ad-hoc solution, not good, but work.
            } else if (*hal->parameters->workingMode == ChronosInitiator) {
                fp = buildPacket(taskId, UnifiedChronosProbeRequest);
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
                    RXSDumper::getInstance("rxack_"+hal->phyId).dumpRXS(replyRXS->rawBuffer, replyRXS->rawBufferLength);
                    LoggingService::detail_print("TaskId {} done!\n", taskId);

                    if (LoggingService::localDisplayLevel == Trace) {
                        if (parameters->numOfPacketsPerDotDisplay && *parameters->numOfPacketsPerDotDisplay > 0) {
                            countPerDot = ++countPerDot % *parameters->numOfPacketsPerDotDisplay;
                            if (countPerDot == 0) {
                                printf(".");
                                fflush(stdout);
                            }
                            if (acked_count % (*parameters->numOfPacketsPerDotDisplay * 50) == 0 && acked_count > *parameters->numOfPacketsPerDotDisplay)
                                printf("\n");
                        }
                    }
                } else {
                    if (++continuousFailure > *parameters->tx_max_retry) {
                        if (LoggingService::localDisplayLevel == Trace)
                            printf("\n");
                        LoggingService::warning_printf("Chronos Job Warning: max retry times reached during measurement @ %luHz...\n", curFreq);
                        break;
                    }
                }
            }
            if (parameters->inj_delay_us)
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->inj_delay_us));
        }

        total_acked_count += acked_count;
        total_tx_count    += tx_count;

        if (LoggingService::localDisplayLevel == Trace) {
            printf("\n");
            LoggingService::trace_print("Chronos in {}Hz, tx = {}, acked = {}, success rate = {}\%.\n", curFreq, tx_count, acked_count, 100.0 * acked_count / tx_count);
        }

    }

    if (LoggingService::localDisplayLevel == Trace) {
        LoggingService::trace_print("Job done! total_tx = {}, total_acked = {}, success rate = {}\%.\n", total_tx_count, total_acked_count, 100.0 * total_acked_count / total_tx_count);
    }

    parameters->finishedSessionId = *parameters->workingSessionId;
    blockCV.notify_all();
}

std::tuple<std::shared_ptr<struct RXS_enhanced>, int> UnifiedChronosInitiator::transmitAndSyncRxUnified(
        const PacketFabricator *packetFabricator, const std::chrono::steady_clock::time_point *txTime) {
    std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
    auto taskId = packetFabricator->packetHeader->header_info.taskId;
    auto retryCount = 0;
    while(retryCount++ < *parameters->tx_max_retry) {
        hal->transmitRawPacket(packetFabricator, txTime);
        replyRXS = hal->rxSyncWaitTaskId(taskId, *parameters->chronos_timeout_us);

        if (replyRXS)
            return std::make_tuple(replyRXS, retryCount);
    }

    return std::make_tuple(nullptr, retryCount);
}

int UnifiedChronosInitiator::daemonTask() {
    while(true) {
        if (*parameters->workingSessionId != *parameters->finishedSessionId && (*hal->parameters->workingMode == ChronosInitiator || *hal->parameters->workingMode == Injector)) {
            unifiedChronosWork();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

void UnifiedChronosInitiator::startDaemonTask() {
    ThreadPoolSingleton::getInstance().AddJob([this] {
        this->daemonTask();
    });
}

void UnifiedChronosInitiator::blockWait() {
    if (*hal->parameters->workingMode == ChronosInitiator || *hal->parameters->workingMode == Injector) {
        std::unique_lock<std::mutex> lock(blockMutex);
        blockCV.wait(lock, [&]()->bool {
            return *parameters->finishedSessionId == *parameters->workingSessionId;
        });
    }
}

std::shared_ptr<PacketFabricator> UnifiedChronosInitiator::buildPacket(uint16_t taskId, const ChronosPacketFrameType & frameType) const {
    auto fp= hal->packetFabricator->makePacket_ExtraInfo();
    
    fp->setTaskId(taskId);
    fp->setFrameType(frameType);
    fp->setDestinationAddress(parameters->inj_target_mac_address->data());

    if (parameters->inj_for_intel5300 && *parameters->inj_for_intel5300 == true) {
        fp->setDestinationAddress(AthNicParameters::magicIntel123456.data());
        fp->setSourceAddress(AthNicParameters::magicIntel123456.data());
        fp->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
        hal->setTxNotSounding(true);
    } else {
        hal->setTxNotSounding(false);
    }
    if(parameters->inj_mcs)
        fp->setTxMCS(*parameters->inj_mcs);
    if(parameters->inj_bw)
        fp->setTx40MHzBW((*parameters->inj_bw == 40 ? true : false));
    if(hal->parameters->tx_power)
        fp->setTxpower(*hal->parameters->tx_power);
    if(parameters->inj_sgi)
        fp->setTxSGI(*parameters->inj_sgi == 1 ? true : false);

    if (*hal->parameters->workingMode == ChronosInitiator) {
        fp->addChronosInfoWithData(0, nullptr, 0);
        if(parameters->chronos_ack_mcs)
            fp->chronosInfo->ackMCS = *parameters->chronos_ack_mcs;
        if(parameters->chronos_ack_bw)
            fp->chronosInfo->ackBandWidth = *parameters->chronos_ack_bw;
        if(parameters->chronos_ack_sgi)
            fp->chronosInfo->ackSGI = *parameters->chronos_ack_sgi;

        if (frameType == UnifiedChronosFreqChangeRequest) {
            fp->setTxMCS(0);
            fp->setTxpower(30);
            fp->setTxSGI(false);
            fp->setTx40MHzBW(false);
        }
    }

    return fp;
}

void UnifiedChronosInitiator::serialize() {
    propertyDescriptionTree.clear();
    if (parameters->inj_delay_us) {
        propertyDescriptionTree.put("delay", *parameters->inj_delay_us);
    }

    if (parameters->inj_mcs) {
        propertyDescriptionTree.put("mcs", *parameters->inj_mcs);
    }

    if (parameters->inj_bw) {
        propertyDescriptionTree.put("bw", *parameters->inj_bw);
    }

    if (parameters->inj_sgi) {
        propertyDescriptionTree.put("sgi", *parameters->inj_sgi);
    }

    if (parameters->inj_freq_begin) {
        propertyDescriptionTree.put("freq-begin", *parameters->inj_freq_begin);
    }

    if (parameters->inj_freq_end) {
        propertyDescriptionTree.put("freq-end", *parameters->inj_freq_end);
    }

    if (parameters->inj_freq_step) {
        propertyDescriptionTree.put("freq-step", *parameters->inj_freq_step);
    }

    if (parameters->inj_freq_repeat) {
        propertyDescriptionTree.put("freq-repeat", *parameters->inj_freq_repeat);
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

    if (parameters->inj_delayed_start_s) {
        propertyDescriptionTree.put("delay-start", *parameters->inj_delayed_start_s);
    }

    if (parameters->chronos_ack_mcs) {
        propertyDescriptionTree.put("ack-mcs", *parameters->chronos_ack_mcs);
    }

    if (parameters->chronos_ack_bw) {
        propertyDescriptionTree.put("ack-bw", *parameters->chronos_ack_bw);
    }

    if (parameters->chronos_ack_sgi) {
        propertyDescriptionTree.put("ack-sgi", *parameters->chronos_ack_sgi);
    }
}
