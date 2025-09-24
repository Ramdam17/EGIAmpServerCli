# egi_amp_cli — Headless EGI → LSL streamer

A robust C++17 command‑line tool to read EEG from **EGI Net Amps (AmpServer)** and publish to **Lab Streaming Layer (LSL)** without any Qt dependency.

## Attribution

This project is based on the [EGI AmpServer LSL App](https://github.com/labstreaminglayer/App-EGIAmpServer) from the Lab Streaming Layer project. The vast majority of the EGI protocol implementation and packet processing code originates from that project. This version removes the Qt GUI dependency and provides a command-line interface for headless operation.

## Features
- **Robust Connection Management**: Connects to AmpServer (command/notification/data sockets) with automatic reconnection
- **Protocol Compliance**: Follows the exact initialization sequence from the original Qt GUI version
- **Auto-Detection**: Auto‑detects packet format (1/2) and channel count via net code
- **Multi-Amplifier Support**: Scales samples per amplifier type (NA300/400/410)
- **LSL Integration**: Pushes float32 EEG to LSL with configurable stream name, sampling rate, and chunk size
- **Flexible Configuration**: Config from XML (Boost.PropertyTree); CLI flags override config
- **Production Ready**: Clean shutdown on SIGINT/SIGTERM with comprehensive error reporting
- **Stream Recovery**: Automatic reconnection attempts (up to 5) when data streams are lost

## Build (CMake)
Requirements: CMake ≥3.15, C++17, liblsl, Boost (≥1.69.0 - uses header-only components: asio, property_tree, endian)

### Windows (vcpkg)
1. Install dependencies with vcpkg:
   ```powershell
   vcpkg install boost:x64-windows liblsl:x64-windows
   ```

2. Build the project:
   ```powershell
   mkdir build
   cd build
   cmake -DCMAKE_TOOLCHAIN_FILE="[path-to-vcpkg]\scripts\buildsystems\vcpkg.cmake" ..
   cmake --build . --config Release
   ```

### Linux/macOS
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### macOS notes (Homebrew)
If CMake cannot find Boost/LSL, help it with hints:
```bash
cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew;/usr/local" -DCMAKE_BUILD_TYPE=Release
```

**Note**: This project uses header-only Boost libraries (asio, property_tree) and links to liblsl.

### Build Outputs
- **Debug**: `build/Debug/egi_amp_cli.exe` (Windows) or `build/egi_amp_cli` (Linux/macOS)
- **Release**: `build/Release/egi_amp_cli.exe` (Windows) or `build/egi_amp_cli` (Linux/macOS)

## Run
### Windows
```powershell
# From build directory
.\Release\egi_amp_cli.exe --config ..\amp.cfg --stream-name "EGI NetAmp RoomA" --sampling-rate 1000
```

### Linux/macOS
```bash
./egi_amp_cli --config amp.cfg --stream-name "EGI NetAmp RoomA" --sampling-rate 1000
```

All flags are optional if present in the config; CLI overrides XML.

## Example config (`amp.cfg`)
```xml
<root>
  <ampserver>
    <address>10.10.10.51</address>
    <commandport>9877</commandport>
    <notificationport>9878</notificationport>
    <dataport>9879</dataport>
  </ampserver>
  <settings>
    <amplifierid>0</amplifierid>
    <samplingrate>1000</samplingrate>
    <stream_name>EGI NetAmp 51</stream_name>
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

## Troubleshooting

### Connection Issues
If you see `(sendCommand_return (status error))` responses:
1. **Check Amplifier ID**: Most EGI amplifiers use ID `0`, not higher numbers
2. **Verify Hardware**: Ensure the EGI NetAmp is powered on and connected to AmpServer
3. **Test with Official Software**: Verify the AmpServer can connect to your amplifier first
4. **Check Network**: Ensure the specified IP address is reachable

### Stream Loss Recovery
The application automatically attempts to reconnect up to 5 times when data streams are lost. Look for messages like:
```
[!] Stream lost. Attempting to reconnect (1/5)...
[*] Reconnection attempt successful. Resuming stream...
```

### Verbose Output
The application provides detailed initialization output showing the response to each amplifier command:
```
[*] Stop: (sendCommand_return (status complete))
[*] SetPower (Off): (sendCommand_return (status complete))
[*] SetDecimatedRate: (sendCommand_return (status complete))
...
```

## Automated Recording Setup

This repository includes a Python automation script (`LabrecorderAuto/`) that provides:
- **Multi-Amplifier Management**: Automatically starts multiple EGI Amp Server instances
- **Network Connectivity Checks**: Verifies amplifiers are reachable before connecting
- **LSL Stream Validation**: Ensures streams are publishing actual data before proceeding
- **LabRecorder Integration**: Automatically launches LabRecorder and starts recording
- **Process Monitoring**: Monitors EGI processes and can restart them if they crash


## Notes
- For sampling rates < 1000 Hz on packet format 2, AmpServer duplicates packets to keep 1000 packets/s; duplicates are ignored by `packetCounter` logic.
- Packet format 1 has no counter; we stream packets as they arrive.
- This tool **only** bridges EGI → LSL. Recording to BIDS should be done with a separate LSL recorder (e.g., LabRecorder CLI) or your own pipeline.
- **Amplifier ID**: Most EGI NetAmp devices use amplifier ID `0` regardless of their IP address or physical labeling.
