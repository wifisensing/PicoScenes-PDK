//
// Created by Zhiping Jiang on 11/20/17.
//

#ifndef PICOSCENES_UNIFIEDCHRONOS_H
#define PICOSCENES_UNIFIEDCHRONOS_H

#include <PicoScenes/rxs_enhanced.h>

enum ChronosWorkingMode: uint8_t {
    Injector = 15,
    ChronosInitiator,
    ChronosResponder,
};

enum ChronosPacketFrameType: uint8_t {
    SimpleInjection = 10,
    UnifiedChronosProbeRequest,
    UnifiedChronosProbeReply,
    UnifiedChronosFreqChangeRequest,
    UnifiedChronosFreqChangeACK,
};

enum ChronosACKType: uint8_t {
    ChronosACKType_NoACK = 10,
    ChronosACKType_Injection,
    ChronosACKType_Colocation,
    ChronosACKType_Colocation_Or_Injection,
};

enum ChronosACKInjectionType: uint8_t {
    ChronosACKInjectionType_HeaderOnly = 10,
    ChronosACKInjectionType_ExtraInfo,
    ChronosACKInjectionType_Chronos,
    ChronosACKInjectionType_Chronos_or_HeaderWithColocation,
};


enum UnifiedChronosStage: uint8_t {
    UnifiedChronosInitiating,
    UnifiedChronosProbeRequestSent,
};

#endif //PICOSCENES_UNIFIEDCHRONOS_H
