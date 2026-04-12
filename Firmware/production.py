#!/usr/bin/env python3
"""
Production batch firmware upload script for OTthing devices.

Handles:
- Stable USB device detection (VID/PID)
- Bootloader, firmware, and partitions upload
- Device configuration (MAC reading, hard reset, network setup)
- Batch mode with continuous device reprogramming
"""

import os
import sys
import socket
import subprocess
import time
import webbrowser

# USB Device IDs
TARGET_USB_VID = 0x303A
TARGET_USB_PID = 0x1001
STABLE_DEVICE_SECONDS = 4.0
DEVICE_POLL_INTERVAL_SECONDS = 0.5

# Device Network Configuration
DEVICE_IP = "4.3.2.1"
DEVICE_DATA_PORT = 25238

# Default OTthing configuration
CONFIG = {
    "slaveApp": 0,  # heat/cool
    "otMode": 1,  # master
    "enableSlave": False,
    "boiler": {
        "dhwOn": True,
        "dhwTemperature": 50,
        "overrideDhw": False,
        "coolOn": False,
        "maxModulation": 100,
        "otc": False,
        "summerMode": False,
        "dhwBlocking": False,
    },
    "heating": [
        {
            "flow": 45,
            "chOn": True,
            "flowMax": 60,
            "flowMin": 10,
            "exponent": 1.3,
            "gradient": 1.0,
            "offset": 0,
            "marker": [],
            "roomsetpoint": {"source": 0, "temp": 21},
            "roomtemp": {"source": 1},
            "overrideFlow": False,
            "roomComp": {"enabled": False, "p": 1.0, "i": 0.5, "boost": 1.0},
            "enablyHyst": False,
            "hysteresis": 0.5,
            "curveMode": 0,
            "minSuspend": False,
            "suspOffset": 0.0,
            "returnLimit": {
                "source": 1,
                "deltaT": 0.0
            }
        },
        {
            "flow": 30,
            "chOn": False,
            "flowMax": 40,
            "flowMin": 10,
            "exponent": 1.3,
            "gradient": 1.0,
            "offset": 0,
            "marker": [],
            "roomsetpoint": {"source": 0, "temp": 21},
            "roomtemp": {"source": 1},
            "overrideFlow": False,
            "roomComp": {"enabled": False, "p": 1.0, "i": 0.5, "boost": 1.0},
            "enablyHyst": False,
            "hysteresis": 0.5,
            "curveMode": 0,
            "minSuspend": False,
            "suspOffset": 0.0,
            "returnLimit": {
                "source": 1,
                "deltaT": 0.0
            }
        },
    ],
    "vent": {
        "ventEnable": False,
        "openBypass": False,
        "autoBypass": False,
        "freeVentEnable": False,
        "setpoint": 3,
    },
    "outsideTemp": {"source": 1, "apikey": None, "lat": 49.4771, "lon": 10.9887, "interval": 300},
    "mqtt": {"host": "", "port": 1883, "user": "", "pass": "", "tls": False, "keepAlive": 15},
    "masterMemberId": 8,
    "timezone": 3600,
    "hostname": "otthing",
    "haPrefix": "homeassistant",
    "aux": [{"mode": 4}, {"mode": 0}],  # DQ: 1wire, DI: not used
}

def get_target_port():
    """Find the OTthing device on USB by VID/PID."""
    from serial.tools import list_ports

    for d in list_ports.comports():
        if (d.vid == TARGET_USB_VID) and (d.pid == TARGET_USB_PID):
            return d.device
    return None


def wait_for_stable_target_port(stable_seconds=STABLE_DEVICE_SECONDS):
    """
    Wait for USB device to stay connected for stable_seconds.
    Returns the port name once stable.
    """
    stable_port = None
    stable_since = None

    print(
        f"Waiting for USB device VID:PID {TARGET_USB_VID:04X}:{TARGET_USB_PID:04X} "
        f"to stay connected for {int(stable_seconds)} s... (Ctrl+C to stop)"
    )

    while True:
        port = get_target_port()
        now = time.monotonic()

        if port is None:
            if stable_port is not None:
                print("Device disappeared again, waiting for stable connection...")
            stable_port = None
            stable_since = None
            time.sleep(DEVICE_POLL_INTERVAL_SECONDS)
            continue

        if port != stable_port:
            stable_port = port
            stable_since = now
            print(f"Device {port} detected. Checking stability...")
            time.sleep(DEVICE_POLL_INTERVAL_SECONDS)
            continue

        if (now - stable_since) >= stable_seconds:
            print(f"Device {port} stable for {int(stable_seconds)} s. Starting upload.")
            return port

        time.sleep(DEVICE_POLL_INTERVAL_SECONDS)


def upload_firmware(port, project_dir):
    """
    Upload bootloader, firmware, and partitions to device using esptool.
    
    Assumes firmware has already been built in .pio/build/release/
    
    Args:
        port: Serial port (e.g., 'COM3')
        project_dir: Project directory containing .pio/build/release/
    """
    # Paths to build artifacts
    build_dir = os.path.join(project_dir, ".pio", "build", "release")
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    partition_path = os.path.join(build_dir, "partitions.bin")
    firmware_path = os.path.join(build_dir, "firmware.bin")
    
    # Check if all binaries exist
    if not all(os.path.exists(p) for p in [bootloader_path, partition_path, firmware_path]):
        print("✗ Build artifacts not found at:")
        print(f"  {bootloader_path}")
        print(f"  {partition_path}")
        print(f"  {firmware_path}")
        print("\nRun: pio run -e release")
        return False
    
    print(f"\n=== Uploading firmware to {port} ===")
    
    try:
        import esptool
        from esptool.loader import NotImplementedInROMError
        
        # Connect, erase flash, and write binaries in a single session
        with esptool.cmds.detect_chip(port=port) as esp:
            print(f"Chip: {esp.get_chip_description()}")
            print("Erasing flash...")
            try:
                esp.erase_flash()
            except NotImplementedInROMError:
                print("ROM erase_flash unsupported on this chip; starting stub flasher...")
                esp = esptool.run_stub(esp)
                esp.erase_flash()

            print("Writing bootloader, partition table, and firmware...")
            esptool.write_flash(
                esp,
                [
                    (0x0, bootloader_path),
                    (0x8000, partition_path),
                    (0x10000, firmware_path),
                ],
                flash_freq="keep",
                flash_mode="keep",
                flash_size="keep",
            )
            print("✓ Firmware uploaded successfully")

        return True
    except Exception as e:
        print(f"✗ Upload failed: {e}")
        sys.exit(1)


def verify_tcp_stream(host, port, max_lines=40, connect_timeout=20, read_timeout=3):
    """Connect to a TCP/IP port and verify alternating T/B hex lines."""
    import re

    line_pattern = re.compile(r"^[TB][0-9A-Fa-f]{8}$")
    print(f"\n=== Verifying TCP/IP stream on {host}:{port} ===")

    deadline = time.time() + connect_timeout
    sock = None
    while time.time() < deadline:
        try:
            sock = socket.create_connection((host, port), timeout=connect_timeout)
            break
        except OSError as exc:
            print(f"Waiting for TCP/IP port {host}:{port}... ({exc})")
            time.sleep(1)

    if sock is None:
        print(f"✗ Could not connect to {host}:{port} within {connect_timeout}s")
        return False

    with sock:
        sock.settimeout(read_timeout)
        try:
            with sock.makefile("r", encoding="utf-8", newline="\n") as stream:
                previous_prefix = None
                lines_read = 0
                for _ in range(max_lines):
                    line = stream.readline()
                    if not line:
                        break
                    line = line.strip()
                    if not line:
                        continue
                    lines_read += 1
                    print(f"TCP[{lines_read}]: {line}")
                    if not line_pattern.match(line):
                        print("✗ Invalid line format. Expected T/B followed by 8 hex chars.")
                        return False
                    prefix = line[0]
                    if previous_prefix is not None and prefix == previous_prefix:
                        print(
                            f"✗ Expected alternating T/B sequence, got {prefix} after {previous_prefix}."
                        )
                        return False
                    previous_prefix = prefix

                if lines_read < 2:
                    print("✗ Not enough lines received for verification.")
                    return False

        except OSError as exc:
            print(f"✗ TCP/IP read failed: {exc}")
            return False

    print("✓ TCP/IP stream verified")
    return True


def verify_http_page(host, port=80, connect_timeout=5, read_timeout=3):
    """Open an HTTP connection and verify a valid HTML page is returned."""
    print(f"\n=== Verifying HTTP page on http://{host}:{port}/ ===")
    deadline = time.time() + connect_timeout

    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=connect_timeout) as sock:
                sock.settimeout(read_timeout)
                request = (
                    f"GET / HTTP/1.0\r\n"
                    f"Host: {host}\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                )
                sock.sendall(request.encode("ascii"))

                response = b""
                while b"\r\n\r\n" not in response and len(response) < 16384:
                    chunk = sock.recv(1024)
                    if not chunk:
                        break
                    response += chunk

                content = response.decode("utf-8", errors="replace")
                if "HTTP/" not in content:
                    print("✗ Invalid HTTP response")
                    return False

                status_line = content.splitlines()[0] if content.splitlines() else ""
                print(f"HTTP status: {status_line}")
                if "200" not in status_line:
                    print("✗ Non-OK HTTP status")
                    return False

                body = content.split("\r\n\r\n", 1)[1] if "\r\n\r\n" in content else ""
                if "<html" not in body.lower():
                    print("✗ Response does not contain valid HTML")
                    return False

                print("✓ HTTP page verified")
                return True
        except OSError as exc:
            print(f"Waiting for HTTP port {host}:{port}... ({exc})")
            time.sleep(1)

    print(f"✗ Could not connect to HTTP server on {host}:{port} within {connect_timeout}s")
    return False


def connect_to_otthing_wifi(profile="OTthing", timeout=20):
    print("Connecting to OTthing WiFi...")
    cmd = f'netsh wlan connect name="{profile}"'
    result = subprocess.run(["cmd", "/c", cmd], capture_output=True, text=True)
    if result.stdout.strip():
        print(result.stdout.strip())
    if result.returncode != 0:
        print(result.stderr.strip())
        return False

    deadline = time.time() + timeout
    while time.time() < deadline:
        check = subprocess.run(
            ["cmd", "/c", "netsh wlan show interfaces"],
            capture_output=True,
            text=True,
        )
        output = check.stdout
        if "State                   : connected" in output and f"SSID                   : {profile}" in output:
            print(f"✓ Connected to WiFi profile {profile}")
            return True
        time.sleep(1)

    print(f"✗ OTthing WiFi did not become connected within {timeout}s")
    return False


def configure_device(port):
    """
    Configure device after firmware upload.
    
    Reads MAC, performs hard reset, connects via WiFi, and sends config.
    Uses DEVICE_IP for web interface connection.
    """
    import esptool
    import requests
    
    print(f"\n=== Configuring device on {port} ===")
    
    # Detect chip and read MAC
    with esptool.cmds.detect_chip(port=port) as esp:
        mac = esp.read_mac()
        macstr = ":".join(map(lambda x: "%02X" % x, mac))
        print(f"MAC: {macstr}")
        esp.hard_reset()
    
    # Open device web interface
    config_url = f"http://{DEVICE_IP}"
    print(f"Opening {config_url}...")
    webbrowser.open(config_url)
    
    # Wait for user to be ready
    print("Press enter to send default config to target")
    input("")
    
    # Send configuration
    try:
        config_endpoint = f"http://{DEVICE_IP}/config"
        print(f"Sending config to {config_endpoint}...")
        response = requests.post(config_endpoint, json=CONFIG, timeout=5)
        print(f"Response: {response.status_code}")
        
        # Verify configuration
        time.sleep(1)
        response = requests.get(config_endpoint, timeout=3)
        conf = response.json()
        
        # Compare sent vs received
        import json
        if conf == CONFIG:
            print("✓ Configuration verified - device matches sent config")
        else:
            print("⚠ Configuration mismatch detected:")
            print("Sent:")
            print(json.dumps(CONFIG, indent=2))
            print("\nReceived:")
            print(json.dumps(conf, indent=2))
        
        print("✓ Device configured successfully")
        return True
    except Exception as e:
        print(f"✗ Configuration failed: {e}")
        sys.exit(1)


def wait_for_device_disconnect():
    """
    Wait for the USB device to be disconnected.
    """
    print("Waiting for device to disconnect...")
    
    while True:
        port = get_target_port()
        if port is None:
            print("✓ Device disconnected")
            return
        time.sleep(DEVICE_POLL_INTERVAL_SECONDS)


def batch_upload(project_dir):
    """
    Continuous batch upload loop.
    
    Detects stable USB connections and uploads firmware to each device.
    Press Ctrl+C to stop.
    """
    print("\n=== Batch firmware upload mode ===")
    print("Connect devices one at a time. Each will be programmed automatically.\n")
    
    upload_count = 0
    failure_count = 0
    
    while True:
        try:
            stable_port = wait_for_stable_target_port()
        except KeyboardInterrupt:
            print(f"\n\nBatch mode stopped. Programmed {upload_count} device(s), {failure_count} failure(s).")
            break
        
        # Upload firmware
        success = upload_firmware(stable_port, project_dir)
        if success:
            upload_count += 1

            # Wait for device to disconnect and reconnect into config mode
            print("Bring the device into configuration mode by holding RST > 2 seconds")
            wait_for_device_disconnect()
            wait_for_stable_target_port(stable_seconds=2)
            time.sleep(2)

            if not connect_to_otthing_wifi():
                failure_count += 1
            elif not verify_tcp_stream(DEVICE_IP, DEVICE_DATA_PORT):
                failure_count += 1
            elif not verify_http_page(DEVICE_IP, 80):
                failure_count += 1
            else:
                # Attempt configuration
                try:
                    configure_device(stable_port)
                    # Wait for device to be physically disconnected
                except Exception as e:
                    print(f"Configuration error: {e}")
                    failure_count += 1
        else:
            failure_count += 1
        
        # Wait for next device
        print("\n✓ Device complete. Connect the next one...")
        wait_for_device_disconnect()
        time.sleep(1)


if __name__ == "__main__":
    import os
    
    # Get the directory where this script is located (project directory)
    project_dir = os.path.dirname(os.path.abspath(__file__))
    
    try:
        batch_upload(project_dir)
    except KeyboardInterrupt:
        print("\nBatch upload cancelled.")
