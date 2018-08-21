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

#endif //PICOSCENES_UNIFIEDCHRONOS_H
