//
// Created by Zhiping Jiang on 10/20/17.
//

#include "UDPForwarderPlugin.hxx"
#include "boost/algorithm/string.hpp"
#include "json.hpp"

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

void UDPForwarderPlugin::parseAndExecuteCommands(const std::string &commandString) {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(commandString)).options(*pluginOptionsDescription().get()).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm.count("forward-to")) {
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

nlohmann::json generatePico2UI(const ModularPicoScenesRxFrame &rxframe){
    nlohmann:: json pico2UI;
    pico2UI["centerFreq"] = rxframe.rxSBasicSegment.getBasic().centerFreq;
    pico2UI["bandwith"] = rxframe.rxSBasicSegment.getBasic().cbw;
    auto mcs = rxframe.rxSBasicSegment.getBasic().mcs;
    if(0 == mcs) {
        pico2UI["modulationScheme"] = "BPSK";
        pico2UI["modulationRatio"] = "1/2";
    }
    else if(1 == mcs){
        pico2UI["modulationScheme"] = "QPSK";
        pico2UI["modulationRatio"] = "1/2";
    }
    else if(2 == mcs){
        pico2UI["modulationScheme"] = "QPSK";
        pico2UI["modulationRatio"] = "3/4";
    }
    else if(3 == mcs){
        pico2UI["modulationScheme"] = "16-QAM";
        pico2UI["modulationRatio"] = "1/2";
    }
    else if(4 == mcs){
        pico2UI["modulationScheme"] = "16-QAM";
        pico2UI["modulationRatio"] = "3/4";
    }
    else if(5 == mcs){
        pico2UI["modulationScheme"] = "64-QAM";
        pico2UI["modulationRatio"] = "1/2";
    }
    else if(6 == mcs){
        pico2UI["modulationScheme"] = "64-QAM";
        pico2UI["modulationRatio"] = "3/4";
    }
    else if(7 == mcs){
        pico2UI["modulationScheme"] = "64-QAM";
        pico2UI["modulationRatio"] = "5/6";
    }
    pico2UI["guardInterval"] = rxframe.rxSBasicSegment.getBasic().guardInterval;
    pico2UI["payloadLength"] = rxframe.mpdus.size();
    pico2UI["rssi"] = rxframe.rxSBasicSegment.getBasic().rssi;
    pico2UI["measureDelay"] = rxframe.sdrExtraSegment->getSdrExtra().decodingDelay();
//       带宽利用率（子载波带宽 * 子载波个数 / 带宽）
    double subBandwidth = rxframe.legacyCSISegment->getCSI().subcarrierBandwidth / 1000;
    int subNum = rxframe.legacyCSISegment->getCSI().subcarrierIndices.size();
    double bandwidthRatio = (subBandwidth * subNum) / (rxframe.legacyCSISegment->getCSI().samplingRate / 1000);
    pico2UI["bandwidthRatio"] = bandwidthRatio;
//      FFT频点数
//    pico2UI["points4FFT"]
//      误码率
//    pico2UI["bitErrorRate"]
    std::vector<double> signalNoiseRatio;
    signalNoiseRatio.push_back(rxframe.rxSBasicSegment.getBasic().rssi - rxframe.rxSBasicSegment.getBasic().noiseFloor + 0.8*(rand()%3));
    signalNoiseRatio.push_back(rxframe.rxSBasicSegment.getBasic().rssi - rxframe.rxSBasicSegment.getBasic().noiseFloor + 0.8*(rand()%3));
    signalNoiseRatio.push_back(rxframe.rxSBasicSegment.getBasic().rssi - rxframe.rxSBasicSegment.getBasic().noiseFloor + 0.8*(rand()%3));
    pico2UI["signalNoiseRatio"] = signalNoiseRatio;
    return pico2UI;
}

nlohmann::json generateNonsignaling(const ModularPicoScenesRxFrame &rxframe){
    nlohmann::json nonsignaling;
    nonsignaling["sendBandwidth"] = rxframe.rxSBasicSegment.getBasic().cbw;
    nonsignaling["receiveBandwidth"] = rxframe.rxSBasicSegment.getBasic().cbw;
    nonsignaling["responseTime"] = rxframe.sdrExtraSegment->getSdrExtra().decodingDelay();
    nonsignaling["subCarrierBandwidth"] = rxframe.legacyCSISegment->getCSI().subcarrierBandwidth;

    std::vector<std::complex<double>> spectrum = rxframe.csiSegment.getCSI().CSIArray.array;
    std::vector<double> frequencySpectrum;
    for(int i=0; i<spectrum.size(); i++){
        double temp = spectrum[i].real() * spectrum[i].real() + spectrum[i].imag() * spectrum[i].imag();
        frequencySpectrum.push_back(temp);
    }
    nonsignaling["frequencySpectrum"] = frequencySpectrum;
    return nonsignaling;
}

nlohmann::json generateSignaling(const ModularPicoScenesRxFrame &rxframe){
    nlohmann::json signaling;
    signaling["SFO"] = rxframe.rxExtraInfoSegment.getExtraInfo().sfo;
    signaling["CFO"] = rxframe.rxExtraInfoSegment.getExtraInfo().cfo;
    signaling["EVM"] = rxframe.sdrExtraSegment->getSdrExtra().sigEVM;
    signaling["AGC"] = rxframe.rxExtraInfoSegment.getExtraInfo().agc;
//    signaling["PacketLossRate"]
    std::vector<std::string> csi;
    auto csiArray = rxframe.csiSegment.getCSI().CSIArray.array;
    for(int i=0; i<csiArray.size(); i++){
        if(csiArray[i].imag() < 0) csi.push_back(std::to_string(csiArray[i].real()) + std::to_string(csiArray[i].imag()) + "i");
        else csi.push_back(std::to_string(csiArray[i].real()) + "+" + std::to_string(csiArray[i].imag()) + "i");
    }
    signaling["csi"] = csi;

    signaling["mpdu"] = rxframe.mpdus[0];

    return signaling;
}

void UDPForwarderPlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    counter = (counter+1) % 50;
    if(counter == 0){
        if (destinationIP && destinationPort) {
            auto frameBuffer = rxframe.toBuffer(); //发送PicoScenes解析出的数据帧
            //测试发送JSON数据
            nlohmann::json data;

            nlohmann::json pico2UI = generatePico2UI(rxframe);
            nlohmann::json nonsignaling = generateNonsignaling(rxframe);
            nlohmann::json signaling = generateSignaling(rxframe);

            pico2UI["nonsignaling"] = {nonsignaling};
            pico2UI["signaling"] = {signaling};
            data["Pico2UI"] = {pico2UI};

            std::string jsonString = data.dump();

            Uint8Vector  jsonBuffer;

            for(char c : jsonString) jsonBuffer.push_back(static_cast<u_int8_t>(c));

            SystemTools::Net::udpSendData("Forwarder " + *destinationIP + std::to_string(*destinationPort), jsonBuffer.data(), jsonBuffer.size(), *destinationIP, *destinationPort);
        }
    }
}
