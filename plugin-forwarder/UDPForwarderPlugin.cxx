//
// Created by Zhiping Jiang on 10/20/17.
//

#include "UDPForwarderPlugin.hxx"
#include "boost/algorithm/string.hpp"
#include "json.hpp"
#include <random>

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
            ("forward-to", po::value<std::string>(), "Destination address and port, e.g., 192.168.10.1:50000")
            ("receiveCBW", po::value<std::string>(), "CBW of the USRP sampling ratio, e.g., 20");
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
    if (vm.count("receiveCBW")) {
        receiveCBW = vm["receiveCBW"].as<std::string>();
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
    pico2UI["points4FFT"] = 64;
//    pico2UI["constellationData"] = rxframe.constellationData;
    std::vector<double> signalNoiseRatio;
    signalNoiseRatio.push_back( rxframe.rxSBasicSegment.getBasic().rssi - rxframe.rxSBasicSegment.getBasic().noiseFloor + 0.8*(rand()%3));
    signalNoiseRatio.push_back( rxframe.rxSBasicSegment.getBasic().rssi - rxframe.rxSBasicSegment.getBasic().noiseFloor + 0.8*(rand()%3));
    signalNoiseRatio.push_back( rxframe.rxSBasicSegment.getBasic().rssi - rxframe.rxSBasicSegment.getBasic().noiseFloor + 0.8*(rand()%3));
    pico2UI["signalNoiseRatio"] = signalNoiseRatio;
    pico2UI["mpdu"] = rxframe.mpdus[0];
    return pico2UI;
}

nlohmann::json generateNonsignaling(const ModularPicoScenesRxFrame &rxframe){
    nlohmann::json nonsignaling;
    nonsignaling["sendBandwidth"] = rxframe.rxSBasicSegment.getBasic().cbw;
    nonsignaling["receiveBandwidth"] = rxframe.rxSBasicSegment.getBasic().cbw;
    nonsignaling["responseTime"] = rxframe.sdrExtraSegment->getSdrExtra().decodingDelay();
    nonsignaling["subCarrierBandwidth"] = rxframe.legacyCSISegment->getCSI().subcarrierBandwidth;


    nonsignaling["centerPoint"] = rxframe.rxSBasicSegment.getBasic().centerFreq;

    nonsignaling["SFO"] = rxframe.rxExtraInfoSegment.getExtraInfo().sfo;
    nonsignaling["CFO"] = rxframe.rxExtraInfoSegment.getExtraInfo().cfo;
    nonsignaling["EVM"] = rxframe.sdrExtraSegment->getSdrExtra().sigEVM;
    nonsignaling["AGC"] = rxframe.rxExtraInfoSegment.getExtraInfo().agc;
    return nonsignaling;
}

nlohmann::json generateSignaling(const ModularPicoScenesRxFrame &rxframe, int numErrorFrame, int numTotalFrame){
    nlohmann::json signaling;
    signaling["SFO"] = rxframe.rxExtraInfoSegment.getExtraInfo().sfo;
    signaling["CFO"] = rxframe.rxExtraInfoSegment.getExtraInfo().cfo;
    signaling["EVM"] = rxframe.sdrExtraSegment->getSdrExtra().sigEVM;
    signaling["AGC"] = rxframe.rxExtraInfoSegment.getExtraInfo().agc;
    signaling["errorFrame"] = numErrorFrame;
    signaling["totalFrame"] = numTotalFrame;
    std::vector<std::string> csi;
    auto csiArray = rxframe.csiSegment.getCSI().CSIArray.array;
    for(int i=0; i<csiArray.size(); i++){
        if(csiArray[i].imag() < 0) csi.push_back(std::to_string(csiArray[i].real()) + std::to_string(csiArray[i].imag()) + "i");
        else csi.push_back(std::to_string(csiArray[i].real()).substr(0, 5) + "+" + std::to_string(csiArray[i].imag()) + "i");
    }
//    signaling["csi"] = csi;

    signaling["mpdu"] = rxframe.mpdus[0];

    return signaling;
}

void UDPForwarderPlugin::rxHandle(const ModularPicoScenesRxFrame &rxframe) {
    counter = (counter+1) % 50;
    numTotalFrame++;
    if(rxframe.errorFrame) numErrorFrame++;
    if(counter == 0){
        if (destinationIP && destinationPort) {
            //测试发送JSON数据
            nlohmann::json data;

            double BER = (double)numErrorFrame / numTotalFrame;

            nlohmann::json pico2UI = generatePico2UI(rxframe);
            nlohmann::json nonsignaling = generateNonsignaling(rxframe);
//            nlohmann::json signaling = generateSignaling(rxframe, numErrorFrame, numTotalFrame);
            pico2UI["BER"] = BER;

            std::vector<std::complex<double>> spectrum = rxframe.csiSegment.getCSI().CSIArray.array;
            std::vector<double> frequencySpectrum;
            double noiseFloor = rxframe.rxSBasicSegment.getBasic().noiseFloor;
            int leftBound = rxframe.rxSBasicSegment.getBasic().centerFreq - std::stoi(receiveCBW)/2;
            int rightBound = rxframe.rxSBasicSegment.getBasic().centerFreq + std::stoi(receiveCBW)/2;
            int numPoints = std::stoi(receiveCBW)*1000 / 312.5;
            int halfNumPoints = (numPoints - spectrum.size()) / 2;

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0, 2.0);
            int index = 1;
            for(int i=0; i<numPoints; i++){
                double temp;
                if(i<halfNumPoints || i>=halfNumPoints+spectrum.size()-1){
                    temp = noiseFloor + dis(gen);
                    temp = (int)(temp * 10000) / 10000.0;
                    frequencySpectrum.push_back(temp);
                }
                else{
                    double realNum = (int)(spectrum[index].real() * 100000) / 100000.0;
                    double imagNum = (int)(spectrum[index].imag() * 100000) / 100000.0;
                    index++;
                    temp = sqrt(realNum * realNum + imagNum * imagNum);
                    temp = 20 * log10(temp);
                    temp = (int)(temp * 10000) / 10000.0;
                    frequencySpectrum.push_back(temp);
                }
            }
            nonsignaling["leftBound"] = leftBound;
            nonsignaling["rightBound"] = rightBound;
            nonsignaling["frequencySpectrum"] = frequencySpectrum;

            pico2UI["nonsignaling"] = {nonsignaling};
            data["Pico2UI"] = {pico2UI};


            std::string jsonString = data.dump();

            Uint8Vector  jsonBuffer;

            for(char c : jsonString) jsonBuffer.push_back(static_cast<u_int8_t>(c));

            SystemTools::Net::udpSendData("Forwarder " + *destinationIP + std::to_string(*destinationPort), jsonBuffer.data(), jsonBuffer.size(), *destinationIP, *destinationPort);
        }
    }
}
