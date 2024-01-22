//
// Created by Zhiping Jiang on 11/20/17.
//


#include "EchoProbeParameters.h"

EchoProbeParameters::EchoProbeParameters() {
    injectorContent = EchoProbeInjectionContent::Full;
    tx_delay_us = 5e5;
    timeout_ms = 150;
    tx_max_retry = 100;
    delay_after_cf_change_ms = 5;
    numOfPacketsPerDotDisplay = 10;
    replyStrategy = EchoProbeReplyStrategy::ReplyWithFullPayload;
    useBatchAPI = false;
}
