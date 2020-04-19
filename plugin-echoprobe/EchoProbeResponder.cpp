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

    initiatorDeviceType = rxframe.PicoScenesHeader->deviceType;
    auto echoProbeHeader = EchoProbeHeader::fromBuffer(rxframe.segmentMap->at("EP").second.get(), rxframe.segmentMap->at("EP").first);
    if (!echoProbeHeader) {
        LoggingService::warning_print("EchoProbeHeader parser failed.");
        return;
    }

    auto replies = makeReplies(rxframe, *echoProbeHeader);
    for (auto &reply: replies) {
        reply.transmit();
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
        auto cf = echoProbeHeader->cf;
        auto sf = echoProbeHeader->sf;
        if (cf > 0 && nic->getConfiguration()->getCarrierFreq() != cf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService::info_print("EchoProbe responder shifting {}'s CF to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf / 1e6);
            nic->getConfiguration()->setCarrierFreq(cf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }

        if (sf > 0 && nic->getConfiguration()->getSamplingRate() != sf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService::info_print("EchoProbe responder shifting {}'s BW to {}MHz...\n", nic->getReferredInterfaceName(), sf / 1e6);
            nic->getConfiguration()->setSamplingRate(sf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }
    }
}

void EchoProbeResponder::startJob(const EchoProbeParameters &parametersV) {
    this->parameters = parametersV;
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeReplies(const PicoScenesRxFrameStructure &rxframe, const EchoProbeHeader &epHeader) {
    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequest) {
        return makeRepliesForEchoProbeFreqChangeRequest(rxframe, epHeader);
    }
    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequest) {
        return makeRepliesForEchoProbeRequest(rxframe, epHeader);
    }
    return std::vector<PicoScenesFrameBuilder>();
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeRequest(const PicoScenesRxFrameStructure &rxframe, const EchoProbeHeader &epHeader) {
    std::vector<PicoScenesFrameBuilder> fps;
    uint16_t curPos = 0, curLength = 0;
    auto maxPacketLength = *parameters.ack_maxLengthPerPacket;
    auto numReplyPackets = uint32_t(std::ceil(1.0f * rxframe.rawBufferLength / maxPacketLength));
    auto meanStepLength = rxframe.rawBufferLength / numReplyPackets + 1;
    for (auto i = 0; i < numReplyPackets; ++i) {
        auto frameBuilder = PicoScenesFrameBuilder(nic);
        frameBuilder.makeFrame_HeaderOnly();
        if (curPos == 0)
            frameBuilder.addExtraInfo();
        curLength = curPos + meanStepLength <= rxframe.rawBufferLength ? meanStepLength : rxframe.rawBufferLength - curPos;
        frameBuilder.addSegment("EP", rxframe.rawBuffer.get() + curPos, curLength);
        curPos += curLength;
        if (curPos < rxframe.rawBufferLength) {
            frameBuilder.setMoreFrags();
            frameBuilder.setFragNumber(i);
        }
        frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
        frameBuilder.setPicoScenesFrameType(EchoProbeReply);
        frameBuilder.setMCS(epHeader.ackMCS >= 0 ? epHeader.ackMCS : *parameters.mcs);
        frameBuilder.setChannelBonding(epHeader.ackChannelBonding >= 0 ? (epHeader.ackChannelBonding == 1) : (*parameters.bw == 40));
        if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT20 && frameBuilder.getFrame()->txParameters.channelBonding)
            throw std::invalid_argument("bw=40 or ack-bw=40 is invalid for 802.11n HT20 channel.");
        frameBuilder.setSGI(epHeader.ackSGI >= 0 ? epHeader.ackSGI : *parameters.sgi);
        frameBuilder.setGreenField(parameters.inj_5300_gf.value_or(false));
        frameBuilder.setDestinationAddress(rxframe.standardHeader.addr3);
        if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
            auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
            frameBuilder.setSourceAddress(picoScenesNIC->getMacAddressPhy().data());
            frameBuilder.set3rdAddress(picoScenesNIC->getMacAddressDev().data());
        } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
            frameBuilder.setSourceAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
            frameBuilder.set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
        }
        fps.emplace_back(frameBuilder);
    }
    if (rxframe.PicoScenesHeader->deviceType == PicoScenesDeviceType::USRP) {
        auto originLength = fps.size();
        std::copy(fps.cbegin(), fps.cbegin() + originLength, std::back_inserter(fps));
        std::copy(fps.cbegin(), fps.cbegin() + originLength, std::back_inserter(fps));
    }
    return fps;
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeFreqChangeRequest(const PicoScenesRxFrameStructure &rxframe, const EchoProbeHeader &epHeader) {
    std::vector<PicoScenesFrameBuilder> fps;
    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    auto frameBuilder = PicoScenesFrameBuilder(nic);
    frameBuilder.makeFrame_HeaderOnly();
    frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
    frameBuilder.setPicoScenesFrameType(EchoProbeFreqChangeACK);
    frameBuilder.setMCS(0);
    frameBuilder.setSGI(false);
    frameBuilder.setChannelBonding(epHeader.ackChannelBonding >= 0 ? (epHeader.ackChannelBonding == 1) : (*parameters.bw == 40));
    if (channelFlags2ChannelMode(nic->getConfiguration()->getChannelFlags()) == ChannelMode::HT20 && frameBuilder.getFrame()->txParameters.channelBonding)
        throw std::invalid_argument("bw=40 is invalid for 802.11n HT20 channel.");
    frameBuilder.setDestinationAddress(rxframe.standardHeader.addr3);
    if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
        auto picoScenesNIC = std::dynamic_pointer_cast<PicoScenesNIC>(nic);
        frameBuilder.setSourceAddress(picoScenesNIC->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(picoScenesNIC->getMacAddressDev().data());
    } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
        frameBuilder.setSourceAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(nic->getTypedFrontEnd<USRPFrontEnd>()->getMacAddressPhy().data());
    }

    for (auto i = 0; i < 10; i++)
        fps.emplace_back(frameBuilder);

    return fps;
}
