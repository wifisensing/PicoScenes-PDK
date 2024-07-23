//
// Created by Zhiping Jiang on 11/20/17.
//

#include <PicoScenes/MAC80211CSIExtractableNIC.hxx>
#include "EchoProbeResponder.hxx"
#include "EchoProbeReplySegment.hxx"

void EchoProbeResponder::handle(const ModularPicoScenesRxFrame &rxframe) {
    if (parameters.workingMode == EchoProbeWorkingMode::Injector || parameters.workingMode == EchoProbeWorkingMode::EchoProbeInitiator)
        return;

    if (parameters.workingMode == EchoProbeWorkingMode::Logger) {
        if (!parameters.outputFileName)
            FrameDumper::getInstance("rx_" + nic->getReferredInterfaceName())->dumpRxFrame(rxframe);
        else
            FrameDumper::getInstanceWithoutTime(*parameters.outputFileName)->dumpRxFrame(rxframe);
        return;
    }

    if (parameters.workingMode == EchoProbeWorkingMode::Radar) {
        if (!parameters.outputFileName)
            FrameDumper::getInstance("radar_" + nic->getReferredInterfaceName())->dumpRxFrame(rxframe);
        else
            FrameDumper::getInstanceWithoutTime(*parameters.outputFileName)->dumpRxFrame(rxframe);
        return;
    }

    if (parameters.workingMode != EchoProbeWorkingMode::EchoProbeResponder || !rxframe.PicoScenesHeader || (rxframe.PicoScenesHeader->frameType != static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeRequestFrameType) && rxframe.PicoScenesHeader->frameType != static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeFreqChangeRequestFrameType)))
        return;

    if (!rxframe.txUnknownSegments.contains("EchoProbeRequest"))
        return;

    initiatorDeviceType = rxframe.PicoScenesHeader->deviceType;
    const auto &epBuffer = rxframe.txUnknownSegments.at("EchoProbeRequest");
    auto epSegment = EchoProbeRequestSegment(epBuffer->getSyncedRawBuffer().data(), epBuffer->getSyncedRawBuffer().size());
    if (!parameters.outputFileName) {
        auto dumpId = fmt::sprintf("EPR_%s_%u", nic->getReferredInterfaceName(), epSegment.getEchoProbeRequest().sessionId);
        FrameDumper::getInstance("rx_" + nic->getReferredInterfaceName())->dumpRxFrame(rxframe);
    } else
        FrameDumper::getInstanceWithoutTime(*parameters.outputFileName)->dumpRxFrame(rxframe);

    if (rxframe.PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeRequestFrameType)) {
        auto replies = makeRepliesFrames(rxframe, epSegment.getEchoProbeRequest());
        for (auto &reply: replies) {
            nic->transmitPicoScenesFrame(reply);
        }
    }

    if (rxframe.PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeFreqChangeRequestFrameType)) {
        auto replies = makeRepliesFrames(rxframe, epSegment.getEchoProbeRequest());
        for (auto &reply: replies) {
            nic->transmitPicoScenesFrame(reply);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        auto cf = epSegment.getEchoProbeRequest().cf;
        auto sf = epSegment.getEchoProbeRequest().sf;
        if (cf > 0 && nic->getFrontEnd()->getCarrierFrequency() != cf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService_Plugin_info_print("EchoProbe responder shifting {}'s CF to {}MHz...", nic->getReferredInterfaceName(), (double) cf / 1e6);
            nic->getFrontEnd()->setCarrierFrequency(cf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }

        if (sf > 0 && nic->getFrontEnd()->getSamplingRate() != sf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
            LoggingService_Plugin_info_print("EchoProbe responder shifting {}'s BW to {}MHz...", nic->getReferredInterfaceName(), sf / 1e6);
            nic->getFrontEnd()->setSamplingRate(sf);
            std::this_thread::sleep_for(std::chrono::milliseconds(*parameters.delay_after_cf_change_ms));
        }
    }
}

void EchoProbeResponder::startJob(const EchoProbeParameters &parametersV) {
    this->parameters = parametersV;
}

std::vector<ModularPicoScenesTxFrame> EchoProbeResponder::makeRepliesFrames(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq) {
    if (rxframe.PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeRequestFrameType)) {
        return makeRepliesForEchoProbeRequestFrames(rxframe, epReq);
    }

    if (rxframe.PicoScenesHeader->frameType == static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeFreqChangeRequestFrameType)) {
        return makeRepliesForEchoProbeFreqChangeRequestFrames(rxframe, epReq);
    }

    return {};
}

std::vector<ModularPicoScenesTxFrame> EchoProbeResponder::makeRepliesForEchoProbeRequestFrames(const ModularPicoScenesRxFrame&rxframe, const EchoProbeRequest&epReq) {
    auto frame = nic->initializeTxFrame();

    EchoProbeReply reply;
    reply.sessionId = epReq.sessionId;
    if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithFullPayload) {
        frame.addSegment(std::make_shared<ExtraInfoSegment>(nic->getFrontEnd()->buildExtraInfo()));
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithFullPayload;
        reply.payloadName = "EchoProbeReplyFull";
        frame.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
        if (rxframe.basebandSignalSegment) {
            auto copied = rxframe;
            copied.basebandSignalSegment = nullptr;
            frame.addSegment(std::make_shared<PayloadSegment>(reply.payloadName, copied.toBuffer(), PayloadDataType::FullPicoScenesPacket));
        } else
            frame.addSegment(std::make_shared<PayloadSegment>(reply.payloadName, rxframe.toBuffer(), PayloadDataType::FullPicoScenesPacket));
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithCSI) {
        frame.addSegment(std::make_shared<ExtraInfoSegment>(nic->getFrontEnd()->buildExtraInfo()));
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithCSI;
        reply.payloadName = "EchoProbeReplyCSI";
        frame.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
        frame.addSegment(std::make_shared<PayloadSegment>(reply.payloadName, rxframe.csiSegment->getSyncedRawBuffer(), PayloadDataType::SignalMatrix));
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyWithExtraInfo) {
        frame.addSegment(std::make_shared<ExtraInfoSegment>(nic->getFrontEnd()->buildExtraInfo()));
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyWithExtraInfo;
        frame.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
    } else if (epReq.replyStrategy == EchoProbeReplyStrategy::ReplyOnlyHeader) {
        reply.replyStrategy = EchoProbeReplyStrategy::ReplyOnlyHeader;
        frame.addSegment(std::make_shared<EchoProbeReplySegment>(reply));
    }

    frame.setPicoScenesFrameType(static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeReplyFrameType));
    frame.setTxParameters(nic->getUserSpecifiedTxParameters());
    frame.setSourceAddress(MagicIntel123456.data());
    frame.setDestinationAddress(MagicIntel123456.data());
    frame.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    if (parameters.inj_for_intel5300.value_or(false)) {
        frame.setForceSounding(false);
        frame.setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
    }
    frame.setTaskId(rxframe.PicoScenesHeader->taskId);
    frame.setTxId(rxframe.PicoScenesHeader->txId);
    return {frame};
}

std::vector<ModularPicoScenesTxFrame> EchoProbeResponder::makeRepliesForEchoProbeFreqChangeRequestFrames(const ModularPicoScenesRxFrame &rxframe, const EchoProbeRequest &epReq) {
    auto frame = nic->initializeTxFrame();

    frame.setPicoScenesFrameType(static_cast<uint8_t>(EchoProbePacketFrameType::EchoProbeFreqChangeACKFrameType));
    frame.setTxParameters(nic->getUserSpecifiedTxParameters());
    frame.setSourceAddress(MagicIntel123456.data());
    frame.setDestinationAddress(MagicIntel123456.data());
    frame.set3rdAddress(nic->getFrontEnd()->getMacAddressPhy().data());

    if (parameters.inj_for_intel5300.value_or(false)) {
        frame.setForceSounding(false);
        frame.setChannelCoding(ChannelCodingEnum::BCC); // IWL5300 doesn't support LDPC coding.
    }

    frame.setTaskId(rxframe.PicoScenesHeader->taskId);
    frame.setTxId(rxframe.PicoScenesHeader->txId);
    return {frame};
}
