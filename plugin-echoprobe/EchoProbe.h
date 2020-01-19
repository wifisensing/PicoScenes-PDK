//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBE_H
#define PICOSCENES_ECHOPROBE_H

#include <PicoScenes/rxs_enhanced.h>

enum EchoProbeWorkingMode: uint8_t {
    Standby = 14,
    MODE_Injector,
    MODE_Logger,
    MODE_EchoProbeInitiator,
    MODE_EchoProbeResponder,
};

enum EchoProbePacketFrameType: uint8_t {
    SimpleInjection = 10,
    EchoProbeRequest,
    EchoProbeReply,
    EchoProbeFreqChangeRequest,
    EchoProbeFreqChangeACK,
};

#endif //PICOSCENES_ECHOPROBE_H
