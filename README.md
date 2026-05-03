
# OTthing
**a compact WiFi <-> OpenTherm (master & slave) interface**

## Project homepage
https://www.seegel-systeme.de/2025/01/05/ot-thing-das-universelle-wifi-opentherm-interface/

![OTthing](assets/DSC_0550-scaled.jpg "OTthing board")
![OTthing Home Assistant Dashboard](assets/otthing_hadash.png "OTthing Home Assistant dashboard")
![OTthing webUI](assets/otthing_webui.png "OTthing webUI")

## Overview

OTthing is a versatile OpenTherm gateway that bridges your heating system to the Internet via WiFi. It acts as an OpenTherm master (controlling your boiler) and/or slave (integrating with room thermostats), providing real-time monitoring and control through a modern web interface and Home Assistant integration.

### Key Features

* **Dual OpenTherm Modes**: Operate as both OpenTherm master and slave simultaneously
* **Web Dashboard**: Real-time status monitoring with system metrics
* **Home Assistant Integration**: Native MQTT discovery and seamless HA integration
* **Multi-Zone Support**: Control up to 2 heating circuits with independent parameters
* **Local Control**: Responsive web UI with no cloud dependency
* **Modular Design**: Extensible architecture supporting multiple integrations
* **Advanced Configuration**: Room modes (off/heat/auto), flow control, heating curves, and more
* **Detailed Logging**: Real-time log streaming to monitor system behavior

## Architecture

The firmware consists of several functional modules:

* **OpenTherm Control** (`otcontrol.cpp`): Manages master/slave communication with the boiler
* **MQTT Integration** (`mqtt.cpp`): Publishes device state and subscribes to control topics
* **Web Portal** (`portal.cpp`): Serves the responsive dashboard and API endpoints
* **Heating Control** (`CHcontrol.cpp`): Manages heating circuits, setpoints, and modes
* **Sensor Integration** (`sensors.cpp`): Collects data from external sensors
* **Configuration Management** (`devconfig.cpp`): Handles persistent device settings

## Hardware

* **MCU**: ESP32-C3 (or compatible with UART pins)
* **Communication**: OpenTherm interface
* **Power**: via USB
* **Optional**: 1wire temperature sensors DS18B20, auxiliary inputs

## Installation & Setup

1. **Build the Firmware**:
   ```bash
   platformio run --environment release
   ```

2. **Flash to Device**:
   ```bash
   platformio run --environment release --target upload
   ```

3. **Access the Web Interface**:
   - Connect to your boiler's OpenTherm bus
   - The device creates a WiFi AP or connects to your network
   - Access the web UI at `http://<otthing-ip>/` (default: 4.3.2.1)

4. **Configure**:
   - Set your boiler type and heating circuit parameters
   - Configure MQTT broker if using Home Assistant
   - Adjust setpoints, heating curves, and control modes

## API Reference

OTthing exposes the following REST endpoints and WebSocket connection:

### Core Status Endpoints

* **`GET /`** - Web dashboard (HTML)
* **`GET /status`** - Current device and boiler status (JSON)
* **`GET /config`** - Device configuration and settings (JSON)
* **`POST /config`** - Update device configuration (JSON body)
* **`GET /otitems`** - Raw OpenTherm items and values from master and slave (JSON)
* **`GET /topics`** - List all available MQTT control topics (text/plain)

### Control Endpoints

* **`GET /set?key=value`** - Update settings via query parameters
  - Examples: `/set?chSetTemp1=50`, `/set?chMode1=heat`, `/set?flowSetTemp=45`
  - Returns 200 on success, 503 if value cannot be set

* **`GET /slaverequest?id=X&rw=Y&data=HEX`** - Send raw OpenTherm slave request
  - `id`: OpenTherm message ID (integer)
  - `rw`: 1 for READ_DATA, 0 for WRITE_DATA
  - `data`: Hex-encoded data value
  - Returns JSON with response type, id, and data

### System Endpoints

* **`GET /scan`** - Scan available WiFi networks
  - Returns JSON with SSID, RSSI, and channel for each network
  - Status field: -2 (scanning in progress), -1 (failed), 0+ (number of networks found)

* **`POST /setwifi`** - Configure WiFi credentials
  - Parameters: `ssid` and `pass` (URL-encoded)
  - Triggers device reboot and WiFi connection attempt

* **`GET /reboot`** - Trigger device reboot
  - Returns 200 and schedules reboot

* **`POST /update`** - Firmware update (binary upload)
  - Returns 200 on success, 500/503 on error
  - Triggers automatic reboot if update succeeds

### Real-time Communication

* **`WebSocket /ws`** - Real-time updates and log streaming
  - Receives log messages and status updates
  - Can be used to monitor device behavior in real-time

## Testing & Development

A Python mock server is included for local testing:

```bash
python tools/mock_otthing.py
```

This provides a fully functional test environment without hardware, allowing UI development and API integration testing.

## Schematics
![OTthing schematic 1](assets/OTthing_schem_1.svg "OTthing schematic page 1")
![OTthing schematic 2](assets/OTthing_schem_2.svg "OTthing schematic page 2")

## Discussion
https://community.home-assistant.io/t/ot-thing-an-opentherm-wifi-gateway-with-integrated-ot-master-slave/824667

## Reporting issues
When reporting issues please supply:
* Brand & model of boiler & roomunit
* Log
* status JSON (http://[OTTHING-IP]/status)
* configuration JSON (http://[OTTHING-IP]/config)
* OT status JSON (http://[OTTHING-IP]/otitems)

## Contributing
* make changes in a new branch based on [`develop`](../../tree/develop) branch
* one issue per PR only
* create PR against [`develop`](../../tree/develop) branch