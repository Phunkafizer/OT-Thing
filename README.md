
# OTthing

**A compact WiFi <-> OpenTherm (master & slave) interface**

## Project homepage

https://www.seegel-systeme.de/2025/01/05/ot-thing-das-universelle-wifi-opentherm-interface/

![OTthing](assets/DSC_0550-scaled.jpg "OTthing board")
*OTthing board*

![OTthing web UI](assets/otthing_webui1.png "OTthing web UI status")
*OTthing web interface*

![OTthing Home Assistant Dashboard](assets/otthing_hadash.png "OTthing Home Assistant dashboard")
*Home Assistant integration*

## Overview

OTthing is a versatile OpenTherm gateway that connects your heating system to the Internet via WiFi. It acts as an OpenTherm master (controlling your boiler) and/or slave (integrating with room thermostats), providing real-time monitoring and control through a modern web interface and Home Assistant integration.

### Key Features

* **Dual OpenTherm Modes**: Operate as both an OpenTherm master and a slave simultaneously
* **Single-Board Compact Design**: Cost-efficient SMD-based single-board hardware with a small enclosure footprint
* **OpenTherm Compatibility**: Works with OpenTherm-compatible equipment that implements the functions supported by OTthing; available features vary by device
* **USB-C Powered**: Simple and reliable power supply through a standard USB-C connector
* **Web Dashboard**: Real-time status monitoring with system metrics
* **Home Assistant Integration**: Native MQTT discovery and seamless HA integration
* **Multi-Zone Support**: Control up to 2 heating circuits with independent parameters
* **Time Program**: Built-in scheduling for automatic heating setpoint changes
* **Heating Curve Control**: Outdoor-compensated flow temperature control with configurable curve parameters
* **Flow & Setpoint Management**: Fine-grained control for CH temperature targets, modes, and operating limits
* **Raw OpenTherm Tools**: Direct read/write request endpoint for diagnostics and advanced integrations
* **Real-Time Telemetry**: Live updates via WebSocket for logs and status changes
* **REST API**: Full HTTP interface for status, configuration, control commands, and topic discovery
* **WiFi Provisioning**: Built-in network scan and credential setup through the web interface
* **OTA Firmware Update**: In-browser firmware upload endpoint with automatic reboot on success
* **Local Data Export**: Device data and diagnostics export from the web UI for troubleshooting
* **External Inputs & Sensors**: Optional DS18B20, pulse input, or photointerrupter support (for example, condensate monitoring)
* **Selectable Operating Strategies**: Run OTthing as a full heating controller or in monitoring-first mode for telemetry-only operation
* **Automatic Bypass Behavior**: Allows you to restore conventional room-thermostat operation without rewiring
* **Failsafe Runtime Status**: Device health and communication state tracking for robust operation
* **Local Control**: Responsive web UI with no cloud dependency
* **Modular Design**: Extensible architecture supporting multiple integrations
* **Open-Source Platform**: Open hardware and firmware for custom extensions and modifications
* **Advanced Configuration**: Room modes (off/heat/auto), flow control, heating curves, and more
* **Detailed Logging**: Real-time log streaming to monitor system behavior
* **Turbo Mode**: Temporarily raises the flow target by a configurable amount and duration
* **Automatic Suspend**: Optional suspension based on outside temperature, room conditions, and minimum flow thresholds
* **Customizable dashboard**: add metrics to dashboard

## Architecture

The firmware consists of several functional modules:

* **OpenTherm Control** (`otcontrol.cpp`): Manages master/slave communication with the boiler
* **Master Requests** (`masterrequests.cpp`): Builds and schedules OpenTherm master requests and polling cycles
* **OpenTherm Values** (`otvalues.cpp`): Stores, normalizes, and exposes decoded OpenTherm data points
* **Heating Curve** (`heatingcurve.cpp`): Calculates the target flow temperature from heating-curve parameters
* **MQTT Integration** (`mqtt.cpp`): Publishes device state and subscribes to control topics
* **Device Status** (`devstatus.cpp`): Tracks runtime health, connectivity, and status flags
* **Auxiliary Input** (`auxInput.cpp`): Handles external input signals for additional control logic
* **Web Portal** (`portal.cpp`): Serves the responsive dashboard and API endpoints
* **Heating Control** (`CHcontrol.cpp`): Manages heating circuits, setpoints, and modes
* **Sensor Integration** (`sensors.cpp`): Collects data from external sensors
* **Configuration Management** (`devconfig.cpp`): Handles persistent device settings

## Hardware

* **MCU**: ESP32-C3 (or a compatible MCU with UART pins)
* **Communication**: OpenTherm interface
* **Power**: via USB
* **Optional**: DS18B20 1-Wire temperature sensors and auxiliary inputs

OpenTherm support and available controls depend on the boiler or other equipment. Check the device documentation and OTthing schematics before installation. Disconnect equipment from mains power before wiring; have boiler wiring performed by a qualified installer where required. Connect OpenTherm through the board's designated terminals, never directly to MCU GPIO pins.

## Installation & Setup

1. **Hardware Setup**:

   - Connect the OpenTherm slave side (boiler / ventilation / solar storage) to the **Boiler** screw terminal on OTthing
   - Optionally connect the room unit to the **Roomunit** screw terminals on OTthing
   - Connect OTthing to a USB-C power supply

2. **Access the Web Interface**:

   - The device creates a WiFi access point for provisioning or connects to its configured network
   - The default access point password is `12345678`; change it before deployment
   - Access the web UI at `http://<otthing-ip>/` (default: 4.3.2.1)

3. **Configure**:

   - Set your boiler type and heating circuit parameters
   - Configure the MQTT broker if you are using Home Assistant
   - Adjust setpoints, heating curves, and control modes

### Build and Flash (Developers)

Install PlatformIO, then run these commands from the `Firmware/` directory:

```bash
platformio run --environment release
platformio run --environment release --target upload
platformio run --environment release --target uploadfs
```

The first command builds the firmware. The next two upload the firmware and the LittleFS web assets; perform both uploads when installing a complete build. The firmware project reads access point SSID, password, and hostname from `default.ini` unless overridden by the build environment. Set a unique access point password before deploying devices.

## API Reference

OTthing exposes the following REST endpoints and WebSocket connection:

When web authentication is configured, protected endpoints require a valid login session. Requests without a valid session return 401; log in through the web UI before using these endpoints.

### Core Status Endpoints

* **`GET /`** - Web dashboard (HTML)
* **`GET /status`** - Current device and boiler status (JSON)
* **`GET /config`** - Device configuration and settings (JSON)
* **`POST /config`** - Update device configuration (JSON body)
* **`GET /otitems`** - Raw OpenTherm items and values from master and slave (JSON)
* **`GET /topics`** - List all available MQTT control topics (text/plain)

### Control Endpoints

* **`GET /set?key=value`** - Update settings via query parameters
  - Examples: `/set?chSetTemp1=50`, `/set?chMode1=heat`
  - Use `/topics` to discover supported control keys. URL-encode values when they contain reserved characters.
  - Returns 200 on success, 503 if the value cannot be set

* **`GET /slaverequest?id=X&rw=Y&data=HEX`** - Send a raw OpenTherm slave request
  - `id`: OpenTherm message ID (integer)
  - `rw`: 1 for READ_DATA, 0 for WRITE_DATA
  - `data`: Hex-encoded data value
  - Returns JSON with response type, ID, and data

### System Endpoints

* **`GET /scan`** - Scan for available WiFi networks
  - Returns JSON with the SSID, RSSI, and channel for each network
  - Status field: -2 (scanning in progress), -1 (failed), 0+ (number of networks found)

* **`POST /setwifi`** - Configure WiFi credentials
  - Parameters: `ssid` and `pass` (URL-encoded)
  - Triggers a device reboot and a WiFi connection attempt

* **`GET /reboot`** - Trigger a device reboot
  - Returns 200 and schedules a reboot

* **`POST /update`** - Firmware update (binary upload)
  - Returns 200 on success, 500/503 on error
  - Triggers an automatic reboot if the update succeeds

### Real-time Communication

* **`WebSocket /ws`** - Real-time updates and log streaming
  - Receives log messages and status updates
  - Can be used to monitor device behavior in real time

## Testing & Development

A Python mock server is included for local UI and API testing. Install its dependencies and start it from the `Firmware/` directory:

```bash
python -m pip install -r requirements.txt
python tools/mock_otthing.py
```

The server listens on `http://127.0.0.1:8081/`. It is useful for UI development and API integration, but it does not replace testing against physical OpenTherm hardware.

### MQTT Notes

The device subscribes to control topics under `otthing/<device-id>/<key>/set`; `/topics` lists the supported keys. Home Assistant discovery is provided through MQTT when a broker is configured.

Changes sent through the web API are also published to the corresponding MQTT `/set` topic as retained messages. A broker can replay retained commands when the device reconnects, including after a reboot. Keep this in mind for temporary controls such as Turbo mode, and clear any stale retained command from the broker when needed.

### Troubleshooting

* **Cannot reach the web UI**: Confirm the phone or computer is connected to the OTthing access point or the same network as the device, then check the device IP.
* **No OpenTherm communication**: Check the board terminals, wiring polarity where applicable, boiler compatibility, and the device's status and logs.
* **Home Assistant entities are missing**: Confirm broker connectivity and MQTT discovery settings, then review device logs and the broker's topics.
* **A setting returns 401**: Log in through the web UI and retry the request with the browser session.
* **A temporary action returns after reboot**: Check whether a retained MQTT `/set` command is replaying it.

## Schematics

![OTthing schematic 1](assets/OTthing_schem_1.svg "OTthing schematic page 1")
![OTthing schematic 2](assets/OTthing_schem_2.svg "OTthing schematic page 2")

## Discussion

https://community.home-assistant.io/t/ot-thing-an-opentherm-wifi-gateway-with-integrated-ot-master-slave/824667

## Reporting issues

When reporting issues, please supply:

* Brand and model of the boiler and room unit
* OTthing firmware version and the steps that reproduce the issue
* Log
* Status JSON
* Configuration JSON
* OT items JSON
* Data history JSON

All of this data can be exported from OTthing using the export function at the bottom of the web UI.

## Contributing

* Make changes in a new branch based on the [`develop`](../../tree/develop) branch.
* Address only one issue per PR.
* Create the PR against the [`develop`](../../tree/develop) branch.
