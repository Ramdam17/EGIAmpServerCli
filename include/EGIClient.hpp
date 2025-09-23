#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>

// forward declare LSL type
namespace lsl { class stream_outlet; class stream_info; }

// ---- Config ----
struct Config {
  std::string address = "172.16.2.249";
  int commandPort = 9877;
  int notificationPort = 9878;
  int dataPort = 9879;
  int amplifierId = 0;
  int samplingRate = 1000;
  std::string streamName = "EGI NetAmp";
  int samplesPerChunk = 32;
};

void load_config_xml(const std::string& file, Config& cfg);

// ---- EGIClient ----
class EGIClient {
public:
  EGIClient();
  ~EGIClient();

  void connect(const Config& c);
  void disconnect(int ampId);

  // blocking stream loop; stop_cb() must return true to stop
  void run_stream(const Config& c, const std::function<bool()>& stop_cb);

private:
  enum PacketType { packetType1 = 1, packetType2 = 2 };
  enum AmplifierType { NAunknown, NA300, NA400, NA410 };
  enum NetCode : uint8_t {
    GSN64_2_0, GSN128_2_0, GSN256_2_0,
    HCGSN32_1_0, HCGSN64_1_0, HCGSN128_1_0, HCGSN256_1_0,
    MCGSN32_1_0, MCGSN64_1_0, MCGSN128_1_0, MCGSN256_1_0,
    TestConnector = 14, NoNet = 15, Unknown = 0xFF
  };

#pragma pack(push, 1)
  struct PacketFormat1 {
    uint32_t header[8];
    float eeg[256];
    float pib[7];
    float unused1;
    float ref;
    float com;
    float unused2;
    float padding[13];
  };

  struct PacketFormat2_PIB_AUX {
    uint8_t digitalInputs;
    uint8_t status;
    uint8_t batteryLevel[3];
    uint8_t temperature[3];
    uint8_t sp02;
    uint8_t heartRate[2];
  };

  struct PacketFormat2 {
    uint16_t digitalInputs;
    uint8_t tr;
    PacketFormat2_PIB_AUX pib1_aux;
    PacketFormat2_PIB_AUX pib2_aux;
    uint64_t packetCounter;
    uint64_t timeStamp;
    uint8_t netCode;
    uint8_t reserved[38];
    int32_t eegData[256];
    int32_t auxData[3];
    int32_t refMonitor;
    int32_t comMonitor;
    int32_t driveMonitor;
    int32_t diagnosticsChannel;
    int32_t currentSense;
    int32_t pib1_Data[16];
    int32_t pib2_Data[16];
  };

  struct AmpDataPacketHeader {
    int64_t ampID;
    uint64_t length;
  };
#pragma pack(pop)

  static int detectChannelsFromNetCode(uint8_t netCode);

  bool getAmplifierDetails(int amplifierId);
  void initAmplifier(int amplifierId, int samplingRate);

  std::string sendCommand(std::string command, std::string ampId, std::string channel, std::string value);
  void sendDatastreamCommand(std::string command, std::string ampId, std::string channel, std::string value);

  void getNotifications(const std::function<bool()>& stop_cb);
  void read_packet_format_2_loop(const Config& c, const std::function<bool()>& stop_cb);
  void read_packet_format_1_loop(const Config& c, const std::function<bool()>& stop_cb);

private:
  // sockets
  boost::asio::ip::tcp::iostream commandStream_;
  boost::asio::ip::tcp::iostream notificationStream_;
  boost::asio::ip::tcp::iostream dataStream_;

  // state
  AmplifierType amplifierType_ = NAunknown;
  PacketType packetType_ = packetType2;
  uint16_t channelCount_ = 0;
  float scalingFactor_ = 1.0f;

  // timing (format 2)
  uint64_t lastTimeStamp_ = 0;
  uint64_t lastPacketCounterWithTimeStamp_ = 0;

  std::unique_ptr<std::thread> notificationThread_;
};
