//
// Created by Zhiping Jiang on 11/20/17.
//

#include "EchoProbeResponder.h"

void EchoProbeResponder::handle(const ModularPicoScenesRxFrame &rxframe) {
    if (parameters.workingMode == MODE_Injector || parameters.workingMode == MODE_EchoProbeInitiator)
        return;

    if (parameters.workingMode == MODE_Logger) {
        RXSDumper::getInstance("rx_" + nic->getReferredInterfaceName()).dumpRXS(&rxframe.rawBuffer[0], rxframe.rawBuffer.size());
        return;
    }

    if (parameters.workingMode != MODE_EchoProbeResponder || !rxframe.PicoScenesHeader || (rxframe.PicoScenesHeader->frameType != EchoProbeRequest && rxframe.PicoScenesHeader->frameType != EchoProbeFreqChangeRequest))
        return;

    if (!rxframe.txUnknownSegmentMap.contains("EchoProbe"))
        return;

    initiatorDeviceType = rxframe.PicoScenesHeader->deviceType;
    const auto & epBuffer = rxframe.txUnknownSegmentMap.at("EchoProbe");
    auto epSegment = EchoProbeSegment();
    epSegment.fromBuffer(&epBuffer[0], epBuffer.size());
    auto replies = makeReplies(rxframe, epSegment.echoProbe);
    for (auto i = 0; i < 1 + (rxframe.PicoScenesHeader->deviceType == PicoScenesDeviceType::USRP ? 5 : 0); i++) {
        for (auto &reply: replies) {
            reply.transmit();
        }
    }
//
//    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
//        auto cf = echoProbeHeader->cf;
//        auto sf = echoProbeHeader->sf;
//        if (cf > 0 && nic->getConfiguration()->getCarrierFreq() != cf) {
//            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
//            LoggingService::info_print("EchoProbe responder shifting {}'s CF to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf / 1e6);
//            nic->getConfiguration()->setCarrierFreq(cf);
//            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
//        }
//
//        if (sf > 0 && nic->getConfiguration()->getSamplingRate() != sf) {
//            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
//            LoggingService::info_print("EchoProbe responder shifting {}'s BW to {}MHz...\n", nic->getReferredInterfaceName(), sf / 1e6);
//            nic->getConfiguration()->setSamplingRate(sf);
//            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
//        }
//    }
}

void EchoProbeResponder::startJob(const EchoProbeParameters &parametersV) {
    this->parameters = parametersV;
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeReplies(const ModularPicoScenesRxFrame &rxframe, const EchoProbe &epHeader) {
    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
        return makeRepliesForEchoProbeFreqChangeRequest(rxframe, epHeader);
    }
    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequest) {
        return makeRepliesForEchoProbeRequest(rxframe, epHeader);
    }
    return std::vector<PicoScenesFrameBuilder>();
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbe &epHeader) {
    PicoScenesFrameBuilder fps;
    uint16_t curPos = 0, curLength = 0;
    auto maxPacketLength = *parameters.ack_maxLengthPerPacket;

//    auto numReplyPackets = uint32_t(std::ceil(1.0f * rxframe.rawBufferLength / maxPacketLength));
//    auto meanStepLength = rxframe.rawBufferLength / numReplyPackets + 1;
//    for (auto i = 0; i < numReplyPackets; ++i) {
//        auto frameBuilder = PicoScenesFrameBuilder(nic);
//        frameBuilder.makeFrame_HeaderOnly();
//        if (curPos == 0)
//            frameBuilder.addExtraInfo();
//        curLength = curPos + meanStepLength <= rxframe.rawBufferLength ? meanStepLength : rxframe.rawBufferLength - curPos;
//        frameBuilder.addSegment("EP", rxframe.rawBuffer.get() + curPos, curLength);
//        curPos += curLength;
//        frameBuilder.setFragNumber(i);
//        if (curPos < rxframe.rawBufferLength) {
//            frameBuilder.setMoreFrags();
//        }
//        frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
//        frameBuilder.setPicoScenesFrameType(EchoProbeReply);
//        frameBuilder.setMCS(epHeader.ackMCS >= 0 ? epHeader.ackMCS : parameters.mcs.value_or(0));
//        frameBuilder.setChannelBonding(epHeader.ackChannelBonding >= 0 ? (epHeader.ackChannelBonding == 1) : (parameters.bw.value_or(20) == 40));
//        frameBuilder.setNumberOfExtraSounding(parameters.ness.value_or(0));
//        if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT20 && frameBuilder.getFrame()->txParameters.channelBonding)
//            throw std::invalid_argument("bw=40 or ack-bw=40 is invalid for 802.11n HT20 channel.");
//        frameBuilder.setSGI(epHeader.ackSGI >= 0 ? epHeader.ackSGI : parameters.sgi.value_or(false));
//        frameBuilder.setDestinationAddress(rxframe.standardHeader.addr3);
//        if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
//            auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
//            frameBuilder.setSourceAddress(picoScenesNIC->getMacAddressPhy().data());
//            frameBuilder.set3rdAddress(picoScenesNIC->getMacAddressDev().data());
//        }
//        if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
//            auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
//            frameBuilder.setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
//            frameBuilder.setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
//            frameBuilder.set3rdAddress(picoScenesNIC->getMacAddressPhy().data());
//        } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
//            frameBuilder.setSourceAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
//            frameBuilder.set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
//        }
//        fps.emplace_back(frameBuilder);
//    }
//
//    if (initiatorDeviceType == PicoScenesDeviceType::USRP) {
//        std::this_thread::sleep_for(2ms);
//    }
    return fps;
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeFreqChangeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbe &epHeader) {
    std::vector<PicoScenesFrameBuilder> fps;
    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    auto frameBuilder = PicoScenesFrameBuilder(nic);
    frameBuilder.makeFrame_HeaderOnly();
    frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
    frameBuilder.setPicoScenesFrameType(EchoProbeFreqChangeACK);
    frameBuilder.setMCS(0);
//    frameBuilder.setSGI(false);
//    frameBuilder.setChannelBonding(epHeader.ackChannelBonding >= 0 ? (epHeader.ackChannelBonding == 1) : (parameters.bw.value_or(20) == 40));
//    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT20 && frameBuilder.getFrame()->txParameters.channelBonding)
//        throw std::invalid_argument("bw=40 is invalid for 802.11n HT20 channel.");
    frameBuilder.setDestinationAddress(rxframe.standardHeader.addr3);
    if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
        auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
        frameBuilder.setSourceAddress(picoScenesNIC->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(picoScenesNIC->getMacAddressDev().data());
    }
    if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
        frameBuilder.setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        frameBuilder.setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        frameBuilder.set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
    } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
        frameBuilder.setSourceAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
    }

    if (initiatorDeviceType == PicoScenesDeviceType::USRP) {
        std::this_thread::sleep_for(2ms);
    }
    fps.reserve(10);
    for (auto i = 0; i < 10; i++)
        fps.emplace_back(frameBuilder);

    return fps;
}
