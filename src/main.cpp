#include "EGIClient.hpp"
#include <csignal>
#include <iostream>
#include <map>
#include <string>

static std::atomic_bool g_stop{false};
static void handle_signal(int){ g_stop = true; }

static void print_help() {
  std::cout << "egi_amp_cli options:\n"
            << "  --help                         Show help\n"
            << "  --config PATH                  Path to XML config\n"
            << "  --address IP                   AmpServer IP address\n"
            << "  --cmd-port N                   Command port\n"
            << "  --notif-port N                 Notification port\n"
            << "  --data-port N                  Data port\n"
            << "  --amp-id N                     Amplifier ID\n"
            << "  --sampling-rate N              Sampling rate (Hz)\n"
            << "  --stream-name NAME             LSL stream name\n"
            << "  --samples-per-chunk N          LSL samples per chunk\n";
}

int main(int argc, char** argv) {
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  Config cfg;
  std::string config_path;

  // Minimal CLI parser (no Boost.Program_options)
  auto get_arg = [&](const std::string& key)->const char*{
    for (int i=1; i<argc-1; ++i) {
      if (std::string(argv[i]) == key) return argv[i+1];
    }
    return nullptr;
  };
  auto has_flag = [&](const std::string& key)->bool{
    for (int i=1; i<argc; ++i) if (std::string(argv[i]) == key) return true;
    return false;
  };

  if (has_flag("--help") || has_flag("-h")) { print_help(); return 0; }

  if (const char* p = get_arg("--config")) {
    config_path = p;
    load_config_xml(config_path, cfg);
  }
  if (const char* p = get_arg("--address")) cfg.address = p;
  if (const char* p = get_arg("--cmd-port")) cfg.commandPort = std::stoi(p);
  if (const char* p = get_arg("--notif-port")) cfg.notificationPort = std::stoi(p);
  if (const char* p = get_arg("--data-port")) cfg.dataPort = std::stoi(p);
  if (const char* p = get_arg("--amp-id")) cfg.amplifierId = std::stoi(p);
  if (const char* p = get_arg("--sampling-rate")) cfg.samplingRate = std::stoi(p);
  if (const char* p = get_arg("--stream-name")) cfg.streamName = p;
  if (const char* p = get_arg("--samples-per-chunk")) cfg.samplesPerChunk = std::stoi(p);

  try {
    EGIClient client;
    client.connect(cfg);

    std::cout << "[*] Streaming. Press Ctrl+C to stop.\n";
    client.run_stream(cfg, [&](){ return g_stop.load(); });

    client.disconnect(cfg.amplifierId);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << "\n";
    return 1;
  }
}
