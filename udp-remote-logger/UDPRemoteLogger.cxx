//
// Created by Zhiping Jiang on 2024/1/25.
//
#include <iostream>
#include <array>
#include <optional>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <PicoScenes/AsyncPipeline.hxx>
#include <PicoScenes/FrameDumper.hxx>
#include <PicoScenes/ModularPicoScenesFrame.hxx>

using boost::asio::ip::udp;

/**
 * \brief Receive UDP datagram from remote, parse the data to RxFrame data structure, print it, and dump it to a .csi file.
 */
class PicoScenesRxFrameUDPReceiver {
public:
    explicit PicoScenesRxFrameUDPReceiver(const uint16_t rxUDPPort, const std::optional<std::string>&outputPrefix = std::nullopt) : outputPrefix(outputPrefix) {
        std::cout << "Receiving PicoScenes Rx frames from port: " << rxUDPPort << " ..." << std::endl;
        if (outputPrefix)
            std::cout << "Output file prefix: " << *outputPrefix << std::endl;

        // This AsyncPipeline is used to decode the input bytes to frames
        decoderPipeline = std::make_shared<AsyncPipeline<std::vector<uint8_t>>>();
        decoderPipeline->registerAsyncHandler("RxFrameDecoder", [&](const std::vector<uint8_t>&bytes) {
            if (const auto decodedFrame = ModularPicoScenesRxFrame::fromBuffer(bytes.data(), bytes.size())) {
                // Delegate the parsed frame to frame dumping pipeline
                dumperPipeline->send(*decodedFrame);
            }
        }).startService();

        // This AsyncPipeline is used to dump the decoded frames.
        dumperPipeline = std::make_shared<AsyncPipeline<ModularPicoScenesRxFrame>>();
        dumperPipeline->registerAsyncHandler("FrameDumper", [&](const ModularPicoScenesRxFrame&rxframe) {
            std::cout << rxframe << std::endl;

            if (!outputPrefix)
                FrameDumper::getInstance("udp_logged")->dumpRxFrame(rxframe);
            else
                FrameDumper::getInstance(*outputPrefix)->dumpRxFrame(rxframe);
        }).startService();

        // Initializing the Boost ASIO UDP receiver...
        endPoint = udp::endpoint(udp::v4(), rxUDPPort);
        socket = std::make_shared<udp::socket>(io_service);
        socket->open(endPoint.protocol());
        socket->set_option(boost::asio::socket_base::receive_buffer_size(65536));
        socket->set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket->bind(endPoint);

        // start receiving
        activateUDPAsyncReceiving();
    }

    auto activateUDPAsyncReceiving() -> void {
        socket->async_receive_from(boost::asio::buffer(rxTempBuffer), endPoint, [this](const boost::system::error_code&error, std::size_t bytes_transferred) {
            if (!error)
                this->rxHandler(error, bytes_transferred);
            else
                std::cerr << error << std::endl;
        });
    }

    // This is the callback function invoked by socket->async_receive_from
    auto rxHandler(const boost::system::error_code&errorCode, const std::size_t bytesTransferred) -> void {
        if (!errorCode) {
            // Use dual-AsyncPipeline approach to prevent the possible UDP packet loss
            decoderPipeline->send(std::vector<uint8_t>(rxTempBuffer.data(), rxTempBuffer.data() + bytesTransferred));

            // re-activate the receiving
            activateUDPAsyncReceiving();
        }
    }

    /**
     * \brief Use a thread pool for high-performance UDP receiver, and block the main thread
     */
    auto startReceivingAndPostProcessingAndBlockingInvokerSide() -> int {
        std::vector<std::thread> threadPool;
        threadPool.reserve(std::thread::hardware_concurrency());
        for (auto i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threadPool.emplace_back([this] {
                io_service.run();
            });
        }

        // Use join to block the main thread...
        for (auto&thread: threadPool) {
            if (thread.joinable())
                thread.join();
        }

        return 0;
    }

private:
    // BOOST ASIO facilities
    boost::asio::io_service io_service;
    std::shared_ptr<udp::socket> socket;
    udp::endpoint endPoint;

    // Large buffer for large frames, e.g., super long frames with MIMO and large CBW
    std::array<uint8_t, 65536> rxTempBuffer{};

    // Two asynchronous pipelines used to prevent UDP blocking or packet loss
    std::shared_ptr<AsyncPipeline<std::vector<uint8_t>>> decoderPipeline;
    std::shared_ptr<AsyncPipeline<ModularPicoScenesRxFrame>> dumperPipeline;

    // User specified output file prefix
    std::optional<std::string> outputPrefix{std::nullopt};
};

auto main(const int argc, char* argv[]) -> int {
    auto parseProgramOptions_via_Boost_ProgramOptions = [&] {
        uint16_t rxPort{0};
        std::optional<std::string> outputPrefix{std::nullopt};
        boost::program_options::options_description optionSet("Options for PicoScenes UDP remove logger");
        optionSet.add_options()
                ("port", boost::program_options::value<int>(), "UDP Rx port, e.g., \"--port 12345\".")
                ("prefix", boost::program_options::value<std::string>(), "Prefix string for the logged .CSI file, e.g., \"--output test\".")
                ("help", "produce help message");

        boost::program_options::variables_map vm; // 用于存储选项和参数值的变量
        store(parse_command_line(argc, argv, optionSet), vm);
        notify(vm);

        if (vm.contains("port")) {
            rxPort = vm["port"].as<int>();
        }

        if (vm.contains("prefix")) {
            outputPrefix = vm["prefix"].as<std::string>();
        }

        if (vm.contains("help") || argc <= 1) {
            std::cout << optionSet << std::endl;
            exit(0);
        }

        return std::make_tuple(rxPort, outputPrefix);
    };
    auto [port, outputPrefix] = parseProgramOptions_via_Boost_ProgramOptions();

    return PicoScenesRxFrameUDPReceiver(port, outputPrefix).startReceivingAndPostProcessingAndBlockingInvokerSide();
}
