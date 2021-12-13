//
// Created by Zhiping Jiang on 11/20/17.
//


#include "EchoProbeParameters.h"

EchoProbeParameters::EchoProbeParameters() {
    inj_target_mac_address = PicoScenesFrameBuilder::magicIntel123456;
    format = PacketFormatEnum::PacketFormat_HT;
    mcs = 0;
    numSTS = 1;
    guardInterval = 800;
    cbw = 20;
    coding = uint32_t(ChannelCodingEnum::BCC);
    txHEExtendedRange = false;
    heHighDoppler = false;
    heMidamblePeriodicity = 10;
    injectorContent = EchoProbeInjectionContent::Full;
    tx_delay_us = 5e5;
    timeout_ms = 10;
    tx_max_retry = 100;
    delay_after_cf_change_ms = 5;
    numOfPacketsPerDotDisplay = 10;
    replyStrategy = EchoProbeReplyStrategy::ReplyWithFullPayload;
    ifs = 10e-6;
}
