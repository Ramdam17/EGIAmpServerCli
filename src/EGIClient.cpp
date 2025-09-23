#include "EGIClient.hpp"
#include <lsl_cpp.h>
#include <boost/endian/conversion.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/version.hpp>
#include <chrono>
#include <iostream>
#include <regex>

using boost::asio::ip::tcp;
namespace ip = boost::asio::ip;
namespace pt = boost::property_tree;

#define SET_STREAM_EXPIRES_AFTER(stream, dur) stream.expires_after(dur)

void load_config_xml(const std::string& file, Config& cfg) {
  pt::ptree tree;
  read_xml(file, tree);
  cfg.address = tree.get<std::string>("root.ampserver.address", cfg.address);
  cfg.commandPort = tree.get<int>("root.ampserver.commandport", cfg.commandPort);
  cfg.notificationPort = tree.get<int>("root.ampserver.notificationport", cfg.notificationPort);
  cfg.dataPort = tree.get<int>("root.ampserver.dataport", cfg.dataPort);
  cfg.amplifierId = tree.get<int>("root.settings.amplifierid", cfg.amplifierId);
  cfg.samplingRate = tree.get<int>("root.settings.samplingrate", cfg.samplingRate);
  cfg.streamName = tree.get<std::string>("root.settings.stream_name", cfg.streamName);
  cfg.samplesPerChunk = tree.get<int>("root.settings.samples_per_chunk", cfg.samplesPerChunk);
}

EGIClient::EGIClient() {}
EGIClient::~EGIClient() {}

void EGIClient::connect(const Config& c) {
  std::cout << "[*] Connecting to AmpServer at " << c.address << " ..." << std::endl;

  // command
  commandStream_.clear();
  commandStream_.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
  SET_STREAM_EXPIRES_AFTER(commandStream_, std::chrono::seconds(2));
  commandStream_.connect(ip::tcp::endpoint(ip::make_address(c.address), c.commandPort));
  SET_STREAM_EXPIRES_AFTER(commandStream_, std::chrono::hours(8760));

  // notification
  notificationStream_.clear();
  SET_STREAM_EXPIRES_AFTER(notificationStream_, std::chrono::seconds(2));
  notificationStream_.connect(ip::tcp::endpoint(ip::make_address(c.address), c.notificationPort));

  // data
  dataStream_.clear();
  SET_STREAM_EXPIRES_AFTER(dataStream_, std::chrono::seconds(5));
  dataStream_.connect(ip::tcp::endpoint(ip::make_address(c.address), c.dataPort));

  if (!getAmplifierDetails(c.amplifierId)) {
    throw std::runtime_error("Failed to get amplifier details.");
  }

  initAmplifier(c.amplifierId, c.samplingRate);
  sendDatastreamCommand("cmd_ListenToAmp", std::to_string(c.amplifierId), "0", "0");

  // notifications thread
  notificationThread_ = std::make_unique<std::thread>([this]() {
    try {
      getNotifications([](){ return false; }); // will be ignored; we use stream-good break
    } catch (...) {}
  });
}

void EGIClient::disconnect(int ampId) {
  std::cout << "[*] Stopping stream..." << std::endl;
  try {
    sendDatastreamCommand("cmd_StopListeningToAmp", std::to_string(ampId), "0", "0");
    sendCommand("cmd_Stop", std::to_string(ampId), "0", "0");
    sendCommand("cmd_SetPower", std::to_string(ampId), "0", "0");
  } catch (...) {}
  if (notificationThread_ && notificationThread_->joinable()) notificationThread_->join();
  std::cout << "[*] Stream stopped." << std::endl;
}

std::string EGIClient::sendCommand(std::string command, std::string ampId, std::string channel, std::string value) {
  char response[4096];
  commandStream_ << "(sendCommand " << command << ' ' << ampId << ' ' << channel << ' ' << value << ")\n" << std::flush;
  commandStream_.getline(response, sizeof(response));
  return std::string(response);
}

void EGIClient::sendDatastreamCommand(std::string command, std::string ampId, std::string channel, std::string value) {
  dataStream_ << "(sendCommand " << command << ' ' << ampId << ' ' << channel << ' ' << value << ")\n" << std::flush;
}

bool EGIClient::getAmplifierDetails(int amplifierId) {
  try {
    std::string details = sendCommand("cmd_GetAmpDetails", std::to_string(amplifierId), "0", "0");
    std::cout << "=== Amplifier Details ===\n" << details << "\n=========================\n";

    std::regex token(R"(\((\w+)\s+([^()]+)\))");
    std::smatch match;
    while (std::regex_search(details, match, token)) {
      std::string key = match[1].str(), value = match[2].str();
      if (key.find("packet_format") != std::string::npos) {
        packetType_ = (PacketType)std::stoi(value);
        std::cout << "packet_format: " << (int)packetType_ << "\n";
      } else if (key.find("number_of_channels") != std::string::npos) {
        channelCount_ = (uint16_t)std::stoi(value);
        std::cout << "number_of_channels: " << channelCount_ << "\n";
      } else if (key.find("amp_type") != std::string::npos) {
        if (value.find("NA300") != std::string::npos) amplifierType_ = NA300;
        else if (value.find("NA400") != std::string::npos) amplifierType_ = NA400;
      } else if (key.find("legacy_board") != std::string::npos) {
        if (value.find("true") != std::string::npos && amplifierType_ == NAunknown) amplifierType_ = NA410;
      }
      details = match.suffix().str();
    }
    switch (amplifierType_) {
      case NA300: scalingFactor_ = 0.0244140625f; std::cout << "Amp: Net Amps 300\n"; break;
      case NA400: scalingFactor_ = 0.00015522042f; std::cout << "Amp: Net Amps 400\n"; break;
      case NA410: scalingFactor_ = 0.00009636188f; std::cout << "Amp: Net Amps 410\n"; break;
      default: break;
    }
    return true;
  } catch (...) { return false; }
}

void EGIClient::initAmplifier(int amplifierId, int samplingRate) {
  (void)sendCommand("cmd_Stop", std::to_string(amplifierId), "0", "0");
  (void)sendCommand("cmd_SetPower", std::to_string(amplifierId), "0", "0");
  (void)sendCommand("cmd_SetDecimatedRate", std::to_string(amplifierId), "0", std::to_string(samplingRate));
  (void)sendCommand("cmd_SetPower", std::to_string(amplifierId), "0", "1");
  (void)sendCommand("cmd_Start", std::to_string(amplifierId), "0", "0");
  (void)sendCommand("cmd_DefaultAcquisitionState", std::to_string(amplifierId), "0", "0");
  std::cout << "[*] Amplifier initialized. SamplingRate=" << samplingRate << "\n";
}

void EGIClient::getNotifications(const std::function<bool()>& stop_cb) {
  while (notificationStream_.good()) {
    SET_STREAM_EXPIRES_AFTER(notificationStream_, std::chrono::seconds(1));
    char response[4096];
    notificationStream_.getline(response, sizeof(response));
    if (std::string(response).length() > 0) {
      std::cout << "[Notification] " << response << "\n";
    }
    if (stop_cb && stop_cb()) break;
  }
}

int EGIClient::detectChannelsFromNetCode(uint8_t netCode) {
  switch ((NetCode)netCode) {
    case HCGSN32_1_0:
    case MCGSN32_1_0: return 32;
    case GSN64_2_0:
    case HCGSN64_1_0:
    case MCGSN64_1_0: return 64;
    case GSN128_2_0:
    case HCGSN128_1_0:
    case MCGSN128_1_0: return 128;
    case GSN256_2_0:
    case HCGSN256_1_0:
    case MCGSN256_1_0: return 256;
    default: return 0;
  }
}

void EGIClient::read_packet_format_2_loop(const Config& c, const std::function<bool()>& stop_cb) {
  std::unique_ptr<lsl::stream_outlet> outlet;
  int nChannels = channelCount_;
  bool firstPacketReceived = false;
  uint64_t lastPacketCounter = 0;

  std::cout << "[*] Starting stream (format 2)..." << std::endl;
  while (dataStream_.good()) {
    AmpDataPacketHeader header{};
    dataStream_.clear();
    SET_STREAM_EXPIRES_AFTER(dataStream_, std::chrono::seconds(5));
    dataStream_.read((char*)&header, sizeof(header));

    header.ampID = boost::endian::big_to_native(header.ampID);
    header.length = boost::endian::big_to_native(header.length);
    int nSamples = header.length / sizeof(PacketFormat2);

    for (int s = 0; s < nSamples && dataStream_.good(); s++) {
      PacketFormat2 packet{};
      dataStream_.read((char*)&packet, sizeof(PacketFormat2));

      if (!outlet) {
        int detected = detectChannelsFromNetCode(packet.netCode);
        if (detected > 0) nChannels = detected;
        std::string streamname = c.streamName.empty()
                               ? ("EGI NetAmp " + std::to_string(header.ampID))
                               : c.streamName;
        lsl::stream_info info(streamname, "EEG", nChannels, c.samplingRate,
                              lsl::cf_float32, streamname + "_at_" + c.address);
        auto acq = info.desc().append_child("acquisition");
        acq.append_child_value("manufacturer", "Philips Neuro");
        acq.append_child_value("model", "NetAmp");
        acq.append_child_value("application", "AmpServer");
        acq.append_child_value("precision", "24");
        outlet = std::make_unique<lsl::stream_outlet>(info, c.samplesPerChunk);
        std::cout << "[*] LSL outlet created: name='" << streamname
                  << "', ch=" << nChannels
                  << ", fs=" << c.samplingRate
                  << ", chunk=" << c.samplesPerChunk << "\n";
      }

      if (packet.packetCounter != 0 && packet.packetCounter != lastPacketCounter + 1 &&
          packet.packetCounter != lastPacketCounter && lastPacketCounter != 0) {
        std::cout << "[Warn] Packet(s) dropped: "
                  << (packet.packetCounter - lastPacketCounter) << "\n";
      } else if (firstPacketReceived && packet.packetCounter == lastPacketCounter) {
        continue; // duplicate
      }

      if (!firstPacketReceived) {
        std::cout << "[*] Stream started.\n";
        firstPacketReceived = true;
      }
      lastPacketCounter = packet.packetCounter;

      if (lastTimeStamp_ != 0 && (packet.packetCounter % (c.samplingRate / 2)) == 0) {
        lastTimeStamp_ = packet.timeStamp;
        lastPacketCounterWithTimeStamp_ = packet.packetCounter;
      } else if (lastTimeStamp_ == 0) {
        std::cout << "[*] First timestamp: " << packet.timeStamp << "\n";
        lastTimeStamp_ = packet.timeStamp;
        lastPacketCounterWithTimeStamp_ = packet.packetCounter;
      }

      std::vector<float> samples;
      samples.reserve(nChannels);
      for (int ch = 0; ch < nChannels; ch++) {
        samples.push_back(static_cast<float>(packet.eegData[ch]) * scalingFactor_);
      }
      outlet->push_sample(samples);
    }
    if (stop_cb && stop_cb()) break;
  }
  if (!dataStream_.good() && (!stop_cb || !stop_cb())) std::cerr << "[!] Stream lost.\n";
}

void EGIClient::read_packet_format_1_loop(const Config& c, const std::function<bool()>& stop_cb) {
  std::shared_ptr<lsl::stream_outlet> outlet;
  int nChannels = channelCount_;
  bool firstPacketReceived = false;

  std::cout << "[*] Starting stream (format 1)..." << std::endl;
  while (dataStream_.good()) {
    AmpDataPacketHeader header{};
    dataStream_.read((char*)&header, sizeof(header));
    header.ampID = boost::endian::big_to_native(header.ampID);
    header.length = boost::endian::big_to_native(header.length);

    int nSamples = header.length / sizeof(PacketFormat1);
    for (int s = 0; s < nSamples && dataStream_.good(); s++) {
      PacketFormat1 packet{};
      dataStream_.clear();
      SET_STREAM_EXPIRES_AFTER(dataStream_, std::chrono::seconds(1));
      dataStream_.read((char*)&packet, sizeof(PacketFormat1));

      if (!firstPacketReceived) {
        firstPacketReceived = true;
        std::cout << "[*] Stream started.\n";
        uint8_t* headerAsByteArray = reinterpret_cast<uint8_t*>(packet.header);
        uint8_t netCode = ((uint8_t)headerAsByteArray[26] & 0x78) >> 3;
        int detected = detectChannelsFromNetCode(netCode);
        if (detected > 0) nChannels = detected;

        std::string streamname = c.streamName.empty()
                               ? ("EGI NetAmp " + std::to_string(header.ampID))
                               : c.streamName;
        lsl::stream_info info(streamname, "EEG", nChannels, c.samplingRate,
                              lsl::cf_float32, streamname + "_at_" + c.address);
        auto acq = info.desc().append_child("acquisition");
        acq.append_child_value("manufacturer", "Philips Neuro");
        acq.append_child_value("model", "NetAmp");
        acq.append_child_value("application", "AmpServer");
        acq.append_child_value("precision", "24");
        outlet = std::make_shared<lsl::stream_outlet>(info, c.samplesPerChunk);
        std::cout << "[*] LSL outlet created: name='" << streamname
                  << "', ch=" << nChannels
                  << ", fs=" << c.samplingRate
                  << ", chunk=" << c.samplesPerChunk << "\n";
      }

      std::vector<float> sample;
      sample.reserve(nChannels);
      for (int i = 0; i < nChannels; i++) {
        float val = packet.eeg[i];
        boost::endian::big_to_native_inplace(*((uint32_t*)&val));
        sample.push_back(val);
      }
      outlet->push_sample(sample);
    }
    if (stop_cb && stop_cb()) break;
  }
  if (!dataStream_.good() && (!stop_cb || !stop_cb())) std::cerr << "[!] Stream lost.\n";
}

void EGIClient::run_stream(const Config& c, const std::function<bool()>& stop_cb) {
  if (packetType_ == packetType2) read_packet_format_2_loop(c, stop_cb);
  else read_packet_format_1_loop(c, stop_cb);
}
