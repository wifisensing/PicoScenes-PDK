//
// Created by Zhiping Jiang on 10/27/17.
//

#include "EchoProbeInitiator.h"

void EchoProbeInitiator::unifiedEchoProbeWork() {
    { // initialization if necessary
        if (!parameters->cf_begin)
            parameters->cf_begin = hal->getCarrierFreq();
        if (!parameters->cf_end)
            parameters->cf_end = parameters->cf_begin;
    }

    if (parameters->delayed_start_seconds)
        std::this_thread::sleep_for(std::chrono::seconds(*parameters->delayed_start_seconds));

    auto total_acked_count =0, total_tx_count = 0;
    auto freqGrowthDirection = *parameters->cf_begin > *parameters->cf_end;
    auto continue2Work = true;
    for(auto curFreq = *parameters->cf_begin;(freqGrowthDirection ? curFreq >= *parameters->cf_end : curFreq <=*parameters->cf_end); curFreq += (freqGrowthDirection ? -1 : 1) * std::labs(*parameters->cf_step)) {
        if (curFreq != hal->getCarrierFreq() && *hal->parameters->workingMode == MODE_Injector) {
            hal->setCarrierFreq(curFreq);
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
        }

        if (curFreq != hal->getCarrierFreq() && *hal->parameters->workingMode == MODE_EchoProbeInitiator) { // should initiate freq shifting work.
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
            auto fp = buildPacket(taskId, EchoProbeFreqChangeRequest);
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
            fp->echoProbeInfo->frequency = curFreq;

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
                LoggingService::warning_print("EchoProbe Job Error: max retry times reached in frequency changing stage...\n");
                continue2Work = false;
                break;
            } else { // successfully changed the frequency
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            }
        }

        auto acked_count = 0, tx_count = 0, countPerDot = 0, continuousFailure = 0;
        for(;continue2Work && acked_count < *parameters->cf_repeat;) {
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(0, UINT16_MAX);
            std::shared_ptr<PacketFabricator> fp = nullptr;
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;

            if (*hal->parameters->workingMode == MODE_Injector) {
                fp = buildPacket(taskId, SimpleInjection);
                hal->transmitRawPacket(fp.get());
                acked_count++; // this is an ad-hoc solution, not good, but work.
            } else if (*hal->parameters->workingMode == MODE_EchoProbeInitiator) {
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
                        LoggingService::warning_printf("EchoProbe Job Warning: max retry times reached during measurement @ %luHz...\n", curFreq);
                        break;
                    }
                }
            }
            if (parameters->tx_delay_us)
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->tx_delay_us));
        }

        total_acked_count += acked_count;
        total_tx_count    += tx_count;

        if (LoggingService::localDisplayLevel == Trace) {
            printf("\n");
            LoggingService::trace_print("EchoProbe in {}Hz, tx = {}, acked = {}, success rate = {}\%.\n", curFreq, tx_count, acked_count, 100.0 * acked_count / tx_count);
        }

    }

    if (LoggingService::localDisplayLevel == Trace) {
        LoggingService::trace_print("Job done! total_tx = {}, total_acked = {}, success rate = {}\%.\n", total_tx_count, total_acked_count, 100.0 * total_acked_count / total_tx_count);
    }

    parameters->finishedSessionId = *parameters->workingSessionId;
    blockCV.notify_all();
}

std::tuple<std::shared_ptr<struct RXS_enhanced>, int> EchoProbeInitiator::transmitAndSyncRxUnified(
        const PacketFabricator *packetFabricator, const std::chrono::steady_clock::time_point *txTime) {
    std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
    auto taskId = packetFabricator->packetHeader->header_info.taskId;
    auto retryCount = 0;
    while(retryCount++ < *parameters->tx_max_retry) {
        hal->transmitRawPacket(packetFabricator, txTime);
        replyRXS = hal->rxSyncWaitTaskId(taskId, *parameters->timeout_us);

        if (replyRXS)
            return std::make_tuple(replyRXS, retryCount);
    }

    return std::make_tuple(nullptr, retryCount);
}

int EchoProbeInitiator::daemonTask() {
    while(true) {
        if (*parameters->workingSessionId != *parameters->finishedSessionId && (*hal->parameters->workingMode == MODE_EchoProbeInitiator || *hal->parameters->workingMode == MODE_Injector)) {
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
        std::unique_lock<std::mutex> lock(blockMutex);
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

    if (parameters->inj_for_intel5300 && *parameters->inj_for_intel5300 == true) {
        fp->setDestinationAddress(AthNicParameters::magicIntel123456.data());
        fp->setSourceAddress(AthNicParameters::magicIntel123456.data());
        fp->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
        hal->setTxNotSounding(true);
    } else {
        hal->setTxNotSounding(false);
    }
    if(parameters->mcs)
        fp->setTxMCS(*parameters->mcs);
    if(parameters->bw)
        fp->setTx40MHzBW((*parameters->bw == 40 ? true : false));
    if(hal->parameters->tx_power)
        fp->setTxpower(*hal->parameters->tx_power);
    if(parameters->sgi)
        fp->setTxSGI(*parameters->sgi == 1 ? true : false);

    if (*hal->parameters->workingMode == MODE_EchoProbeInitiator) {
        fp->addEchoProbeInfoWithData(0, nullptr, 0);
        if(parameters->ack_mcs)
            fp->echoProbeInfo->ackMCS = *parameters->ack_mcs;
        if(parameters->ack_bw)
            fp->echoProbeInfo->ackBandWidth = *parameters->ack_bw;
        if(parameters->ack_sgi)
            fp->echoProbeInfo->ackSGI = *parameters->ack_sgi;

        if (frameType == EchoProbeFreqChangeRequest) {
            fp->setTxMCS(0);
            fp->setTxpower(30);
            fp->setTxSGI(false);
            fp->setTx40MHzBW(false);
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
}
