//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_ECHOPROBE_H
#define PICOSCENES_ECHOPROBE_H

#include <PicoScenes/ModularPicoScenesFrame.hxx>

enum class EchoProbeWorkingMode : uint8_t {
    Standby = 14,
    Injector,
    Logger,
    EchoProbeInitiator,
    EchoProbeResponder,
    Radar
};

enum class EchoProbeInjectionContent: uint8_t {
    NDP = 20,
    Header,
    Full,
};

enum class EchoProbePacketFrameType : uint8_t {
    SimpleInjectionFrameType = 10,
    EchoProbeRequestFrameType,
    EchoProbeReplyFrameType,
    EchoProbeFreqChangeRequestFrameType,
    EchoProbeFreqChangeACKFrameType,
};

#endif //PICOSCENES_ECHOPROBE_H
