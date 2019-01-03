//
// Created by Zhiping Jiang on 11/20/17.
//

#include "EchoProbeResponder.h"

bool EchoProbeResponder::handle(const struct RXS_enhanced *received_rxs) {
    if (*hal->parameters->workingMode == MODE_Injector || *hal->parameters->workingMode == MODE_EchoProbeInitiator)
        return false;

    if (*hal->parameters->workingMode == MODE_Logger) {
        RXSDumper::getInstance("rx_"+hal->referredInterfaceName).dumpRXS(received_rxs->rawBuffer, received_rxs->rawBufferLength);
        return true;
    }

    if (*hal->parameters->workingMode != MODE_EchoProbeResponder || !received_rxs->txHeader.header_info.hasEchoProbeInfo)
        return false;

    auto replies = this->makePacket_EchoProbeWithACK(received_rxs);
    for(auto & reply: replies) {
        if (parameters->inj_for_intel5300.value_or(false) == true) {
            if (hal->isAR9300) {
                hal->setTxNotSounding(false);
                hal->transmitRawPacket(reply.get());
                std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            }
            hal->setTxNotSounding(true);
            reply->setDestinationAddress(AthNicParameters::magicIntel123456.data());
            reply->setSourceAddress(AthNicParameters::magicIntel123456.data());
            reply->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
            hal->transmitRawPacket(reply.get());
        } else {
            hal->setTxNotSounding(false);
            if (hal->isAR9300 == false) {
                reply->setDestinationAddress(AthNicParameters::magicIntel123456.data());
                reply->setSourceAddress(AthNicParameters::magicIntel123456.data());
                reply->set3rdAddress(AthNicParameters::broadcastFFMAC.data());
            }
            hal->transmitRawPacket(reply.get());
        }

        if (received_rxs->txHeader.header_info.frameType == EchoProbeFreqChangeRequest) {
            for (auto i = 0; i < 2; i ++) { // send Freq Change ACK frame 60 times to ensure the reception at the Initiator
                std::this_thread::sleep_for(std::chrono::microseconds(1000));
                hal->transmitRawPacket(reply.get());
            }
        }
    }

    if (received_rxs->txHeader.header_info.frameType == EchoProbeFreqChangeRequest) {
        auto cf = received_rxs->echoProbeInfo.frequency;
        auto pll_rate = received_rxs->echoProbeInfo.pll_rate;
        auto pll_refdiv = received_rxs->echoProbeInfo.pll_refdiv;
        auto pll_clock_select = received_rxs->echoProbeInfo.pll_clock_select;
        if ((cf > 0 && hal->getCarrierFreq() != cf) || (pll_rate > 0 && hal->getPLLMultipler() != pll_rate)) {
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
            
            if (pll_rate > 0 && hal->getPLLMultipler() != pll_rate) {
                auto bb_rate_mhz = ath9kPLLBandwidthComputation(pll_rate, pll_refdiv, pll_clock_select, (*parameters->bw == 40 ? true : false)) / 1e6;
                LoggingService::info_print("EchoProbe responder shifting {}'s BW to {}MHz...\n", hal->referredInterfaceName, bb_rate_mhz);
                hal->setPLLValues(pll_rate, pll_refdiv, pll_clock_select);
            }

            if (cf > 0 && hal->getCarrierFreq() != cf) {
                LoggingService::info_print("EchoProbe responder shifting {}'s CF to {}MHz...\n", hal->referredInterfaceName, (double)cf / 1e6);
                hal->setCarrierFreq(cf);
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
        }
    }

    return true;
}

std::vector<std::shared_ptr<PacketFabricator>> EchoProbeResponder::makePacket_EchoProbeWithACK(const struct RXS_enhanced *rxs) {
    uint16_t curPos = 0, curLength = 0;
    std::vector<std::shared_ptr<PacketFabricator>> fps;
    auto packetLength = *parameters->ack_maxLengthPerPacket;

    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    if (rxs->txHeader.header_info.frameType == EchoProbeFreqChangeRequest) {
        auto txPacketFabricator = hal->packetFabricator->makePacket_headerOnly();
        txPacketFabricator->setTaskId(rxs->txHeader.header_info.taskId);
        txPacketFabricator->setFrameType(EchoProbeFreqChangeACK);
        txPacketFabricator->setTxMCS(0);
        txPacketFabricator->setTxpower(20);
        txPacketFabricator->setTxSGI(false);
        txPacketFabricator->setDestinationAddress(rxs->txHeader.addr3);
        fps.emplace_back(txPacketFabricator);
    } else do {
        curLength = (rxs->rawBufferLength - curPos) <= packetLength ? (rxs->rawBufferLength - curPos) : packetLength;
        auto txPacketFabricator = hal->packetFabricator->makePacket_chronosWithData(curLength, rxs->rawBuffer + curPos, 0);
        if (curPos + curLength < rxs->rawBufferLength) {
            txPacketFabricator->packetHeader->header_info.moreIsComming = 1;
        }
        txPacketFabricator->setTaskId(rxs->txHeader.header_info.taskId);
        txPacketFabricator->setFrameType(EchoProbeReply);
        txPacketFabricator->setTxMCS(rxs->echoProbeInfo.ackMCS >= 0 ? rxs->echoProbeInfo.ackMCS : *parameters->mcs);
        txPacketFabricator->setTx40MHzBW(
                rxs->echoProbeInfo.ackBandWidth >= 0 ? (rxs->echoProbeInfo.ackBandWidth == 40) : (*parameters->bw == 40));
        txPacketFabricator->setTxSGI(rxs->echoProbeInfo.ackSGI >= 0 ? rxs->echoProbeInfo.ackSGI : *parameters->sgi);
        txPacketFabricator->setTxGreenField(parameters->inj_5300_gf.value_or(false));
        txPacketFabricator->setTxDuplicationOn40MHz(parameters->inj_5300_duplication.value_or(false));
        txPacketFabricator->setDestinationAddress(rxs->txHeader.addr3);
        fps.emplace_back(txPacketFabricator);
        curPos += curLength;
    } while(curPos < rxs->rawBufferLength);

    auto segmentHeadSeq = fps[0]->packetHeader->seq;
    for (auto & fabricator: fps)
        fabricator->packetHeader->segment_head_seq = segmentHeadSeq;

    return fps;
}

void EchoProbeResponder::serialize() {
    propertyDescriptionTree.clear();
}
