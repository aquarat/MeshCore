# MeshCore GitHub Copilot Instructions

**ALWAYS follow these instructions first.** Only search for additional information or use bash commands if these instructions are incomplete or found to be incorrect.

## Working Effectively

### System Requirements
- **Operating System**: Linux, macOS, or Windows with WSL
- **Python**: Python 3.7+ with pip
- **Internet connection**: Required for initial toolchain and library downloads
- **Disk space**: 3-5 GB for toolchains and build artifacts
- **Memory**: 4+ GB RAM recommended for compilation

### Bootstrap and Build Process
- **CRITICAL**: Always set up the Python virtual environment first:
  ```bash
  # From parent directory of MeshCore repository
  python3 -m venv meshcore
  cd meshcore && source bin/activate
  pip install -U platformio
  cd ../MeshCore
  ```

- **Build firmware**: Use PlatformIO with specific environment targets:
  ```bash
  source ../meshcore/bin/activate  # Always activate virtual environment first
  pio run -e <environment_name>
  ```
  **NEVER CANCEL**: First-time builds take 15-45 minutes due to toolchain downloads. Set timeout to 60+ minutes.

- **Dependency resolution**: Update packages when needed:
  ```bash
  source ../meshcore/bin/activate
  pio pkg update  # Takes 2-5 minutes, resolves library conflicts
  ```

- **Available build environments**: 186 total environments including:
  - `RAK_4631_Repeater`, `RAK_4631_companion_radio_usb`, `RAK_4631_room_server`
  - `Heltec_v3_repeater`, `Heltec_v3_companion_radio_usb`, `Heltec_v3_companion_radio_ble`
  - `LilyGo_T3S3_sx1262_Repeater`, `Xiao_C3_Repeater_sx1262`
  - Use `pio project config | grep "env:"` to see all available environments

- **Build scripts**: Use the provided build script for batch operations:
  ```bash
  # IMPORTANT: Activate virtual environment first, then export to PATH
  source ../meshcore/bin/activate
  export PATH="$(pwd)/../meshcore/bin:$PATH"
  
  # Set firmware version before building
  export FIRMWARE_VERSION="v1.8.1"
  ./build.sh build-firmware RAK_4631_Repeater
  ./build.sh build-companion-firmwares    # NEVER CANCEL: Takes 2-3 hours
  ./build.sh build-repeater-firmwares     # NEVER CANCEL: Takes 2-3 hours  
  ./build.sh build-room-server-firmwares  # NEVER CANCEL: Takes 2-3 hours
  ```

### Platform Support
- **ESP32**: Most common platform (Heltec, LilyGo, Xiao variants)
- **NRF52**: Nordic boards (RAK4631, Xiao NRF52)
- **RP2040**: Raspberry Pi Pico variants
- **STM32**: STM32-based boards

## Validation and Testing

### Build Validation
- **ALWAYS verify build success**: Check for firmware files in `.pio/build/<environment>/`
  - ESP32: `firmware.bin` and `firmware-merged.bin`
  - NRF52: `firmware.zip` and `firmware.uf2`
  - RP2040: `firmware.uf2`
  - STM32: `firmware.hex` and `firmware.bin`
  - Verify files exist before proceeding
  
- **Output location**: Built firmware files are also copied to `out/` directory by build script
  - Format: `<environment>-<version>-<commit>.bin` (e.g., `RAK_4631_Repeater-v1.8.1-abc123.bin`)

### Manual Testing Scenarios
- **CRITICAL**: You cannot run the actual firmware in the development environment
- **Hardware-specific testing**: MeshCore requires actual LoRa hardware to function
- **Serial interface testing**: When firmware is flashed, connect via serial monitor at 115200 baud
- **Basic validation commands** (when connected to actual hardware):
  ```
  info          # Shows firmware version and build date
  neighbors     # Lists neighboring nodes
  config node.name "TestNode"  # Sets node name
  ```

### Build Time Expectations
- **NEVER CANCEL builds or tests**: Build times are extended due to toolchain downloads
- **First build**: 15-45 minutes (toolchain download + compilation)
- **Subsequent builds**: 3-8 minutes (compilation only)
- **Full firmware suite**: 2-3 hours for all variants
- **Package resolution**: 2-5 minutes for dependency updates

## Key Projects and Examples

### Core Examples (in `/examples/`)
- **simple_repeater**: Basic mesh repeater functionality - most commonly modified
- **simple_secure_chat**: Terminal-based secure messaging
- **companion_radio**: For use with external applications (USB/BLE/WiFi)
- **simple_room_server**: BBS-style messaging server
- **simple_sensor**: Environmental sensor data collection

### Important Files and Locations
- **Main configuration**: `platformio.ini` - radio frequency (869.525 MHz) and build settings
- **Variant configurations**: `variants/*/platformio.ini` - hardware-specific settings
- **Build scripts**: `build.sh` - automated firmware building (requires virtual environment)
- **Hardware targets**: `include/target.h` and `variants/` - board-specific definitions
- **Core mesh library**: `src/` - main MeshCore networking implementation
- **GitHub Actions**: `.github/workflows/` - automated CI/CD build configurations
- **Build environment setup**: `.github/actions/setup-build-environment/` - CI setup scripts

### Configuration Editing
- **Radio frequency**: Edit `LORA_FREQ` in `platformio.ini` or variant files
- **Default frequency**: 869.525 MHz (configurable per region)
- **Network settings**: Bandwidth, spreading factor, coding rate in build flags
- **Node identity**: Auto-generated on first boot, stored in device filesystem

## Development Workflow

### Making Changes
- **ALWAYS work in examples**: Modify existing examples rather than core library
- **Test with hardware**: Changes require actual LoRa hardware for validation
- **Build verification**: Always build successfully before committing changes
- **Multiple variants**: Test with different hardware targets when possible

### Common Tasks
- **Adding new hardware support**: Create new variant in `variants/` directory
- **Modifying mesh behavior**: Edit example files in `examples/` directories
- **Configuration changes**: Update `platformio.ini` or variant-specific configs
- **Network parameters**: Modify `LORA_FREQ`, `LORA_BW`, `LORA_SF` build flags

### CI/CD Integration
- **GitHub Actions**: Builds are automated via `.github/workflows/`
- **Release process**: Tag with `companion-v1.x.x`, `repeater-v1.x.x`, or `room-server-v1.x.x`
- **Artifact generation**: Firmware binaries are automatically created and released

## Troubleshooting

### Build Issues
- **Network timeouts**: PlatformIO downloads can fail - retry with longer timeouts
- **Toolchain errors**: Delete `.platformio` directory and rebuild if persistent issues
- **Environment issues**: Always activate Python virtual environment before building
- **Dependency conflicts**: Run `pio pkg update` to resolve library conflicts

### Common Errors
- **"PlatformIO not installed"**: Activate virtual environment with `source ../meshcore/bin/activate`
- **"Platform not found"**: First build requires internet for toolchain download
- **"Library conflicts"**: Run package update and rebuild
- **"Out of memory"**: Some variants require specific build flags for memory optimization
- **"pio: command not found"**: Ensure virtual environment is activated and `pio` is in PATH

### Hardware Requirements
- **Development**: Linux, macOS, or Windows with WSL required for building
- **Runtime**: Requires actual LoRa hardware (cannot be simulated)
- **Testing**: Serial connection at 115200 baud for interaction
- **Flashing**: Hardware-specific tools:
  - ESP32: `esptool` (included with ESP32 platform)
  - NRF52: `nrfutil` or dedicated programmer
  - RP2040: UF2 bootloader (drag-and-drop)
  - STM32: ST-Link or similar programmer

## Important Notes
- **NO simulation possible**: MeshCore requires real LoRa hardware
- **Serial interaction**: Use 115200 baud for terminal communication
- **Frequency regulations**: Ensure LORA_FREQ complies with local regulations
- **Build patience**: NEVER cancel long-running builds - they WILL complete
- **Hardware variety**: 186 different firmware variants for different boards
- **Active project**: Firmware is actively developed - check for updates regularly