//
// Created by Zhiping Jiang on 11/20/17.
//

#include "UnifiedChronosResponder.h"

bool UnifiedChronosResponder::handle(const struct RXS_enhanced *received_rxs) {
    if (*hal->parameters->workingMode != ChronosResponder || !received_rxs->txHeader.header_info.hasChronosInfo)
        return false;

    auto replies = this->makePacket_chronosWithACK(received_rxs);
    for(auto & reply: replies) {
        hal->transmitRawPacket(reply.get());
        if (reply->packetHeader->header_info.frameType == UnifiedChronosFreqChangeRequest) {
            for (auto i = 0; i < 60; i ++) { // send Freq Change ACK frame 60 times to ensure the reception at the Initiator
                hal->transmitRawPacket(reply.get());
                // std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    }

    if (received_rxs->chronosInfo.frequency > 0 && hal->getCarrierFreq() != received_rxs->chronosInfo.frequency) {
        std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_freq_change_us));
        hal->setCarrierFreq(received_rxs->chronosInfo.frequency);
    }

    return true;
}


std::vector<std::shared_ptr<PacketFabricator>> UnifiedChronosResponder::makePacket_chronosWithACK(const struct RXS_enhanced * rxs) {
    uint16_t curPos = 0, curLength = 0;
    std::vector<std::shared_ptr<PacketFabricator>> fps;
    auto packetLength = *parameters->chronos_ack_maxLengthPerPacket;

    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    if (rxs->txHeader.header_info.frameType == UnifiedChronosFreqChangeRequest) {
        auto txPacketFabricator = hal->packetFabricator->makePacket_ExtraInfo();
        txPacketFabricator->setTaskId(rxs->txHeader.header_info.taskId);
        txPacketFabricator->setFrameType(UnifiedChronosFreqChangeACK);
        txPacketFabricator->setTxMCS(0);
        txPacketFabricator->setTxpower(30);
        txPacketFabricator->setTxSGI(false);
        txPacketFabricator->setTx40MHzBW(false);
        txPacketFabricator->setDestinationAddress(rxs->txHeader.addr3);
        fps.emplace_back(txPacketFabricator);
    } else do {
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
        txPacketFabricator->setDestinationAddress(rxs->txHeader.addr3);
        fps.emplace_back(txPacketFabricator);
        curPos += curLength;
    } while(curPos < rxs->rawBufferLength);

    return fps;
}

void UnifiedChronosResponder::serialize() {
    propertyDescriptionTree.clear();
}
