# egi_amp_cli — Headless EGI → LSL streamer

A minimal C++17 command‑line tool to read EEG from **EGI Net Amps (AmpServer)** and publish to **Lab Streaming Layer (LSL)** without any Qt dependency.

## Features
- Connects to AmpServer (command/notification/data sockets)
- Auto‑detects packet format (1/2) and channel count via net code
- Scales samples per amplifier type (NA300/400/410)
- Pushes float32 EEG to LSL with configurable stream name, sampling rate, and chunk size
- Config from XML (Boost.PropertyTree); CLI flags override config
- Clean shutdown on SIGINT/SIGTERM

## Build (CMake)
Requirements: CMake ≥3.15, C++17, liblsl, Boost (≥1.69.0 - uses header-only components: asio, property_tree, endian)

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### macOS notes (Homebrew)
If CMake cannot find Boost/LSL, help it with hints:
```bash
cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew;/usr/local" -DCMAKE_BUILD_TYPE=Release
```

**Note**: This project uses only header-only Boost libraries, so no separate boost library linking is required.

## Run
```bash
./egi_amp_cli --config amp.cfg   --stream-name "EGI NetAmp RoomA"   --sampling-rate 1000
```

All flags are optional if present in the config; CLI overrides XML.

## Example config (`amp.cfg`)
```xml
<root>
  <ampserver>
    <address>172.16.2.249</address>
    <commandport>9877</commandport>
    <notificationport>9878</notificationport>
    <dataport>9879</dataport>
  </ampserver>
  <settings>
    <amplifierid>0</amplifierid>
    <samplingrate>1000</samplingrate>
    <stream_name>EGI NetAmp Default</stream_name>
    <samples_per_chunk>32</samples_per_chunk>
  </settings>
</root>
```

## CLI
```
Usage: egi_amp_cli [options]
  -h [ --help ]                         Show help
  --config arg                          Path to XML config
  --address arg                         AmpServer IP address
  --cmd-port arg                        Command port
  --notif-port arg                      Notification port
  --data-port arg                       Data port
  --amp-id arg                          Amplifier ID
  --sampling-rate arg                   Sampling rate (Hz)
  --stream-name arg                     LSL stream name
  --samples-per-chunk arg               LSL samples per chunk
```

## Notes
- For sampling rates < 1000 Hz on packet format 2, AmpServer duplicates packets to keep 1000 packets/s; duplicates are ignored by `packetCounter` logic.
- Packet format 1 has no counter; we stream packets as they arrive.
- This tool **only** bridges EGI → LSL. Recording to BIDS should be done with a separate LSL recorder (e.g., LabRecorder CLI) or your own pipeline.
