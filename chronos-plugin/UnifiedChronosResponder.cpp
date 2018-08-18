//
// Created by Zhiping Jiang on 11/20/17.
//

#include "UnifiedChronosResponder.h"

bool UnifiedChronosResponder::handle(const struct RXS_enhanced *received_rxs) {
    if (*hal->parameters->workingMode != ChronosResponder || !received_rxs->txHeader.header_info.hasChronosInfo)
        return false;

    auto handleCompletely = false;
    auto ackType = received_rxs->chronosInfo.ackRequestType;
    if (ackType == ChronosACKType_NoACK)
        handleCompletely = true;

    if (ackType == ChronosACKType_Colocation_Or_Injection) {
        auto colocationalHAL = ColocationService::getInstance()->getColocationalHALByMACAddress(received_rxs->txExtraInfo.macaddr_rom);
        if (!colocationalHAL)
            ackType = ChronosACKType_Injection;
        else
            ackType = ChronosACKType_Colocation;
    }

    if (ackType == ChronosACKType_Injection) {
        std::vector<std::shared_ptr<PacketFabricator>> simpleReplies;
        auto ackInjectionType = received_rxs->chronosInfo.ackInjectionType;

        if (ackInjectionType ==ChronosACKInjectionType_Chronos_or_HeaderWithColocation) {
            auto colocationalHAL = ColocationService::getInstance()->getColocationalHALByMACAddress(received_rxs->txExtraInfo.macaddr_rom);
            if (!colocationalHAL)
                ackInjectionType = ChronosACKInjectionType_Chronos;
            else {
                ackInjectionType = ChronosACKInjectionType_HeaderOnly;
                ackType = ChronosACKType_Colocation;
            }
        }

        if (ackInjectionType ==ChronosACKInjectionType_HeaderOnly) {
            simpleReplies.emplace_back(hal->packetFabricator->makePacket_headerOnly());
        } else if (ackInjectionType ==ChronosACKInjectionType_ExtraInfo) {
            simpleReplies.emplace_back(hal->packetFabricator->makePacket_ExtraInfo());
        } else if (ackInjectionType ==ChronosACKInjectionType_Chronos) {
            auto replies = this->makePacket_chronosWithACK(received_rxs);
            simpleReplies.insert(simpleReplies.end(), replies.begin(), replies.end());
        }

        for(auto & reply: simpleReplies) {
            reply->setDestinationAddress(received_rxs->txHeader.addr3);
            reply->setTxMCS(received_rxs->chronosInfo.ackMCS >= 0 ? received_rxs->chronosInfo.ackMCS : *parameters->inj_mcs);
            reply->setTx40MHzBW(
                    received_rxs->chronosInfo.ackBandWidth >= 0 ? (received_rxs->chronosInfo.ackBandWidth == 40) : (*parameters->inj_bw == 40));
            reply->setTxSGI(received_rxs->chronosInfo.ackSGI >= 0 ? received_rxs->chronosInfo.ackSGI : *parameters->inj_sgi);
            if (received_rxs->chronosInfo.ackTxChainMask > 0 && received_rxs->chronosInfo.ackTxChainMask <=7)
                reply->setTxChainMask(received_rxs->chronosInfo.ackTxChainMask);
            if (received_rxs->chronosInfo.ackRxChainMask > 0 && received_rxs->chronosInfo.ackRxChainMask <=7)
                reply->setRxChainMask(received_rxs->chronosInfo.ackRxChainMask);
            if (received_rxs->chronosInfo.ackTxpower > 0)
                reply->setTxpower(received_rxs->chronosInfo.ackTxpower);
        }


        if (received_rxs->txHeader.header_info.frameType == UnifiedChronosFreqChangeRequest) {
            for(auto & reply: simpleReplies) {
                reply->setFrameType(UnifiedChronosFreqChangeACK);
                reply->setTxMCS(0);

                if (hal->parameters->tx_power || received_rxs->chronosInfo.ackTxpower > 0)
                    reply->setTxpower(30);
            }

        }

        for(auto & reply: simpleReplies) {
            reply->setTaskId(received_rxs->txHeader.header_info.taskId);
            hal->transmitRawPacket(reply.get());
            if (received_rxs->txHeader.header_info.frameType == UnifiedChronosFreqChangeRequest) {
                for (auto i = 0; i < *hal->parameters->tx_max_retry; i ++) { // send Freq Change ACK frame multiple times to ensure the reception at the Initiator
                    hal->transmitRawPacket(reply.get());
                    std::this_thread::sleep_for(std::chrono::microseconds(*hal->parameters->tx_retry_delay_us));
                }
            }
        }

        handleCompletely = true;
    }

    if (ackType == ChronosACKType_Colocation) {
        ColocationService::getInstance()->notifyReception(received_rxs);
        if (received_rxs->chronosInfo.frequency > 0 && hal->getCarrierFreq() != received_rxs->chronosInfo.frequency) {
            hal->setCarrierFreq(received_rxs->chronosInfo.frequency);
        }
        handleCompletely = true;
    }

    if (received_rxs->chronosInfo.frequency > 0 && hal->getCarrierFreq() != received_rxs->chronosInfo.frequency) {
        if (ackType == ChronosACKType_Injection)
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_freq_change_us));
        hal->setCarrierFreq(received_rxs->chronosInfo.frequency);
    }

    return handleCompletely;
}


std::vector<std::shared_ptr<PacketFabricator>> UnifiedChronosResponder::makePacket_chronosWithACK(const struct RXS_enhanced * rxs) {
    uint16_t curPos = 0, curLength = 0;
    std::vector<std::shared_ptr<PacketFabricator>> fps;
    auto packetLength = *parameters->chronos_ack_maxLengthPerPacket;

    do {
        curLength = (rxs->rawBufferLength - curPos) <= packetLength ? (rxs->rawBufferLength - curPos) : packetLength;
        auto txPacketFabricator = hal->packetFabricator->makePacket_chronosWithData(curLength, rxs->rawBuffer + curPos, 0);
        if (curPos + curLength < rxs->rawBufferLength) {
            txPacketFabricator->packetHeader->header_info.moreIsComming = 1;
        }
        txPacketFabricator->setTaskId(rxs->txHeader.header_info.taskId);
        txPacketFabricator->setFrameType(UnifiedChronosProbeReply);
        txPacketFabricator->setTxMCS(rxs->chronosInfo.ackMCS >= 0 ? rxs->chronosInfo.ackMCS : *parameters->inj_mcs);
        txPacketFabricator->setTx40MHzBW(
                rxs->chronosInfo.ackBandWidth >= 0 ? (rxs->chronosInfo.ackBandWidth == 40) : (*parameters->inj_bw == 40));
        txPacketFabricator->setTxSGI(rxs->chronosInfo.ackSGI >= 0 ? rxs->chronosInfo.ackSGI : *parameters->inj_sgi);
        if (rxs->chronosInfo.ackTxChainMask > 0 && rxs->chronosInfo.ackTxChainMask <=7)
            txPacketFabricator->setTxChainMask(rxs->chronosInfo.ackTxChainMask);
        if (rxs->chronosInfo.ackRxChainMask > 0 && rxs->chronosInfo.ackRxChainMask <=7)
            txPacketFabricator->setRxChainMask(rxs->chronosInfo.ackRxChainMask);
        if (rxs->chronosInfo.ackTxpower > 0)
            txPacketFabricator->setTxpower(rxs->chronosInfo.ackTxpower);

        fps.emplace_back(txPacketFabricator);
        curPos += curLength;
    } while(curPos < rxs->rawBufferLength);

    return fps;
}

void UnifiedChronosResponder::serialize() {
    propertyDescriptionTree.clear();
}
