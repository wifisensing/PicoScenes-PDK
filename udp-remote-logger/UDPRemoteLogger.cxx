//
// Created by Zhiping Jiang on 2024/1/25.
//
#include <iostream>
#include <array>
#include <map>
#include <optional>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <PicoScenes/AsyncPipeline.hxx>
#include <PicoScenes/FrameDumper.hxx>
#include <PicoScenes/ModularPicoScenesFrame.hxx>
#include "../plugin-forwarder/UDPForwardingHeader.hxx"

using boost::asio::ip::udp;

/**
 * \brief Receive UDP datagram from remote, parse the data to RxFrame data structure, print it, and dump it to a .csi file.
 */
class PicoScenesRxFrameUDPReceiver {
public:
    explicit PicoScenesRxFrameUDPReceiver(const uint16_t rxUDPPort, const std::optional<std::string>& outputPrefix = std::nullopt) : outputPrefix(outputPrefix) {
        std::cout << "Receiving PicoScenes Rx frames from port: " << rxUDPPort << " ..." << std::endl;
        if (outputPrefix)
            std::cout << "Output file prefix: " << *outputPrefix << std::endl;

        // This AsyncPipeline is used to decode the input bytes to frames
        udpDiagramReceivingPipeline = std::make_shared<AsyncPipeline<std::vector<uint8_t>>>();
        udpDiagramReceivingPipeline->registerAsyncHandler("RxFrameDecoder", [&](const std::vector<uint8_t>& bytes) {
            if (const auto metaHeader = PicoScenesFrameUDPForwardingDiagramHeader::fromBuffer(bytes.data())) {
                // Cut through to dumper if only one diagram
                if (metaHeader->numDiagrams == 1 && metaHeader->diagramId == 0) {
                    if (const auto decodedFrame = ModularPicoScenesRxFrame::fromBuffer(bytes.data() + sizeof(PicoScenesFrameUDPForwardingDiagramHeader), bytes.size() - sizeof(PicoScenesFrameUDPForwardingDiagramHeader))) {
                        // Delegate the parsed frame to frame dumping pipeline
                        dumperPipeline->send(*decodedFrame);
                    }
                } else {
                    frameConcatPipeline->send(std::make_pair(*metaHeader, U8Vector(bytes.data() + sizeof(PicoScenesFrameUDPForwardingDiagramHeader), bytes.data() + bytes.size())));
                }
            }
        }).startService();

        // This pipeline buffers the frame segments, concat them (if all segments are collected), and delegate the concatenated frame to dumperPipeline
        frameConcatPipeline = std::make_shared<AsyncPipeline<std::pair<PicoScenesFrameUDPForwardingDiagramHeader, U8Vector>>>();
        frameConcatPipeline->registerAsyncHandler("FrameConcat", [&](const std::pair<PicoScenesFrameUDPForwardingDiagramHeader, U8Vector>& headerAndBuffer) {
            if (const auto& [header, buffer] = headerAndBuffer; !frameConcatBuffer.contains(header.diagramTaskId)) {
                frameConcatBuffer.emplace(header.diagramTaskId, std::make_pair(std::vector{std::make_pair(header, buffer)}, 0));
            } else {
                auto& queue = frameConcatBuffer[header.diagramTaskId];
                queue.first.emplace_back(header, buffer);
                std::sort(queue.first.begin(), queue.first.end(), [](const auto& x, const auto& y) {
                    return x.first.diagramId < y.first.diagramId;
                });
            }

            std::vector<uint16_t> toBeRemovedTasks;
            for (const auto& task: frameConcatBuffer) {
                if (task.second.first[0].first.numDiagrams == task.second.first.size()) {
                    auto mergeBuffer = U8Vector{};
                    mergeBuffer.reserve(task.second.first[0].first.totalDiagramLength);
                    for (const auto& segment: task.second.first) {
                        std::copy(segment.second.cbegin(), segment.second.cend(), std::back_inserter(mergeBuffer));
                    }

                    if (const auto decodedFrame = ModularPicoScenesRxFrame::fromBuffer(mergeBuffer.data(), mergeBuffer.size())) {
                        // Delegate the parsed frame to frame dumping pipeline
                        dumperPipeline->send(*decodedFrame);
                    }
                    toBeRemovedTasks.emplace_back(task.first);
                }
            }

            for (auto& task: frameConcatBuffer) {
                task.second.second++;
                if (task.second.second > 100)
                    toBeRemovedTasks.emplace_back(task.first);
            }

            for (const auto& task: toBeRemovedTasks)
                if (frameConcatBuffer.contains(task))
                    frameConcatBuffer.erase(task);
        }).startService();

        // This AsyncPipeline is used to dump the decoded frames.
        dumperPipeline = std::make_shared<AsyncPipeline<ModularPicoScenesRxFrame>>();
        dumperPipeline->registerAsyncHandler("FrameDumper", [&](const ModularPicoScenesRxFrame& rxframe) {
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
        socket->async_receive_from(boost::asio::buffer(rxTempBuffer), endPoint, [this](const boost::system::error_code& error, const std::size_t bytes_transferred) {
            if (!error)
                this->rxHandler(error, bytes_transferred);
            else
                std::cerr << error << std::endl;
        });
    }

    // This is the callback function invoked by socket->async_receive_from
    auto rxHandler(const boost::system::error_code& errorCode, const std::size_t bytesTransferred) -> void {
        if (!errorCode) {
            // Use async pipeline approach to prevent the possible UDP packet loss
            udpDiagramReceivingPipeline->send(std::vector<uint8_t>(rxTempBuffer.data(), rxTempBuffer.data() + bytesTransferred));

            // re-activate the UDP receiving
            activateUDPAsyncReceiving();
        } else
            LoggingService_Plugin_error_print("Rx error: {}", errorCode.message());
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
        for (auto& thread: threadPool) {
            if (thread.joinable())
                thread.join();
        }

        return 0;
    }

private:
    // BOOST ASIO facilities, used for UDP data receiving
    boost::asio::io_service io_service;
    std::shared_ptr<udp::socket> socket;
    udp::endpoint endPoint;

    // Rx temp buffer for UDP diagram receiving
    std::array<uint8_t, 65536> rxTempBuffer{};

    // This pipeline forwards the received bytes to frameConcatPipeline to prevent the blocking of UDP receiving
    std::shared_ptr<AsyncPipeline<std::vector<uint8_t>>> udpDiagramReceivingPipeline;
    // This pipeline used for concatnating multiple UDP diagrams
    std::shared_ptr<AsyncPipeline<std::pair<PicoScenesFrameUDPForwardingDiagramHeader, U8Vector>>> frameConcatPipeline;
    // This pipeline is used for dumping the decoded frame into .csi file
    std::shared_ptr<AsyncPipeline<ModularPicoScenesRxFrame>> dumperPipeline;

    // A large data structure used for frame concatenation. frameConcatBuffer.first holds the taskId, frameConcatBuffer.second.first holds the received segments, and frameConcatBuffer.second.second hold the live count
    std::map<uint16_t, std::pair<std::vector<std::pair<PicoScenesFrameUDPForwardingDiagramHeader, U8Vector>>, int>> frameConcatBuffer;

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

        if (vm.count("port")) {
            rxPort = vm["port"].as<int>();
        }

        if (vm.count("prefix")) {
            outputPrefix = vm["prefix"].as<std::string>();
        }

        if (vm.count("help") || argc <= 1) {
            std::cout << optionSet << std::endl;
            exit(0);
        }

        return std::make_tuple(rxPort, outputPrefix);
    };
    auto [port, outputPrefix] = parseProgramOptions_via_Boost_ProgramOptions();

    return PicoScenesRxFrameUDPReceiver(port, outputPrefix).startReceivingAndPostProcessingAndBlockingInvokerSide();
}
