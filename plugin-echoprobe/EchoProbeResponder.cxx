//
// Created by Zhiping Jiang on 11/20/17.
//

#include "EchoProbeResponder.h"
#include "EchoProbeReplySegment.hxx"

void EchoProbeResponder::handle(const ModularPicoScenesRxFrame &rxframe) {
    if (parameters.workingMode == MODE_Injector || parameters.workingMode == MODE_EchoProbeInitiator)
        return;

    if (parameters.workingMode == MODE_Logger) {
        RXSDumper::getInstance("rx_" + nic->getReferredInterfaceName()).dumpRXS(&rxframe.rawBuffer[0], rxframe.rawBuffer.size());
        return;
    }

    if (parameters.workingMode != MODE_EchoProbeResponder || !rxframe.PicoScenesHeader || (rxframe.PicoScenesHeader->frameType != EchoProbeRequestFrameType && rxframe.PicoScenesHeader->frameType != EchoProbeFreqChangeRequestFrameType))
        return;

    if (!rxframe.txUnknownSegmentMap.contains("EchoProbeRequest"))
        return;

    initiatorDeviceType = rxframe.PicoScenesHeader->deviceType;
    const auto &epBuffer = rxframe.txUnknownSegmentMap.at("EchoProbeRequest");
    auto epSegment = EchoProbeRequestSegment::createByBuffer(&epBuffer[0], epBuffer.size());
    RXSDumper::getInstance("EPR_" + std::to_string(epSegment.echoProbeRequest.sessionId)).dumpRXS(&rxframe.rawBuffer[0], rxframe.rawBuffer.size());

    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequestFrameType) {
        auto replies = makeReplies(rxframe, epSegment.echoProbeRequest);
        for (auto i = 0; i < 1 + (rxframe.PicoScenesHeader->deviceType == PicoScenesDeviceType::USRP ? 5 : 0); i++) {
            for (auto &reply: replies) {
                reply.transmit();
            }
        }
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequestFrameType) {
        auto cf = epSegment.echoProbeRequest.cf;
        auto sf = epSegment.echoProbeRequest.sf;
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

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeReplies(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq) {
    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequestFrameType) {
        return makeRepliesForEchoProbeRequest(rxframe, epReq);
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequestFrameType) {
        return makeRepliesForEchoProbeFreqChangeRequest(rxframe, epReq);
    }

    return std::vector<PicoScenesFrameBuilder>();
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq) {
    auto frameBuilder = PicoScenesFrameBuilder(nic);
    frameBuilder.makeFrame_HeaderOnly();

    EchoProbeReply reply;
    reply.sessionId = epReq.sessionId;
    if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithFullPayload) {
        frameBuilder.addExtraInfo();
        auto copiedCSISegment = std::make_shared<CSISegment>(rxframe.csiSegment);
        frameBuilder.addSegment(copiedCSISegment);
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithFullPayload;
        reply.replyBuffer.resize(rxframe.rawBuffer.size());
        std::copy(rxframe.rawBuffer.cbegin(), rxframe.rawBuffer.cend(), reply.replyBuffer.begin());
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithCSI) {
        frameBuilder.addExtraInfo();
        auto copiedCSISegment = std::make_shared<CSISegment>(rxframe.csiSegment);
        frameBuilder.addSegment(copiedCSISegment);
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithCSI;
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithExtraInfo) {
        frameBuilder.addExtraInfo();
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithExtraInfo;
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyOnlyHeader) {
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyOnlyHeader;
    }
    frameBuilder.addSegment(std::make_shared<EchoProbeReplySegment>(reply));


    frameBuilder.setPicoScenesFrameType(EchoProbeReplyFrameType);
    frameBuilder.setMCS(epReq.ackMCS == -1 ? (parameters.mcs ? *parameters.mcs : 0) : epReq.ackMCS);
    frameBuilder.setNumSTS(epReq.ackNumSTS == -1 ? (parameters.numSTS ? *parameters.numSTS : 1) : epReq.ackNumSTS);
    frameBuilder.setGuardInterval((GuardIntervalEnum) (epReq.ackGI == -1 ? (parameters.gi ? *parameters.gi : 800) : epReq.ackGI));
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
    frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
    frameBuilder.setTxId(rxframe.PicoScenesHeader->txId);
    return std::vector<PicoScenesFrameBuilder>{frameBuilder};
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeFreqChangeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq) {
    std::vector<PicoScenesFrameBuilder> fps;
    // Use txpower(30), MCS(0) , LGI and BW20 to boost the ACK
    auto frameBuilder = PicoScenesFrameBuilder(nic);
    frameBuilder.makeFrame_HeaderOnly();

    frameBuilder.setPicoScenesFrameType(EchoProbeFreqChangeACKFrameType);
    frameBuilder.setMCS(0);
    frameBuilder.setGuardInterval(GuardIntervalEnum::GI_800);
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
    frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
    frameBuilder.setTxId(rxframe.PicoScenesHeader->txId);

    if (initiatorDeviceType == PicoScenesDeviceType::USRP) {
        std::this_thread::sleep_for(2ms);
    }
    fps.reserve(10);
    for (auto i = 0; i < 10; i++)
        fps.emplace_back(frameBuilder);

    return fps;
}
