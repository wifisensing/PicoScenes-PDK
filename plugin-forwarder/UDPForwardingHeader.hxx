//
// Created by csi on 24-2-11.
//

#ifndef UDPFORWARDINGHEADER_HXX
#define UDPFORWARDINGHEADER_HXX

#include <cstdint>
#include <optional>

#include "macros.hxx"

PACKED(struct PicoScenesFrameUDPForwardingDiagramHeader {
    uint32_t magicNumber{0x20150315U};
    uint16_t version{0x1U};
    uint16_t diagramTaskId{0};
    uint16_t diagramId{0};
    uint16_t numDiagrams{0};
    uint32_t currentDiagramLength{0};
    uint32_t totalDiagramLength{0};

    static std::optional<PicoScenesFrameUDPForwardingDiagramHeader> fromBuffer(const uint8_t* buffer) {
        if (const auto magicValue = *reinterpret_cast<const uint32_t *>(buffer); magicValue == 0x20150315) {
            auto header = *reinterpret_cast<const PicoScenesFrameUDPForwardingDiagramHeader *>(buffer);
            return header;
        }

        return {};
    }

});

#endif //UDPFORWARDINGHEADER_HXX
