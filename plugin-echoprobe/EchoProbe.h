//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBE_H
#define PICOSCENES_ECHOPROBE_H

#include <PicoScenes/ModularPicoScenesFrame.hxx>

enum EchoProbeWorkingMode: uint8_t {
    MODE_Standby = 14,
    MODE_Injector,
    MODE_Logger,
    MODE_EchoProbeInitiator,
    MODE_EchoProbeResponder,
};

enum class EchoProbeInjectionContent: uint8_t {
    NDP = 20,
    Header,
    Full,
};

enum EchoProbePacketFrameType: uint8_t {
    SimpleInjectionFrameType = 10,
    EchoProbeRequestFrameType,
    EchoProbeReplyFrameType,
    EchoProbeFreqChangeRequestFrameType,
    EchoProbeFreqChangeACKFrameType,
};

#endif //PICOSCENES_ECHOPROBE_H
