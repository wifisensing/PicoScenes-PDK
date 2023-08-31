//
// Created by Zhiping Jiang on 11/20/17.
//

#include <PicoScenes/MAC80211CSIExtractableNIC.hxx>
#include "EchoProbeResponder.hxx"
#include "EchoProbeReplySegment.hxx"

void EchoProbeResponder::handle(const ModularPicoScenesRxFrame &rxframe) {
    if (parameters.workingMode == MODE_Injector || parameters.workingMode == MODE_EchoProbeInitiator)
        return;

    if (parameters.workingMode == MODE_Logger) {
        if (!parameters.outputFileName)
            FrameDumper::getInstance("rx_" + nic->getReferredInterfaceName())->dumpRxFrame(rxframe);
        else
            FrameDumper::getInstanceWithoutTime(*parameters.outputFileName)->dumpRxFrame(rxframe);
        return;
    }

    if (parameters.workingMode != MODE_EchoProbeResponder || !rxframe.PicoScenesHeader || (rxframe.PicoScenesHeader->frameType != EchoProbeRequestFrameType && rxframe.PicoScenesHeader->frameType != EchoProbeFreqChangeRequestFrameType))
        return;

    if (!rxframe.txUnknownSegments.contains("EchoProbeRequest"))
        return;

    initiatorDeviceType = rxframe.PicoScenesHeader->deviceType;
    const auto &epBuffer = rxframe.txUnknownSegments.at("EchoProbeRequest");
    auto epSegment = EchoProbeRequestSegment(epBuffer.rawBuffer.data(), epBuffer.rawBuffer.size());
    if (!parameters.outputFileName) {
        auto dumpId = fmt::sprintf("EPR_%s_%u", nic->getReferredInterfaceName(), epSegment.getEchoProbeRequest().sessionId);
        FrameDumper::getInstance("rx_" + nic->getReferredInterfaceName())->dumpRxFrame(rxframe);
    } else
        FrameDumper::getInstanceWithoutTime(*parameters.outputFileName)->dumpRxFrame(rxframe);

    if (rxframe.PicoScenesHeader->frameType == EchoProbeRequestFrameType) {
        auto replies = makeReplies(rxframe, epSegment.getEchoProbeRequest());
        for (auto &reply: replies) {
            reply.transmit();
        }
    }

    if (rxframe.PicoScenesHeader->frameType == EchoProbeFreqChangeRequestFrameType) {
        auto replies = makeReplies(rxframe, epSegment.getEchoProbeRequest());
        for (auto &reply: replies) {
            reply.transmit();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        auto cf = epSegment.getEchoProbeRequest().cf;
        auto sf = epSegment.getEchoProbeRequest().sf;
        if (cf > 0 && nic->getFrontEnd()->getCarrierFrequency() != cf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService_info_print("EchoProbe responder shifting {}'s CF to {}MHz...", nic->getReferredInterfaceName(), (double) cf / 1e6);
            nic->getFrontEnd()->setCarrierFrequency(cf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }

        if (sf > 0 && nic->getFrontEnd()->getSamplingRate() != sf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService_info_print("EchoProbe responder shifting {}'s BW to {}MHz...", nic->getReferredInterfaceName(), sf / 1e6);
            nic->getFrontEnd()->setSamplingRate(sf);
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

    return std::vector<PicoScenesFrameBuilder>{};
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
        if (rxframe.basebandSignalSegment) {
            auto copied = rxframe;
            copied.basebandSignalSegment = std::nullopt;
            frameBuilder.addSegment(std::make_shared<PayloadSegment>(reply.payloadName, copied.toBuffer(), PayloadDataType::FullPicoScenesPacket));
        } else
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
    frameBuilder.setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
    frameBuilder.setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
    frameBuilder.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    if (parameters.inj_for_intel5300.value_or(false)) {
        frameBuilder.setForceSounding(false);
        frameBuilder.setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
    }
    frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
    frameBuilder.setTxId(rxframe.PicoScenesHeader->txId);
    return std::vector<PicoScenesFrameBuilder>{frameBuilder};
}

std::vector<PicoScenesFrameBuilder> EchoProbeResponder::makeRepliesForEchoProbeFreqChangeRequest(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq) {
    std::vector<PicoScenesFrameBuilder> fps;
    auto frameBuilder = PicoScenesFrameBuilder(nic);
    frameBuilder.makeFrame_HeaderOnly();

    frameBuilder.setPicoScenesFrameType(EchoProbeFreqChangeACKFrameType);
    frameBuilder.setTxParameters(nic->getUserSpecifiedTxParameters());
    frameBuilder.setSourceAddress(PicoScenesFrameBuilder::magicIntel123456.data());
    frameBuilder.setDestinationAddress(PicoScenesFrameBuilder::magicIntel123456.data());
    frameBuilder.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    if (parameters.inj_for_intel5300.value_or(false)) {
        frameBuilder.setForceSounding(false);
        frameBuilder.setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
    }

    frameBuilder.setTaskId(rxframe.PicoScenesHeader->taskId);
    frameBuilder.setTxId(rxframe.PicoScenesHeader->txId);
    return std::vector<PicoScenesFrameBuilder>{frameBuilder};
}
