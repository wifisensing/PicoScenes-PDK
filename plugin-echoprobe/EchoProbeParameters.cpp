//
// Created by Zhiping Jiang on 11/20/17.
//


#include "EchoProbeParameters.h"

EchoProbeParameters::EchoProbeParameters() {
    inj_target_mac_address = PicoScenesFrameBuilder::magicIntel123456;
    mcs = 0;
    sgi = 0;
    bw = 20;
    tx_delay_us = 5e5;
    finishedSessionId = UINT64_MAX;
    workingSessionId = UINT64_MAX;
    timeout_ms = 10;
    tx_max_retry = 600;
    ack_maxLengthPerPacket = 1800;
    delay_after_cf_change_ms = 5;
    numOfPacketsPerDotDisplay = 10;
}
