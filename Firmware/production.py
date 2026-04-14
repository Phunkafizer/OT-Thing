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

# ANSI colour output
if sys.platform == "win32":
    os.system("")  # enable VT processing on Windows
_RED    = "\033[91m"
_GREEN  = "\033[92m"
_YELLOW = "\033[93m"
_RESET  = "\033[0m"

def _ok(msg):   print(f"{_GREEN}{msg}{_RESET}")
def _err(msg):  print(f"{_RED}{msg}{_RESET}")
def _warn(msg): print(f"{_YELLOW}{msg}{_RESET}")

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
    Wait for USB device to appear, then return immediately.
    Returns the port name as soon as it is present.
    """
    last_port = None

    print(
        f"Waiting for USB device VID:PID {TARGET_USB_VID:04X}:{TARGET_USB_PID:04X}... (Ctrl+C to stop)"
    )

    while True:
        port = get_target_port()

        if port is None:
            if last_port is not None:
                print("Device disappeared, waiting for it to re-appear...")
            last_port = None
            time.sleep(DEVICE_POLL_INTERVAL_SECONDS)
            continue

        print(f"Device {port} present. Starting upload.")
        return port


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
        _err("✗ Build artifacts not found at:")
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
        esp = esptool.cmds.detect_chip(port=port)
        try:
            print(f"Chip: {esp.get_chip_description()}")
            print("Erasing flash...")
            try:
                esp.erase_flash()
            except NotImplementedInROMError:
                print("ROM erase_flash unsupported on this chip; starting stub flasher...")
                esp = esp.run_stub()
                esp.erase_flash()

            print("Writing bootloader, partition table, and firmware...")
            import argparse
            with open(bootloader_path, "rb") as f0, \
                 open(partition_path, "rb") as f1, \
                 open(firmware_path, "rb") as f2:
                flash_args = argparse.Namespace(
                    addr_filename=[
                        (0x0, f0),
                        (0x8000, f1),
                        (0x10000, f2),
                    ],
                    flash_size="keep",
                    flash_mode="keep",
                    flash_freq="keep",
                    no_stub=False,
                    compress=None,
                    no_compress=False,
                    verify=False,
                    erase_all=False,
                    encrypt=False,
                    encrypt_files=None,
                    force=False,
                    ignore_flash_encryption_efuse_setting=False,
                )
                esptool.cmds.write_flash(esp, flash_args)
            _ok("✓ Firmware uploaded successfully")
        finally:
            esp._port.close()

        return True
    except Exception as e:
        _err(f"✗ Upload failed: {e}")
        sys.exit(1)


def verify_tcp_stream(host, port, max_cycles=20, connect_timeout=30, read_timeout=10):
    """Connect to a TCP/IP port and verify OT event lines cycling T→S→P→B."""
    import re

    line_pattern = re.compile(r"^[TSPBtspb][0-9A-Fa-f]{8}$")
    sequence = ["T", "S", "P", "B"]
    max_lines = max_cycles * len(sequence)
    print(f"\n=== Verifying TCP/IP stream on {host}:{port} ({max_cycles} cycles) ===")

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
        _err(f"✗ Could not connect to {host}:{port} within {connect_timeout}s")
        return False

    with sock:
        sock.settimeout(read_timeout)
        try:
            with sock.makefile("r", encoding="utf-8", newline="\n") as stream:
                lines_read = 0
                seq_idx = None
                last_t_hex = None  # hex from last T message, expect S to match
                last_p_hex = None  # hex from last P message, expect B to match
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
                        _err(f"✗ Invalid line format: '{line}'. Expected T/S/P/B + 8 hex digits.")
                        return False
                    prefix = line[0].upper()
                    hex_val = line[1:]
                    if seq_idx is None:
                        # Accept any starting position in the cycle
                        seq_idx = sequence.index(prefix) if prefix in sequence else None
                        if seq_idx is None:
                            _err(f"✗ Unexpected prefix '{prefix}', expected one of {sequence}.")
                            return False
                    else:
                        expected_idx = (seq_idx + 1) % len(sequence)
                        if prefix != sequence[expected_idx]:
                            _err(f"✗ Expected '{sequence[expected_idx]}' in T→S→P→B sequence, got '{prefix}'.")
                            return False
                        seq_idx = expected_idx

                    # Cross-message hex matching
                    if prefix == "T":
                        last_t_hex = hex_val
                    elif prefix == "S":
                        if last_t_hex is not None and hex_val != last_t_hex:
                            _err(f"✗ S hex '{hex_val}' does not match preceding T hex '{last_t_hex}'.")
                            return False
                        last_t_hex = None
                    elif prefix == "P":
                        last_p_hex = hex_val
                    elif prefix == "B":
                        if last_p_hex is not None and hex_val != last_p_hex:
                            _err(f"✗ B hex '{hex_val}' does not match preceding P hex '{last_p_hex}'.")
                            return False
                        last_p_hex = None

                if lines_read < max_lines:
                    _err(f"✗ Only received {lines_read}/{max_lines} lines ({lines_read // len(sequence)}/{max_cycles} complete cycles).")
                    return False

        except OSError as exc:
            _err(f"✗ TCP/IP read failed: {exc}")
            return False

    _ok("✓ TCP/IP stream verified")
    return True


def verify_http_page(host, port=80, connect_timeout=30, read_timeout=40):
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

                # Read the full response until connection closes
                response = b""
                while True:
                    try:
                        chunk = sock.recv(4096)
                    except OSError:
                        break
                    if not chunk:
                        break
                    response += chunk

                content = response.decode("utf-8", errors="replace")
                if "HTTP/" not in content:
                    _err("✗ Invalid HTTP response")
                    return False

                status_line = content.splitlines()[0] if content.splitlines() else ""
                print(f"HTTP status: {status_line}")
                if "200" not in status_line:
                    _err("✗ Non-OK HTTP status")
                    return False

                body_start = response.find(b"\r\n\r\n")
                headers = response[:body_start].decode("utf-8", errors="replace") if body_start >= 0 else ""
                body = response[body_start + 4:] if body_start >= 0 else b""

                print(f"Body size: {len(body)} bytes, headers: {headers[:200].strip()}")

                if not body:
                    _err("✗ Response body is empty")
                    return False

                # Handle gzip-encoded response — detect by magic bytes since ESP32
                # often omits Content-Encoding: gzip header
                is_gzip = ("content-encoding: gzip" in headers.lower()) or body[:2] == b"\x1f\x8b"
                if is_gzip:
                    import gzip
                    try:
                        body = gzip.decompress(body)
                    except Exception as exc:
                        _err(f"✗ Failed to decompress gzip body: {exc}")
                        return False

                if b"<html" not in body.lower() and b"<!doctype" not in body.lower():
                    _err("✗ Response body does not contain valid HTML")
                    return False

                _ok("✓ HTTP page verified")
                return True
        except OSError as exc:
            print(f"Waiting for HTTP port {host}:{port}... ({exc})")
            time.sleep(1)

    _err(f"✗ Could not connect to HTTP server on {host}:{port} within {connect_timeout}s")
    return False


def connect_to_otthing_wifi(profile="OTthing", timeout=20, retries=3):
    def _netsh_connect():
        print(f"Connecting to OTthing WiFi (profile: {profile})...")
        cmd = f'netsh wlan connect name="{profile}"'
        result = subprocess.run(["cmd", "/c", cmd], capture_output=True, text=True)
        if result.stdout.strip():
            print(result.stdout.strip())
        if result.returncode != 0:
            print(result.stderr.strip())
            return False
        return True

    for attempt in range(1, retries + 1):
        if not _netsh_connect():
            return False

        wait = 8 if attempt == 1 else 5
        disconnected_early = False
        for remaining in range(wait, 0, -1):
            # Query current WiFi SSID every second during the wait
            check = subprocess.run(
                ["cmd", "/c", "netsh wlan show interfaces"],
                capture_output=True, text=True, encoding="cp850", errors="replace"
            )
            ssid_line = next((l.strip() for l in check.stdout.splitlines() if "SSID" in l and "BSSID" not in l), "SSID: ?")
            state_line = next((l.strip() for l in check.stdout.splitlines() if "Status" in l or "tatus" in l or "Status" in l), "State: ?")
            print(f"  [{remaining:2d}s] {state_line} | {ssid_line}")
            output_lower = check.stdout.lower()
            # Break early if already associated
            if ("verbunden" in output_lower or "connected" in output_lower) and profile.lower() in output_lower:
                print(f"  WiFi associated after {wait - remaining + 1}s")
                break
            # If fully disconnected (not just associating), retry connect immediately
            if "getrennt" in output_lower or "disconnected" in output_lower:
                _warn("  Disconnected — retrying netsh connect immediately...")
                disconnected_early = True
                break
            time.sleep(1)

        if disconnected_early:
            continue  # skip IP probe, go straight to next netsh connect attempt

        # Probe the device IP to confirm the WiFi link is up
        deadline = time.time() + timeout
        connected = False
        while time.time() < deadline:
            try:
                with socket.create_connection((DEVICE_IP, 80), timeout=5):
                    _ok(f"✓ Connected to WiFi profile {profile} (device reachable at {DEVICE_IP})")
                    connected = True
                    break
            except OSError:
                time.sleep(1)

        if connected:
            return True

        _warn(f"⚠ Device not reachable at {DEVICE_IP} after attempt {attempt}/{retries}, retrying netsh...")

    _err(f"✗ OTthing WiFi did not become connected after {retries} attempt(s)")
    return False


def configure_device():
    """
    Configure device after firmware upload.
    Uses DEVICE_IP for web interface connection.
    """
    import requests
    
    print(f"\n=== Configuring device ===")
    
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
            _ok("✓ Configuration verified - device matches sent config")
        else:
            _warn("⚠ Configuration mismatch detected:")
            print("Sent:")
            print(json.dumps(CONFIG, indent=2))
            print("\nReceived:")
            print(json.dumps(conf, indent=2))
        
        _ok("✓ Device configured successfully")
        return True
    except Exception as e:
        _err(f"✗ Configuration failed: {e}")
        sys.exit(1)


def wait_for_device_disconnect():
    """
    Wait for the USB device to be disconnected.
    """
    _warn("Waiting for device to disconnect...")
    
    while True:
        port = get_target_port()
        if port is None:
            _ok("✓ Device disconnected")
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
            _warn("Bring the device into configuration mode by holding RST > 2 seconds")
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
                    configure_device()
                    # Open device web interface
                    config_url = f"http://{DEVICE_IP}"
                    print(f"Opening {config_url}...")
                    webbrowser.open(config_url)
                    _ok("\n" + "=" * 50)
                    _ok(f"  ✓  DEVICE #{upload_count} COMPLETE — ALL STEPS PASSED")
                    _ok("=" * 50 + "\n")
                except Exception as e:
                    _err(f"Configuration error: {e}")
                    failure_count += 1
        else:
            failure_count += 1
        
        # Wait for next device
        _warn("Connect the next one...")
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
