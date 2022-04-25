//
// Created by Zhiping Jiang on 11/20/17.
//

#include <PicoScenes/MAC80211CSIExtractableNIC.hxx>
#include "EchoProbeResponder.h"
#include "EchoProbeReplySegment.hxx"

void EchoProbeResponder::handle(const ModularPicoScenesRxFrame &rxframe) {
    if (parameters.workingMode == MODE_Injector || parameters.workingMode == MODE_EchoProbeInitiator)
        return;

    if (parameters.workingMode == MODE_Logger) {
        auto buffer = rxframe.toBuffer();
        if (!parameters.outputFileName)
            RXSDumper::getInstance("rx_" + nic->getReferredInterfaceName()).dumpRXS(buffer.data(), buffer.size());
        else
            RXSDumper::getInstance(*parameters.outputFileName).dumpRXS(buffer.data(), buffer.size());
        return;
    }

    if (parameters.workingMode != MODE_EchoProbeResponder || !rxframe.PicoScenesHeader || (rxframe.PicoScenesHeader->frameType != EchoProbeRequestFrameType && rxframe.PicoScenesHeader->frameType != EchoProbeFreqChangeRequestFrameType))
        return;

    if (!rxframe.txUnknownSegmentMap.contains("EchoProbeRequest"))
        return;

    initiatorDeviceType = rxframe.PicoScenesHeader->deviceType;
    const auto &epBuffer = rxframe.txUnknownSegmentMap.at("EchoProbeRequest");
    auto epSegment = EchoProbeRequestSegment::createByBuffer(&epBuffer[0], epBuffer.size());
    auto buffer = rxframe.toBuffer();
    if (!parameters.outputFileName) {
        auto dumpId = fmt::sprintf("EPR_%s_%u", nic->getReferredInterfaceName(), epSegment.getEchoProbeRequest().sessionId);
        RXSDumper::getInstance(dumpId).dumpRXS(buffer.data(), buffer.size());
    } else
        RXSDumper::getInstance(*parameters.outputFileName).dumpRXS(buffer.data(), buffer.size());

    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequestFrameType) {
        auto replies = makeReplies(rxframe, epSegment.getEchoProbeRequest());
        for (auto i = 0; i < 1 + (rxframe.PicoScenesHeader->deviceType == PicoScenesDeviceType::USRP ? 5 : 0); i++) {
            for (auto &reply: replies) {
                reply.transmit();
            }
        }
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequestFrameType) {
        auto cf = epSegment.getEchoProbeRequest().cf;
        auto sf = epSegment.getEchoProbeRequest().sf;
        if (cf > 0 && nic->getFrontEnd()->getCarrierFrequency() != cf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService::info_print("EchoProbe responder shifting {}'s CF to {}MHz...\n", nic->getReferredInterfaceName(), (double) cf / 1e6);
            nic->getFrontEnd()->setCarrierFrequency(cf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }

        if (sf > 0 && nic->getFrontEnd()->getSamplingRate() != sf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService::info_print("EchoProbe responder shifting {}'s BW to {}MHz...\n", nic->getReferredInterfaceName(), sf / 1e6);
            nic->getFrontEnd()->setSamplingRate(sf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }

        auto replies = makeReplies(rxframe, epSegment.getEchoProbeRequest());
        for (auto i = 0; i < 1 + (rxframe.PicoScenesHeader->deviceType == PicoScenesDeviceType::USRP ? 5 : 0); i++) {
            for (auto &reply: replies) {
                reply.transmit();
            }
        }
    }
}

void EchoProbeResponder::startJob(const EchoProbeParameters &parametersV) {
    this->parameters = parametersV;
    if (parameters.workingMode == MODE_EchoProbeResponder) {
        nic->getFrontEnd()->setDestinationMACAddressFilter(std::vector<std::array<uint8_t, 6>>{PicoScenesFrameBuilder::magicIntel123456});
    }
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
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithFullPayload;
        reply.payloadName = "EchoProbeReplyFull";
        frameBuilder.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
        frameBuilder.addSegment(std::make_shared<PayloadSegment>(reply.payloadName, rxframe.toBuffer(), PayloadDataType::FullPicoScenesPacket));
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithCSI) {
        frameBuilder.addExtraInfo();
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithCSI;
        reply.payloadName = "EchoProbeReplyCSI";
        frameBuilder.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
        frameBuilder.addSegment(std::make_shared<PayloadSegment>(reply.payloadName, rxframe.csiSegment.toBuffer(), PayloadDataType::SignalMatrix));
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithExtraInfo) {
        frameBuilder.addExtraInfo();
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithExtraInfo;
        frameBuilder.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyOnlyHeader) {
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyOnlyHeader;
        frameBuilder.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
    }


    frameBuilder.setPicoScenesFrameType(EchoProbeReplyFrameType);
    frameBuilder.setTxParameters(nic->getUserSpecifiedTxParameters());
    frameBuilder.setDestinationAddress(rxframe.standardHeader.addr3.data());
    if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300 || nic->getDeviceType() == PicoScenesDeviceType::IWLMVM_AX200 || nic->getDeviceType() == PicoScenesDeviceType::IWLMVM_AX210) {
        auto macNIC = std::dynamic_pointer_cast<MAC80211CSIExtractableNIC>(nic);
        frameBuilder.setSourceAddress(nic->getFrontEnd()->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(macNIC->getMacAddressDev().data());
    }
    if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
        frameBuilder.setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        frameBuilder.setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        frameBuilder.set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
    } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
        frameBuilder.setSourceAddress(nic->getFrontEnd()->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());
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
    frameBuilder.setDestinationAddress(rxframe.standardHeader.addr3.data());
    if (nic->getDeviceType() == PicoScenesDeviceType::QCA9300) {
        auto macNIC = std::dynamic_pointer_cast<MAC80211CSIExtractableNIC>(nic);
        frameBuilder.setSourceAddress(macNIC->getFrontEnd()->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(macNIC->getMacAddressDev().data());
    }
    if (nic->getDeviceType() == PicoScenesDeviceType::IWL5300) {
        frameBuilder.setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        frameBuilder.setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
        frameBuilder.set3rdAddress(PicoScenesFrameBuilder::broadcastFFMAC.data());
    } else if (nic->getDeviceType() == PicoScenesDeviceType::USRP) {
        frameBuilder.setSourceAddress(nic->getFrontEnd()->getMacAddressPhy().data());
        frameBuilder.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());
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
