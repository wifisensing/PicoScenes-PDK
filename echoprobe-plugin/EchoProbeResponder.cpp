//
// Created by Zhiping Jiang on 11/20/17.
//

#include "EchoProbeResponder.h"

bool EchoProbeResponder::handle(const struct RXS_enhanced *received_rxs) {
    if (*hal->parameters->workingMode == MODE_Injector || *hal->parameters->workingMode == MODE_EchoProbeInitiator)
        return false;

    if (*hal->parameters->workingMode == MODE_Logger) {
        RXSDumper::getInstance("rx_"+hal->phyId).dumpRXS(received_rxs->rawBuffer, received_rxs->rawBufferLength);
        return true;
    }


    if (*hal->parameters->workingMode != MODE_EchoProbeResponder || !received_rxs->txHeader.header_info.hasEchoProbeInfo)
        return false;

    auto replies = this->makePacket_EchoProbeWithACK(received_rxs);
    for(auto & reply: replies) {
        hal->transmitRawPacket(reply.get());
        if (received_rxs->txHeader.header_info.frameType == EchoProbeFreqChangeRequest) {
            for (auto i = 0; i < 60; i ++) { // send Freq Change ACK frame 60 times to ensure the reception at the Initiator
                hal->transmitRawPacket(reply.get());
            }
        }
    }

    if (auto cf = received_rxs->echoProbeInfo.frequency; cf > 0 && hal->getCarrierFreq() != cf) {
        std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
        hal->setCarrierFreq(cf);
    }

    auto pll_rate   = received_rxs->echoProbeInfo.pll_rate;
    auto pll_refdiv = received_rxs->echoProbeInfo.pll_refdiv;
    auto pll_clksel = received_rxs->echoProbeInfo.pll_clock_select;
    if (pll_rate > 0 || pll_refdiv > 0 || pll_clksel > 0) {
        pll_rate   = pll_rate   > 0 ? pll_rate   : hal->getPLLMultipler();
        pll_refdiv = pll_refdiv > 0 ? pll_refdiv : hal->getPLLRefDiv();
        pll_clksel = pll_clksel > 0 ? pll_clksel : hal->getPLLClockSelect();
        std::this_thread::sleep_for(std::chrono::microseconds(*parameters->delay_after_cf_change_us));
        hal->setPLLValues(pll_rate, pll_refdiv, pll_clksel);
    }

    return true;
}


std::vector<std::shared_ptr<PacketFabricator>> EchoProbeResponder::makePacket_EchoProbeWithACK(const struct RXS_enhanced *rxs) {
    uint16_t curPos = 0, curLength = 0;
    std::vector<std::shared_ptr<PacketFabricator>> fps;
    auto packetLength = *parameters->ack_maxLengthPerPacket;

    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    if (rxs->txHeader.header_info.frameType == EchoProbeFreqChangeRequest) {
        auto txPacketFabricator = hal->packetFabricator->makePacket_ExtraInfo();
        txPacketFabricator->setTaskId(rxs->txHeader.header_info.taskId);
        txPacketFabricator->setFrameType(EchoProbeFreqChangeACK);
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
        txPacketFabricator->setFrameType(EchoProbeReply);
        txPacketFabricator->setTxMCS(rxs->echoProbeInfo.ackMCS >= 0 ? rxs->echoProbeInfo.ackMCS : *parameters->mcs);
        txPacketFabricator->setTx40MHzBW(
                rxs->echoProbeInfo.ackBandWidth >= 0 ? (rxs->echoProbeInfo.ackBandWidth == 40) : (*parameters->bw == 40));
        txPacketFabricator->setTxSGI(rxs->echoProbeInfo.ackSGI >= 0 ? rxs->echoProbeInfo.ackSGI : *parameters->sgi);
        txPacketFabricator->setDestinationAddress(rxs->txHeader.addr3);
        fps.emplace_back(txPacketFabricator);
        curPos += curLength;
    } while(curPos < rxs->rawBufferLength);

    return fps;
}

void EchoProbeResponder::serialize() {
    propertyDescriptionTree.clear();
}
