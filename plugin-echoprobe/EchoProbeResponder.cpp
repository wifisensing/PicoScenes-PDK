//
// Created by Zhiping Jiang on 11/20/17.
//

#include "EchoProbeResponder.h"

void EchoProbeResponder::handle(const PicoScenesRxFrameStructure &rxframe) {
    if (parameters.workingMode == MODE_Injector || parameters.workingMode == MODE_EchoProbeInitiator)
        return;

    if (parameters.workingMode == MODE_Logger) {
        RXSDumper::getInstance("rx_" + nic->getReferredInterfaceName()).dumpRXS(rxframe.rawBuffer.get(), rxframe.rawBufferLength);
        return;
    }

    if (parameters.workingMode != MODE_EchoProbeResponder || !rxframe.PicoScenesHeader || (rxframe.PicoScenesHeader->frameType != EchoProbeRequest && rxframe.PicoScenesHeader->frameType != EchoProbeFreqChangeRequest))
        return;

    if (!rxframe.segmentMap || rxframe.segmentMap->find("EP") == rxframe.segmentMap->end())
        return;

    auto echoProbeHeader = EchoProbeHeader::fromBuffer(rxframe.segmentMap->at("EP").second.get(), rxframe.segmentMap->at("EP").first);
    if (!echoProbeHeader) {
        LoggingService::warning_print("EchoProbeHeader parser failed.");
        return;
    }

    auto replies = this->makePacket_EchoProbeWithACK(rxframe, *echoProbeHeader);
    for (auto &reply: replies) {
        if (parameters.inj_for_intel5300.value_or(false)) {
            if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
                nic->getConfiguration()->setTxNotSounding(false);
                reply->transmitSync();
                std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            }
            nic->getConfiguration()->setTxNotSounding(true);
            reply->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            reply->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
            reply->set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
            reply->transmitSync();
        } else {
            nic->getConfiguration()->setTxNotSounding(false);
            if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
                reply->setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                reply->setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
                reply->set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
            }
            reply->transmitSync();
        }

        if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
            for (auto i = 0; i < 2; i++) { // send Freq Change ACK frame 60 times to ensure the reception at the Initiator
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                reply->transmitSync();
            }
        }
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
        auto cf = echoProbeHeader->frequency;
        auto pll_rate = echoProbeHeader->pll_rate;
        auto pll_refdiv = echoProbeHeader->pll_refdiv;
        auto pll_clock_select = echoProbeHeader->pll_clock_select;
        if ((cf > 0 && nic->getConfiguration()->getCarrierFreq() != cf) || (pll_rate > 0 && nic->getConfiguration()->getPLLMultiplier() != pll_rate)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));

            if (pll_rate > 0 && nic->getConfiguration()->getPLLMultiplier() != pll_rate) {
                auto bb_rate_mhz = ath9kPLLBandwidthComputation(pll_rate, pll_refdiv, pll_clock_select, *parameters.bw == 40) / 1e6;
                LoggingService::info_print("EchoProbe responder shifting {}'s BW to {}MHz...\n", nic->getReferredInterfaceName(), bb_rate_mhz);
                nic->getConfiguration()->setPLLValues(pll_rate, pll_refdiv, pll_clock_select);
            }

            if (cf > 0 && nic->getConfiguration()->getCarrierFreq() != cf) {
                LoggingService::info_print("EchoProbe responder shifting {}'s CF to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf / 1e6);
                nic->getConfiguration()->setCarrierFreq(cf);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }
    }
}

void EchoProbeResponder::startJob(const EchoProbeParameters &parameters) {
    this->parameters = parameters;
}

std::vector<std::shared_ptr<PicoScenesFrameBuilder>> EchoProbeResponder::makePacket_EchoProbeWithACK(const PicoScenesRxFrameStructure &rxframe, const EchoProbeHeader &epHeader) {

    std::vector<std::shared_ptr<PicoScenesFrameBuilder>> fps;

    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
        auto frameBuilder = std::make_shared<PicoScenesFrameBuilder>(nic);
        frameBuilder->makeFrame_HeaderOnly();
        frameBuilder->setTaskId(rxframe.PicoScenesHeader->taskId);
        frameBuilder->setPicoScenesFrameType(EchoProbeFreqChangeACK);
        frameBuilder->setMCS(0);
        frameBuilder->setSGI(false);
        frameBuilder->setDestinationAddress(rxframe.standardHeader.addr3);
        frameBuilder->setSourceAddress(nic->getMacAddressPhy().data());
        frameBuilder->set3rdAddress(nic->getMacAddressDev().data());
        fps.emplace_back(frameBuilder);
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequest) {
        uint16_t curPos = 0, curLength = 0;
        auto maxPacketLength = *parameters.ack_maxLengthPerPacket;

        do {
            curLength = (rxframe.rawBufferLength - curPos) <= maxPacketLength ? (rxframe.rawBufferLength - curPos) : maxPacketLength;
            auto frameBuilder = std::make_shared<PicoScenesFrameBuilder>(nic);
            frameBuilder->makeFrame_withExtraInfo();
            frameBuilder->addSegment("EP", rxframe.rawBuffer.get() + curPos, curLength);
            if (curPos + curLength < rxframe.rawBufferLength)
                frameBuilder->setMoreFrags();
            frameBuilder->setTaskId(rxframe.PicoScenesHeader->taskId);
            frameBuilder->setPicoScenesFrameType(EchoProbeReply);
            frameBuilder->setMCS(epHeader.ackMCS >= 0 ? epHeader.ackMCS : *parameters.mcs);
            frameBuilder->setChannelBonding(epHeader.ackChannelBonding >= 0 ? (epHeader.ackChannelBonding == 1) : (*parameters.bw == 40));
            frameBuilder->setSGI(epHeader.ackSGI >= 0 ? epHeader.ackSGI : *parameters.sgi);
            frameBuilder->setGreenField(parameters.inj_5300_gf.value_or(false));
            frameBuilder->setDestinationAddress(rxframe.standardHeader.addr3);
            frameBuilder->setSourceAddress(nic->getMacAddressPhy().data());
            frameBuilder->set3rdAddress(nic->getMacAddressDev().data());
            fps.emplace_back(frameBuilder);
            curPos += curLength;
        } while (curPos < rxframe.rawBufferLength);
    }

//    auto segmentHeadSeq = fps[0]->packetHeader->seq;
//    for (auto &fabricator: fps)
//        fabricator->packetHeader->segment_head_seq = segmentHeadSeq;

    return fps;
}
