//
// Created by Zhiping Jiang on 2024/1/25.
//
#include <iostream>
#include <array>
#include <boost/asio.hpp>
#include <PicoScenes/AsyncPipeline.hxx>
#include <PicoScenes/FrameDumper.hxx>
#include <PicoScenes/ModularPicoScenesFrame.hxx>

using boost::asio::ip::udp;

class PicoScenesRxFrameUDPServer {
public:
    explicit PicoScenesRxFrameUDPServer(const uint16_t port) : rxSocket(io_service, udp::endpoint(udp::v4(), port)) {
        // This pipeline is used to decode the input bytes to frames
        rxBytesPipeline = std::make_shared<AsyncPipeline<std::vector<uint8_t>>>();
        rxBytesPipeline->registerAsyncHandler("RxFrameDecoder", [&](const std::vector<uint8_t>&bytes) {
            if (const auto decodedFrame = ModularPicoScenesRxFrame::fromBuffer(bytes.data(), bytes.size())) {
                rxFramePipeline->send(*decodedFrame);
            }
        }).startService();

        // This pipeline is used to dump the decoded frames.
        rxFramePipeline = std::make_shared<AsyncPipeline<ModularPicoScenesRxFrame>>();
        rxFramePipeline->registerAsyncHandler("DumpFrames", [&](const ModularPicoScenesRxFrame&rxframe) {
            FrameDumper::getInstance("udp_logged")->dumpRxFrame(rxframe);
        }).startService();

        setupUDPAsyncReceiving();
    }

    void setupUDPAsyncReceiving() {
        rxSocket.async_receive_from(boost::asio::buffer(rxBuffer), rxRemoteEndPoint, [this](const boost::system::error_code&error, std::size_t bytes_transferred) {
            this->rxHandler(error, bytes_transferred);
        });
    }

    void rxHandler(const boost::system::error_code&errorCode, const std::size_t bytesTransferred) {
        if (!errorCode) {

            // Use dual-AsyncPipeline approach to prevent the possible UDP packet loss
            rxBytesPipeline->send(std::vector<uint8_t>(rxBuffer.data(), rxBuffer.data() + bytesTransferred));
            setupUDPAsyncReceiving();
        }
    }

    void startRxService() {
        io_service.run();
    }

    // BOOST ASIO facilities
    boost::asio::io_service io_service;
    udp::socket rxSocket;
    udp::endpoint rxRemoteEndPoint;

    // Large buffer for large frames, e.g., super long frames with MIMO and large CBW
    std::array<uint8_t, 10000000> rxBuffer{};

    // Two asynchronous pipelines used to prevent UDP blocking or packet loss
    std::shared_ptr<AsyncPipeline<std::vector<uint8_t>>> rxBytesPipeline;
    std::shared_ptr<AsyncPipeline<ModularPicoScenesRxFrame>> rxFramePipeline;
};

int main(const int argc, char *argv[]) {

    PicoScenesRxFrameUDPServer server(12345);
    server.startRxService();
    return 0;
}
