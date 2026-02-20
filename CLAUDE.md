# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Flash Commands

To build, flash, and monitor the device:

```bash
# Clean build
idf.py clean

# Build the project
idf.py build

# Erase flash before flashing (recommended)
idf.py erase-flash

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor
```

## Architecture Overview

This is an ESP32-S3 IoT device (xiaozhi) running a voice assistant with MCP (Model Context Protocol) server capabilities. The device includes:

### Core Components

1. **Application** (`main/application.cc`): Main application state machine and lifecycle management
   - States: starting, activating, idle, connecting, listening, speaking, upgrading
   - Manages MQTT connection for cloud communication
   - Handles wake word detection and audio processing

2. **MCP Server** (`main/mcp_server.cc`): Implements Model Context Protocol for device control
   - Provides tools for device control via MCP
   - Registers tools dynamically based on available hardware
   - Handles JSON-RPC 2.0 communication

3. **Alarm Manager** (`main/alarm_manager.h/cc`): Manages alarm functionality
   - Singleton pattern for alarm management
   - Supports one-time, repeating, and countdown alarms
   - Uses NVS (Non-Volatile Storage) for persistence
   - Integrated with MCP server as tools

### Hardware Abstraction Layer

- **Board** (`main/board.h/cc`): Hardware abstraction layer
- **Display**: ILI9341 LCD with LVGL graphics
- **Audio**: K10AudioCodec with I2S interface
- **Camera**: GC2145 camera sensor
- **LED Strip**: Addressable LED support
- **IO Expander**: PCA9554 for additional GPIO

### Data Persistence

- **Settings** (`main/settings.h/cc`): NVS wrapper for persistent storage
- Used for alarms, configuration, and other device state
- Key-value storage with automatic serialization

### Key Integrations

- **MQTT**: For cloud communication and voice assistant services
- **Audio Service**: Handles audio capture and playback
- **Model Loader**: Manages AI models for wake word and speech processing
- **OTA Updates**: Over-the-air firmware updates

## Known Issues

1. **Version Mismatch**: The log shows "Checksum mismatch between flashed and built applications". Ensure you're building and flashing the correct version.

2. **MCP Tool Registration**: Alarm tools may not be registering properly with the MCP server. Check logs for "MCP: Adding tool" messages during STATE:activating.

3. **Alarm Persistence**: Alarms stored in NVS may not survive reboot if partition configuration is incorrect.

## Development Tips

- Monitor serial output during boot to see MCP tool registration
- Look for "STATE: activating" to see when application-level tools should be added
- Check NVS storage with `idf.py menuconfig` -> "Partition Table" to ensure alarm partition exists
- Use `idf.py monitor` with `-b 115200` for detailed logging