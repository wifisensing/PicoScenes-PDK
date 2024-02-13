//
// Created by Zhiping Jiang on 10/20/17.
//

#include "UDPForwarderPlugin.hxx"
#include "UDPForwardingHeader.hxx"
#include "boost/algorithm/string.hpp"

std::string UDPForwarderPlugin::getPluginName() {
    return "UDPForwarder";
}

std::string UDPForwarderPlugin::getPluginDescription() {
    return "forward all received packets to the specified destination.";
}

std::vector<PicoScenesDeviceType> UDPForwarderPlugin::getSupportedDeviceTypes() {
    static auto supportedDevices = std::vector<PicoScenesDeviceType>{PicoScenesDeviceType::IWL5300, PicoScenesDeviceType::QCA9300, PicoScenesDeviceType::IWLMVM_AX200, PicoScenesDeviceType::IWLMVM_AX210, PicoScenesDeviceType::VirtualSDR, PicoScenesDeviceType::USRP, PicoScenesDeviceType::SoapySDR};
    return supportedDevices;
}

void UDPForwarderPlugin::initialization() {
    options = std::make_shared<po::options_description>("UDPForward Options", 120);
    options->add_options()
            ("forward-to", po::value<std::string>(), "Destination address and port, e.g., 192.168.10.1:50000");
}

std::shared_ptr<boost::program_options::options_description> UDPForwarderPlugin::pluginOptionsDescription() {
    return options;
}

std::string UDPForwarderPlugin::pluginStatus() {
    return "Destination IP/Port: " + destinationIP.value_or("null") + ":" + std::to_string(destinationPort.value_or(0u));
}

void UDPForwarderPlugin::parseAndExecuteCommands(const std::string& commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.contains("forward-to")) {
        auto input = vm["forward-to"].as<std::string>();
        std::vector<std::string> segments;
        boost::split(segments, input, boost::is_any_of(":"), boost::token_compress_on);
        boost::trim(segments[0]);
        boost::trim(segments[1]);
        destinationIP = segments[0];
        destinationPort = boost::lexical_cast<uint16_t>(segments[1]);

        LoggingService_info_print("UDP Forwarder destination: {}/{}\n", *destinationIP, *destinationPort);
    }
}

void UDPForwarderPlugin::rxHandle(const ModularPicoScenesRxFrame& rxframe) {
    thread_local constexpr auto maxDiagramLength = 65000;
    thread_local auto transferBuffer = std::array<uint8_t, maxDiagramLength + sizeof(PicoScenesFrameUDPForwardingDiagramHeader)>();

    if (!destinationIP || !destinationPort)
        return;

    auto frameBuffer = rxframe.toBuffer();
    const auto taskId = SystemTools::Math::uniformRandomNumberWithinRange<uint16_t>(9999, UINT16_MAX);
    std::vector<U8Vector> segments;
    std::vector<PicoScenesFrameUDPForwardingDiagramHeader> headers;
    if (frameBuffer.size() < maxDiagramLength) {
        // Use move semantics to bypass memory copy
        segments.emplace_back(std::move(frameBuffer));
        headers.emplace_back(PicoScenesFrameUDPForwardingDiagramHeader{
            .version = 0x1U,
            .diagramTaskId = taskId,
            .diagramId = 0,
            .numDiagrams = 1,
            .currentDiagramLength = static_cast<uint32_t>(segments.front().size()),
            .totalDiagramLength = static_cast<uint32_t>(segments.front().size()),
        });
    } else {
        for (auto pos = 0; pos < frameBuffer.size(); pos += maxDiagramLength) {
            auto stepLength = pos + maxDiagramLength < frameBuffer.size() ? maxDiagramLength : frameBuffer.size() - pos;
            segments.emplace_back(frameBuffer.cbegin() + pos, frameBuffer.cbegin() + pos + stepLength);
        }
        for (auto i = 0; i < segments.size(); i++) {
            headers.emplace_back(PicoScenesFrameUDPForwardingDiagramHeader{
                .version = 0x1U,
                .diagramTaskId = taskId,
                .diagramId = static_cast<uint16_t>(i),
                .numDiagrams = static_cast<uint16_t>(segments.size()),
                .currentDiagramLength = static_cast<uint32_t>(segments[i].size()),
                .totalDiagramLength = static_cast<uint32_t>(frameBuffer.size()),
            });
        }
    }

    for (auto i = 0; i < segments.size(); i++) {
        const auto diagramLength = sizeof(PicoScenesFrameUDPForwardingDiagramHeader) + segments[i].size();
        std::copy_n(reinterpret_cast<const uint8_t *>(&headers[i]), sizeof(PicoScenesFrameUDPForwardingDiagramHeader), transferBuffer.begin());
        std::copy_n(segments[i].data(), segments[i].size(), transferBuffer.data() + sizeof(PicoScenesFrameUDPForwardingDiagramHeader));
        SystemTools::Net::udpSendData("Forwarder" + *destinationIP + std::to_string(*destinationPort), transferBuffer.data(), diagramLength, *destinationIP, *destinationPort);
    }
}
