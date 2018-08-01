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

    auto inj_total_count =0;
    auto freqGrowthDirection = *parameters->inj_freq_begin > *parameters->inj_freq_end;
    auto continue2Work = true;
    for(auto curFreq = *parameters->inj_freq_begin;(freqGrowthDirection ? curFreq >= *parameters->inj_freq_end : curFreq <=*parameters->inj_freq_end); curFreq += (freqGrowthDirection ? -1 : 1) * std::labs(*parameters->inj_freq_step)) {
        if (curFreq != hal->getCarrierFreq() && *hal->parameters->workingMode == Injector) {
            hal->setCarrierFreq(curFreq);
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_freq_change_us));
        }

        if (curFreq != hal->getCarrierFreq() && *hal->parameters->workingMode == ChronosInitiator) { // should initiate freq shifting work.
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(0, UINT16_MAX);
            auto fp = buildPacket(taskId, UnifiedChronosFreqChangeRequest);
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
            fp->chronosInfo->frequency = curFreq + (parameters->chronos_inj_freq_gap ? *parameters->chronos_inj_freq_gap : 0);
            for(auto retryCount = 0; retryCount < 10; retryCount ++) { // try connect in current frequency
                replyRXS = this->transmitAndSyncRxUnified(fp.get());
                if (replyRXS) {
                    hal->setCarrierFreq(curFreq);
                    break;
                } else if (retryCount >=3){ // try to recover the connect in next frequency.
                    LoggingService::warning_print("{} shift to next freq to recovery connection.\n", hal->phyId);
                    hal->setCarrierFreq(curFreq, CFTuningByFastCC);
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

        for(auto injectionPerFreq = 0; continue2Work && injectionPerFreq < *parameters->inj_freq_repeat; inj_total_count++, injectionPerFreq++) {
            auto taskId = uniformRandomNumberWithinRange<uint16_t>(0, UINT16_MAX);
            std::shared_ptr<PacketFabricator> fp = nullptr;
            std::shared_ptr<RXS_enhanced> replyRXS = nullptr;

            if (*hal->parameters->workingMode == Injector) {
                fp = buildPacket(taskId, SimpleInjection);
                hal->transmitRawPacket(fp.get());
            } else if (*hal->parameters->workingMode == ChronosInitiator) {
                fp = buildPacket(taskId, UnifiedChronosProbeRequest);
                replyRXS = this->transmitAndSyncRxUnified(fp.get());

                if (replyRXS) {
                    if (LoggingService::localDisplayLevel <= Debug) {
                        struct RXS_enhanced rxs_acked_tx;
                        parse_rxs_enhanced(replyRXS->chronosACKBody, &rxs_acked_tx, EXTRA_NOCSI);
                        LoggingService::debug_print("Raw ACK: {}\n", printRXS(*replyRXS.get()));
                        LoggingService::debug_print("ACKed Tx: {}\n", printRXS(rxs_acked_tx));
                    }
                    RXSDumper::getInstance("rxack_"+hal->phyId).dumpRXS(replyRXS->rawBuffer, replyRXS->rawBufferLength);
                    LoggingService::detail_print("TaskId {} done!\n", taskId);
                }
            }
            if (parameters->inj_delay_us)
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->inj_delay_us));
        }
    }

    parameters->finishedSessionId = *parameters->workingSessionId;
    blockCV.notify_all();

}

std::shared_ptr<struct RXS_enhanced> UnifiedChronosInitiator::transmitAndSyncRxUnified(
        const PacketFabricator *packetFabricator, const std::chrono::steady_clock::time_point *txTime) {
    std::shared_ptr<RXS_enhanced> replyRXS = nullptr;
    auto taskId = packetFabricator->packetHeader->header_info.taskId;
    auto retryCount = 0;
    while(retryCount++ < *hal->parameters->tx_max_retry) {
        hal->transmitRawPacket(packetFabricator, txTime);

        if (!packetFabricator->packetHeader->header_info.hasChronosInfo || packetFabricator->chronosInfo->ackRequestType == ChronosACKType_NoACK)
            break;

        switch (packetFabricator->chronosInfo->ackRequestType) {
            case ChronosACKType_Injection: {
                if (packetFabricator->chronosInfo->ackInjectionType == ChronosACKInjectionType_Chronos_or_HeaderWithColocation) {
                    replyRXS = hal->unifiedSyncRx(taskId, true, *parameters->chronos_timeout_us);
                }
                else
                    replyRXS = hal->rxSyncWaitTaskId(taskId, *parameters->chronos_timeout_us);
                break;
            }
            case ChronosACKType_Colocation:
                replyRXS = ColocationService::getInstance()->colocationSyncWaitTaskId(taskId, *parameters->chronos_timeout_us);
                break;
            case ChronosACKType_Colocation_Or_Injection: {
                replyRXS = hal->unifiedSyncRx(taskId, false, *parameters->chronos_timeout_us);
                break;
            }
        }

        if (replyRXS)
            return replyRXS;

        std::this_thread::sleep_for(std::chrono::microseconds(*hal->parameters->tx_retry_delay_us));
    }

    return nullptr;
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
    if (parameters->wait && *parameters->wait == true) {
        std::unique_lock<std::mutex> lock(blockMutex);
        blockCV.wait(lock, [&]()->bool {
            return *parameters->finishedSessionId == *parameters->workingSessionId;
        });
        parameters->wait.reset();
    }
}

std::shared_ptr<PacketFabricator> UnifiedChronosInitiator::buildPacket(uint16_t taskId, const ChronosPacketFrameType & frameType) const {
    auto fp= hal->packetFabricator->makePacket_ExtraInfo();
    fp->setTaskId(taskId);
    fp->setFrameType(frameType);
    fp->setDestinationAddress(parameters->inj_target_mac_address->data());
    if (parameters->inj_for_intel5300) {
        fp->setDestinationAddress(UnifiedChronosParameters::magicIntel123456.data());
        fp->setSourceAddress(UnifiedChronosParameters::magicIntel123456.data());
        fp->set3rdAddress(UnifiedChronosParameters::fullFF.data());
    }
    if(parameters->inj_mcs)
        fp->setTxMCS(*parameters->inj_mcs);
    if(parameters->inj_bw)
        fp->setTx40MHzBW((*parameters->inj_bw == 40 ? true : false));
    if(hal->parameters->tx_power)
        fp->setTxpower(*hal->parameters->tx_power);
    if(hal->parameters->tx_chainmask)
        fp->setTxChainMask(*hal->parameters->tx_chainmask);
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
        if(parameters->chronos_ack_txpower)
            fp->chronosInfo->ackTxpower = *parameters->chronos_ack_txpower;
        if(parameters->chronos_ack_txchainmask)
            fp->chronosInfo->ackTxChainMask = *parameters->chronos_ack_txchainmask;
        if(parameters->chronos_ack_rxchainmask)
            fp->chronosInfo->ackRxChainMask = *parameters->chronos_ack_rxchainmask;
        if(parameters->chronos_ack_additional_delay)
            fp->chronosInfo->ackExpectedDelay_us = *parameters->chronos_ack_additional_delay;

        if (frameType == UnifiedChronosProbeRequest) {
            fp->setChronosACKType(ChronosACKType_Injection);
            fp->setChronosACKInjectionType(ChronosACKInjectionType_Chronos);
        }

        if(parameters->chronos_ack_type)
            fp->setChronosACKType(*parameters->chronos_ack_type);
        if(parameters->chronos_ack_injection_type)
            fp->setChronosACKInjectionType(*parameters->chronos_ack_injection_type);

        if (frameType == UnifiedChronosFreqChangeRequest) {
            fp->setChronosACKType(ChronosACKType_Colocation_Or_Injection);
            fp->setChronosACKInjectionType(ChronosACKInjectionType_HeaderOnly);
        }
    }

    return fp;
}

void UnifiedChronosInitiator::serialize() {
    propertyDescriptionTree.clear();
    if (parameters->inj_delay_us) {
        propertyDescriptionTree.put("inj-delay", *parameters->inj_delay_us);
    }

    if (parameters->inj_mcs) {
        propertyDescriptionTree.put("inj-mcs", *parameters->inj_mcs);
    }

    if (parameters->inj_bw) {
        propertyDescriptionTree.put("inj-bw", *parameters->inj_bw);
    }

    if (parameters->inj_sgi) {
        propertyDescriptionTree.put("inj-sgi", *parameters->inj_sgi);
    }

    if (parameters->inj_freq_begin) {
        propertyDescriptionTree.put("inj-freq-begin", *parameters->inj_freq_begin);
    }

    if (parameters->inj_freq_end) {
        propertyDescriptionTree.put("inj-freq-end", *parameters->inj_freq_end);
    }

    if (parameters->inj_freq_step) {
        propertyDescriptionTree.put("inj-freq-step", *parameters->inj_freq_step);
    }

    if (parameters->inj_freq_repeat) {
        propertyDescriptionTree.put("inj-freq-repeat", *parameters->inj_freq_repeat);
    }

    if (parameters->inj_target_interface) {
        propertyDescriptionTree.put("inj-target-interface", *parameters->inj_target_interface);
    }

    if (parameters->inj_target_mac_address) {
        propertyDescriptionTree.put("inj-target-mac-address", macAddress2String(*parameters->inj_target_mac_address));
    }

    if (parameters->inj_for_intel5300) {
        propertyDescriptionTree.put("inj_for_intel5300", *parameters->inj_for_intel5300);
    }

    if (parameters->inj_delayed_start_s) {
        propertyDescriptionTree.put("inj-delay-start", *parameters->inj_delayed_start_s);
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

    if (parameters->chronos_ack_txpower) {
        propertyDescriptionTree.put("ack-txpower", *parameters->chronos_ack_txpower);
    }

    if (parameters->chronos_ack_txchainmask) {
        propertyDescriptionTree.put("ack-txchainmask", *parameters->chronos_ack_txchainmask);
    }

    if (parameters->chronos_ack_rxchainmask) {
        propertyDescriptionTree.put("ack-rxchainmask", *parameters->chronos_ack_rxchainmask);
    }

    if (parameters->chronos_inj_freq_gap) {
        propertyDescriptionTree.put("ack-freq-gap", *parameters->chronos_inj_freq_gap);
    }

    if (parameters->chronos_ack_maxLengthPerPacket) {
        propertyDescriptionTree.put("ack-package-max-length", *parameters->chronos_ack_maxLengthPerPacket);
    }

    if (parameters->chronos_ack_additional_delay) {
        propertyDescriptionTree.put("ack-additional-delay", *parameters->chronos_ack_additional_delay);
    }

    if (parameters->wait) {
        propertyDescriptionTree.put("wait", *parameters->wait);
    }

    if (parameters->chronos_timeout_us) {
        propertyDescriptionTree.put("ack-timeout", *parameters->chronos_timeout_us);
    }

    if (parameters->chronos_ack_type) {
        if (*parameters->chronos_ack_type == ChronosACKType_NoACK) {
            propertyDescriptionTree.put("ack-type", "no-ack");
        } else if (*parameters->chronos_ack_type == ChronosACKType_Colocation_Or_Injection) {
            propertyDescriptionTree.put("ack-type", "colocation-or-injection");
        } else if (*parameters->chronos_ack_type == ChronosACKType_Injection) {
            propertyDescriptionTree.put("ack-type", "injection");
        } else if (*parameters->chronos_ack_type == ChronosACKType_Colocation) {
            propertyDescriptionTree.put("ack-type", "colocation");
        }
    }

    if (parameters->chronos_ack_injection_type) {
        if (*parameters->chronos_ack_injection_type == ChronosACKInjectionType_HeaderOnly) {
            propertyDescriptionTree.put("ack-injection-type", "header");
        } else if (*parameters->chronos_ack_injection_type == ChronosACKInjectionType_ExtraInfo) {
            propertyDescriptionTree.put("ack-injection-type", "extra");
        } else if (*parameters->chronos_ack_injection_type == ChronosACKInjectionType_Chronos) {
            propertyDescriptionTree.put("ack-injection-type", "Chronos");
        } else if (*parameters->chronos_ack_injection_type == ChronosACKInjectionType_Chronos_or_HeaderWithColocation) {
            propertyDescriptionTree.put("ack-injection-type", "chronos-or-header-with-colocation");
        }
    }

}
