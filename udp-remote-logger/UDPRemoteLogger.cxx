//
// Created by Zhiping Jiang on 2024/1/25.
//
#include <iostream>
#include <array>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <PicoScenes/AsyncPipeline.hxx>
#include <PicoScenes/FrameDumper.hxx>
#include <PicoScenes/ModularPicoScenesFrame.hxx>

using boost::asio::ip::udp;
namespace po = boost::program_options;

class PicoScenesRxFrameUDPServer {
public:
    explicit PicoScenesRxFrameUDPServer(const uint16_t port, const std::optional<std::string>& outputPrefix = std::nullopt) : rxSocket(io_service, udp::endpoint(udp::v4(), port)), outputPrefix(outputPrefix) {
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
            std::cout << rxframe << std::endl;

            if (!outputPrefix)
                FrameDumper::getInstance("udp_logged")->dumpRxFrame(rxframe);
            else
                FrameDumper::getInstance(*outputPrefix)->dumpRxFrame(rxframe);
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
        } else {
            std::cerr << "Error occurs for Rx packet [" << bytesTransferred << "] bytes.\n";
        }
    }

    void startRxService() {
        io_service.run();
    }

private:
    // BOOST ASIO facilities
    boost::asio::io_service io_service;
    udp::socket rxSocket;
    udp::endpoint rxRemoteEndPoint;

    // Large buffer for large frames, e.g., super long frames with MIMO and large CBW
    std::array<uint8_t, 10000000> rxBuffer{};

    // Two asynchronous pipelines used to prevent UDP blocking or packet loss
    std::shared_ptr<AsyncPipeline<std::vector<uint8_t>>> rxBytesPipeline;
    std::shared_ptr<AsyncPipeline<ModularPicoScenesRxFrame>> rxFramePipeline;

    std::optional<std::string> outputPrefix{std::nullopt};
};

int main(const int argc, char *argv[]) {
    uint16_t rxPort{0};
    std::optional<std::string> outputPrefix{std::nullopt};

    // 定义要解析的命令行选项
    po::options_description optionSet("Options for PicoScenes UDP remove logger");
    optionSet.add_options()
        ("port", po::value<int>(), "UDP Rx port, e.g., \"--port 12345\".")
        ("output", po::value<std::string>(), "Prefix string for the logged .CSI file, e.g., \"--output test\".")
        ("help", "produce help message");

    po::variables_map vm;  // 用于存储选项和参数值的变量
    store(parse_command_line(argc, argv, optionSet), vm);
    notify(vm);

    if (vm.contains("port")) {
        rxPort = vm["port"].as<int>();
    }

    if (vm.contains("output")) {
        outputPrefix = vm["port"].as<std::string>();
    }

    if (vm.contains("help")) {
        std::cout << optionSet << std::endl;
        return 0;
    }

    PicoScenesRxFrameUDPServer server(rxPort, outputPrefix);
    server.startRxService();
    return 0;
}
