import argparse
import json
from typing import Any
from urllib import error, request

import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse

# Central field table for backend type handling and frontend rendering.
# Add future properties (for example readOnly) here once and both layers consume it.
TESTDATA_FIELDS = [
    {"section": "class1", "key": "status", "label": "Master/Slave Status", "otId": 0, "kind": "bitfield", "readOnly": False},
    {"section": "class1", "key": "fault_flags", "label": "Application specific fault flags", "otId": 5, "kind": "faultflags", "readOnly": False},
    {"section": "class1", "key": "vent_fault_flags", "label": "Application-specific fault flags ventilation / heat-recovery", "otId": 72, "kind": "faultflags_vent", "readOnly": False},
    {"section": "class1", "key": "ch_set_t2", "label": "Control Setpoint 2", "unit": "°C", "otId": 8, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class1", "key": "ch_set_t", "label": "Control setpoint", "unit": "°C", "otId": 1, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class4", "key": "room_set_t", "label": "Room Setpoint", "unit": "°C", "otId": 16, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class4", "key": "rel_mod", "label": "Relative Modulation Level", "unit": "%", "otId": 17, "kind": "f8.8", "step": "0.1", "min": "0", "max": "100", "readOnly": False},
    {"section": "class4", "key": "ch_pressure", "label": "CH water pressure", "unit": "bar", "otId": 18, "kind": "f8.8", "step": "0.1", "min": "0", "max": "5", "readOnly": False},
    {"section": "class4", "key": "dhw_flow_rate", "label": "DHW flow rate", "otId": 19, "kind": "f8.8", "step": "0.1", "min": "0", "max": "16", "readOnly": False},
    {"section": "class4", "key": "room_set_t2", "label": "Room Setpoint CH2", "unit": "°C", "otId": 23, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class4", "key": "room_t", "label": "Room temperature", "unit": "°C", "otId": 24, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class4", "key": "flow_t", "label": "Flow temp.", "unit": "°C", "otId": 25, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "dhw_t", "label": "DHW temp.", "unit": "°C", "otId": 26, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "outside_t", "label": "Outside temperature", "otId": 27, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "return_t", "label": "Return water temperature", "otId": 28, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "flow_t2", "label": "Flow temperature CH2", "otId": 31, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "dhw_t2", "label": "DHW2 temperature", "otId": 32, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "exhaust_t", "label": "Exhaust temperature", "otId": 33, "kind": "s16", "step": "1", "min": "-40", "max": "500", "readOnly": False},
    {"section": "class4", "key": "boiler_heat_ex_t", "label": "Boiler heat exchanger temp.", "otId": 34, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "boiler_fan", "label": "Boiler fan speed", "otId": 35, "kind": "u8pair", "readOnly": True, "parts": [{"slot": "setpoint", "label": "Boiler fan speed Setpoint", "unit": "Hz"}, {"slot": "actual", "label": "Boiler fan speed", "unit": "Hz"}]},
    {"section": "class4", "key": "flame_current", "label": "Flame current", "unit": "µA", "otId": 36, "kind": "f8.8", "step": "0.1", "min": "0", "max": "127", "readOnly": False},
    {"section": "class4", "key": "room_t2", "label": "Room temperature CH2", "unit": "°C", "otId": 37, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class4", "key": "rel_hum_exhaust", "label": "Relative humidity exhaust air", "unit": "%", "otId": 78, "kind": "u8", "step": "1", "min": "0", "max": "100", "readOnly": False},
    {"section": "class4", "key": "co2_exhaust", "label": "CO2 level", "otId": 79, "kind": "u16", "step": "1", "min": "0", "max": "2000", "readOnly": False},
    {"section": "class4", "key": "supply_inlet_t", "label": "Supply inlet temperature", "otId": 80, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "supply_outlet_t", "label": "Supply outlet temperature", "otId": 81, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "exhaust_inlet_t", "label": "Exhaust inlet temperature", "otId": 82, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "exhaust_outlet_t", "label": "Exhaust outlet temperature", "otId": 83, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": False},
    {"section": "class4", "key": "exhaust_fan_speed", "label": "Actual exhaust fan speed", "otId": 84, "kind": "u16", "step": "1", "min": "0", "max": "6000", "readOnly": False},
    {"section": "class4", "key": "supply_fan_speed", "label": "Actual inlet fan speed", "otId": 85, "kind": "u16", "step": "1", "min": "0", "max": "6000", "readOnly": False},
    {"section": "class4", "key": "cooling_op_hours", "label": "Cooling operation hours", "unit": "h", "otId": 96, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "power_cycles", "label": "Power Cycles", "otId": 97, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "unsuccessful_burner_starts", "label": "Unsuccessful burner starts", "otId": 113, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "num_flame_signal_low", "label": "Number of times flame signal was too low", "otId": 114, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "burner_starts", "label": "Successful Burner starts", "otId": 116, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "ch_pump_starts", "label": "CH pump starts", "otId": 117, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "dhw_pump_starts", "label": "DHW pump/valve starts", "otId": 118, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "dhw_burner_starts", "label": "DHW burner starts", "otId": 119, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "burner_op_hours", "label": "Burning operating hours", "unit": "h", "otId": 120, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "chpump_op_hours", "label": "CH pump operation hours", "unit": "h", "otId": 121, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "dhwpump_op_hours", "label": "DHW pump/valve operation hours", "unit": "h", "otId": 122, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class4", "key": "dhw_burner_op_hours", "label": "DHW burner operation hours", "unit": "h", "otId": 123, "kind": "u16", "step": "1", "min": "0", "max": "65535", "readOnly": False},
    {"section": "class5", "key": "dhw_set_t", "label": "DHW Setpoint", "unit": "°C", "otId": 56, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class8", "key": "remote_override_room_setpoint", "label": "Remote Override Room Setpoint", "unit": "°C", "otId": 9, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
    {"section": "class8", "key": "remote_override_room_setpoint2", "label": "Remote Override Room Setpoint 2", "unit": "°C", "otId": 39, "kind": "f8.8", "step": "0.1", "min": "-40", "max": "127", "readOnly": True},
]

STATUS_FIELD_DEF = next(field for field in TESTDATA_FIELDS if field["key"] == "status")
CLASS4_FIELDS = [field for field in TESTDATA_FIELDS if field["section"] == "class4"]

TESTDATA_KEY_TYPES = {field["key"]: field["kind"] for field in TESTDATA_FIELDS}

STATUS_BIT_FLAGS = [
    {"bit": 0, "label": "fault indication", "clear": "no fault", "set": "fault"},
    {"bit": 1, "label": "CH mode", "clear": "CH not active", "set": "CH active"},
    {"bit": 2, "label": "DHW mode", "clear": "DHW not active", "set": "DHW active"},
    {"bit": 3, "label": "Flame status", "clear": "flame off", "set": "flame on"},
    {"bit": 4, "label": "Cooling status", "clear": "cooling mode not active", "set": "cooling mode active"},
    {"bit": 5, "label": "CH2 mode", "clear": "CH2 not active", "set": "CH2 active"},
    {"bit": 6, "label": "diagnostic/service indication", "clear": "no diagnostic/service", "set": "diagnostic/service event"},
    {"bit": 7, "label": "Electricity production", "clear": "off", "set": "on"},
    {"bit": 8, "label": "CH enable", "clear": "CH is disabled", "set": "CH is enabled", "readOnly": True},
    {"bit": 9, "label": "DHW enable", "clear": "DHW is disabled", "set": "DHW is enabled", "readOnly": True},
    {"bit": 10, "label": "Cooling enable", "clear": "Cooling is disabled", "set": "Cooling is enabled", "readOnly": True},
    {"bit": 11, "label": "OTC active", "clear": "OTC not active", "set": "OTC is active", "readOnly": True},
    {"bit": 12, "label": "CH2 enable", "clear": "CH2 is disabled", "set": "CH2 is enabled", "readOnly": True},
    {"bit": 13, "label": "Summer/winter mode", "clear": "winter mode active", "set": "summer mode active", "readOnly": True},
    {"bit": 14, "label": "DHW blocking", "clear": "DHW unblocked", "set": "DHW blocked", "readOnly": True},
    {"bit": 15, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
]

ASF_BIT_FLAGS = [
    {"bit": 0, "label": "Service request", "clear": "service not req’d", "set": "service required", "readOnly": True},
    {"bit": 1, "label": "Lockout-reset", "clear": "remote reset disabled", "set": "rr enabled", "readOnly": True},
    {"bit": 2, "label": "Low water press", "clear": "no WP fault", "set": "water pressure fault", "readOnly": True},
    {"bit": 3, "label": "Gas/flame fault", "clear": "no G/F fault", "set": "gas/flame fault", "readOnly": True},
    {"bit": 4, "label": "Air press fault", "clear": "no AP fault", "set": "air pressure fault", "readOnly": True},
    {"bit": 5, "label": "Water over-temp", "clear": "no OvT fault", "set": "over-temperat. fault", "readOnly": True},
    {"bit": 6, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
    {"bit": 7, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
]

ASF_VENT_BIT_FLAGS = [
    {"bit": 0, "label": "Service request", "clear": "service not req’d", "set": "service required", "readOnly": True},
    {"bit": 1, "label": "Exhaust fan fault", "clear": "no fault", "set": "fault", "readOnly": True},
    {"bit": 2, "label": "Inlet fan fault", "clear": "no fault", "set": "fault", "readOnly": True},
    {"bit": 3, "label": "Frost protection", "clear": "not active", "set": "active", "readOnly": True},
    {"bit": 4, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
    {"bit": 5, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
    {"bit": 6, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
    {"bit": 7, "label": "reserved", "clear": "reserved", "set": "reserved", "readOnly": True},
]


def normalize_base_url(device_host: str) -> str:
    host = device_host.strip()
    if not host:
        raise ValueError("device host must not be empty")
    if "://" not in host:
        host = "http://" + host
    return host.rstrip("/")


def call_device_json(base_url: str, method: str, path: str, payload: dict[str, Any] | None = None) -> Any:
    url = f"{base_url}{path}"
    data: bytes | None = None
    headers: dict[str, str] = {}

    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = request.Request(url=url, data=data, method=method, headers=headers)
    try:
        with request.urlopen(req, timeout=5) as resp:
            content_type = (resp.headers.get("Content-Type") or "").lower()
            body = resp.read().decode("utf-8")
            if "application/json" in content_type:
                return json.loads(body)
            if body:
                return {"raw": body}
            return {}
    except error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise HTTPException(status_code=502, detail=f"device error {exc.code}: {body}") from exc
    except (error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise HTTPException(status_code=502, detail=f"device request failed: {exc}") from exc


def decode_ot_float(value: int) -> float:
    raw16 = value & 0xFFFF
    if raw16 & 0x8000:
        signed = raw16 - 0x10000
    else:
        signed = raw16
    return round(signed * 10 / 256.0) / 10.0


def encode_ot_float(temp_c: float) -> int:
    raw = int(round(temp_c * 256.0))
    if raw < -32768 or raw > 32767:
        raise ValueError("value out of range for signed 16-bit OpenTherm float")
    return raw & 0xFFFF


def to_uint16(value: int) -> int:
    return value & 0xFFFF


def parse_uint16_like(value: Any) -> int | None:
  if value is None:
    return None

  if isinstance(value, bool):
    return int(value)

  if isinstance(value, int):
    return value

  if isinstance(value, float):
    if value != value:
      return None
    return int(value)

  if isinstance(value, str):
    s = value.strip()
    if not s:
      return None

    try:
      if s.startswith(("0x", "0X")):
        return int(s[2:], 16)
      return int(s, 10)
    except ValueError:
      return None

  return None


def encode_ot_value(key: str, value: Any) -> int:
  kind = TESTDATA_KEY_TYPES.get(key)
  if kind == "bitfield":
    return to_uint16(int(value))
  if kind in ("faultflags", "faultflags_vent"):
    return to_uint16(int(value))
  if kind == "u8":
    return to_uint16(int(value) & 0xFF)
  if kind == "f8.8":
    # Accept raw OT uint16 payloads from the web UI; only encode when a float is provided.
    if isinstance(value, int):
      return to_uint16(value)
    return to_uint16(encode_ot_float(float(value)))
  if kind in ("u16", "s16"):
    return to_uint16(int(value))
  raise ValueError(f"unsupported testdata key: {key}")


def find_testdata_value(data: Any, name: str) -> int | None:
    if isinstance(data, dict):
        if data.get("name") == name and "value" in data:
            return parse_uint16_like(data["value"])

        if name in data:
            value = data[name]
            if isinstance(value, dict) and {"setpoint", "actual"}.issubset(value):
                high = parse_uint16_like(value.get("setpoint"))
                low = parse_uint16_like(value.get("actual"))
                if high is not None and low is not None:
                    return ((high & 0xFF) << 8) | (low & 0xFF)
            return parse_uint16_like(value)

        items = data.get("items")
        if isinstance(items, list):
            return find_testdata_value(items, name)

    if isinstance(data, list):
        for item in data:
            value = find_testdata_value(item, name)
            if value is not None:
                return value

    return None


def normalize_testdata_raw(data: Any) -> dict[str, int]:
    normalized: dict[str, int] = {}
    for key in TESTDATA_KEY_TYPES:
        value = find_testdata_value(data, key)
        if value is not None:
            normalized[key] = (value & 0xFF) if TESTDATA_KEY_TYPES[key] == "u8" else to_uint16(value)
    return normalized


def find_unknown_testdata_items(data: Any) -> list[tuple[str, Any]]:
  if not isinstance(data, dict):
    return []

  known_keys = set(TESTDATA_KEY_TYPES)
  unknown_items = [(key, value) for key, value in data.items() if key not in known_keys]
  unknown_items.sort(key=lambda item: item[0])
  return unknown_items


def build_html() -> str:
    status_flags_json = json.dumps(STATUS_BIT_FLAGS, separators=(",", ":"))
    asf_flags_json = json.dumps(ASF_BIT_FLAGS, separators=(",", ":"))
    asf_vent_flags_json = json.dumps(ASF_VENT_BIT_FLAGS, separators=(",", ":"))
    status_field_json = json.dumps(
        {
            "key": STATUS_FIELD_DEF["key"],
            "label": STATUS_FIELD_DEF["label"],
            "otId": STATUS_FIELD_DEF["otId"],
            "kind": STATUS_FIELD_DEF["kind"],
            "readOnly": STATUS_FIELD_DEF.get("readOnly", False),
        },
        separators=(",", ":"),
    )
    class1_fields_json = json.dumps(
        [field for field in TESTDATA_FIELDS if field["section"] == "class1" and field["key"] != "status"],
        separators=(",", ":"),
    )
    class4_fields_json = json.dumps(CLASS4_FIELDS, separators=(",", ":"))
    class5_fields_json = json.dumps(
      [field for field in TESTDATA_FIELDS if field["section"] == "class5"],
      separators=(",", ":"),
    )
    class8_fields_json = json.dumps(
      [field for field in TESTDATA_FIELDS if field["section"] == "class8"],
      separators=(",", ":"),
    )
    html = """<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>OTThing Testdata</title>
    <style>
    :root {
      --bg: #f3f2ec;
      --card: #fffdf7;
      --ink: #1d2a33;
      --accent: #1f7a8c;
      --accent-2: #bf4f35;
      --line: #d9d2c4;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Geneva, Verdana, sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at 20% 10%, #ffffffaa 0%, #ffffff00 40%),
        radial-gradient(circle at 90% 80%, #d8f0e8 0%, #d8f0e800 35%),
        linear-gradient(160deg, var(--bg), #e7edf0);
      display: grid;
      place-items: center;
      padding: 24px;
    }

    .card {
      width: min(1120px, 100%);
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 16px;
      box-shadow: 0 10px 30px #00000014;
      padding: 24px;
    }

    h1 {
      margin: 0 0 6px;
      font-size: 1.4rem;
      letter-spacing: 0.02em;
    }

    .sub {
      margin: 0 0 22px;
      color: #415866;
      font-size: 0.95rem;
    }

    .row {
      display: grid;
      grid-template-columns: minmax(210px, 1fr) minmax(120px, 220px);
      gap: 8px;
      align-items: center;
      margin-bottom: 12px;
    }

    .class4-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 10px 12px;
    }

    .class4-item {
      display: grid;
      grid-template-columns: minmax(180px, 1fr) minmax(92px, 128px);
      gap: 8px;
      align-items: center;
      border: 1px solid #e3e7e8;
      border-radius: 10px;
      background: #ffffff;
      padding: 8px 10px;
    }

    .section {
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 14px;
      margin-bottom: 16px;
      background: #ffffffcc;
    }

    .section h2 {
      margin: 0 0 12px;
      font-size: 0.98rem;
      color: #2c4553;
    }

    .subsection-title {
      margin: 16px 0 10px;
      font-size: 0.9rem;
      color: #3f5a66;
    }

    label {
      font-size: 0.92rem;
      font-weight: 600;
    }

    .badge {
      display: inline-block;
      margin-left: 6px;
      padding: 2px 7px;
      border-radius: 999px;
      font-size: 0.72em;
      font-weight: 700;
      letter-spacing: 0.03em;
      color: #1f7a8c;
      background: #d8f0e8;
      border: 1px solid #b6ddd0;
      vertical-align: middle;
    }

    .unit {
      display: inline-block;
      margin-left: 6px;
      padding: 1px 6px;
      border-radius: 8px;
      font-size: 0.75em;
      font-weight: 700;
      color: #5f4a00;
      background: #ffeeba;
      border: 1px solid #f3d58b;
      vertical-align: middle;
    }

    .bitfield-box {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
      gap: 8px 10px;
      margin-top: 4px;
    }

    .bitfield-item {
      border: 1px solid #dce3dc;
      border-radius: 10px;
      padding: 8px 10px;
      background: #f9fbf8;
    }

    .bitfield-item strong {
      display: inline-block;
      margin-right: 4px;
    }

    .bitfield-toggle {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 0.9rem;
      font-weight: 600;
    }

    .bitfield-toggle input {
      width: 18px;
      height: 18px;
      margin: 0;
      accent-color: var(--accent);
      flex: 0 0 auto;
    }

    .bitfield-meta {
      margin-top: 5px;
      font-size: 0.78rem;
      color: #5e6f78;
      line-height: 1.3;
    }

    .pair-values {
      display: grid;
      gap: 8px;
    }

    .faultflags-item {
      grid-template-columns: 1fr;
      grid-column: 1 / -1;
    }

    .faultflags-box {
      border: 0;
      border-radius: 0;
      background: transparent;
      padding: 0;
    }

    .faultflags-bits {
      margin-top: 8px;
    }

    .faultflags-oem {
      display: grid;
      grid-template-columns: minmax(210px, 1fr) minmax(120px, 220px);
      gap: 8px;
      align-items: center;
      margin-bottom: 10px;
    }

    .faultflags-oem label {
      font-size: 0.92rem;
      font-weight: 600;
    }

    .pair-value {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 10px;
      background: #f7f9fa;
      color: #415866;
      font-size: 1rem;
    }

    .pair-value strong {
      font-weight: 600;
      color: var(--ink);
    }

    .readonly-value {
      width: 100%;
      display: block;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 10px;
      background: #f7f9fa;
      color: #415866;
      font-size: 1rem;
    }

    input[type=number] {
      width: 100%;
      font-size: 1rem;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 10px;
      color: var(--ink);
      background: #fff;
    }

    input[type=text] {
      width: 100%;
      font-size: 1rem;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 10px;
      color: var(--ink);
      background: #f7f9fa;
      font-family: Consolas, "Courier New", monospace;
    }

    .actions {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 14px;
    }

    button {
      border: 0;
      border-radius: 999px;
      padding: 10px 16px;
      font-weight: 700;
      cursor: pointer;
      color: #fff;
      background: var(--accent);
      transition: transform .12s ease, filter .12s ease;
    }

    button.secondary {
      background: var(--accent-2);
    }

    button:hover {
      transform: translateY(-1px);
      filter: brightness(1.05);
    }

    .status {
      font-size: 0.92rem;
      min-height: 1.3em;
      color: #415866;
    }

    .mono {
      font-family: Consolas, "Courier New", monospace;
      font-size: 0.85rem;
      margin-top: 10px;
      white-space: pre-wrap;
      color: #5a6b76;
      border-top: 1px dashed var(--line);
      padding-top: 10px;
    }

    @media (max-width: 900px) {
      .card {
        width: min(720px, 100%);
      }

      .row,
      .class4-item {
        grid-template-columns: 1fr;
      }
    }
    </style>
</head>
<body>
    <section class="card">
      <h1>Testdata Editor</h1>
      <p class="sub">Editor with grouped value items.</p>

      <div class="section">
        <h2>Class 1: Control and status information</h2>
        <div class="row">
          <label for="statusRaw">Master/Slave Status <span class="badge">ID 0</span></label>
          <input id="statusRaw" type="text" readonly />
        </div>
        <div id="statusBitfieldFlags" class="bitfield-box"></div>
        <div id="class1AllFields" class="class4-grid"></div>
      </div>

      <div class="section">
        <h2>Class 4: Sensor and informational data</h2>
        <div id="class4AllFields" class="class4-grid"></div>
      </div>

      <div class="section">
        <h2>Class 5: Pre-defined remote boiler parameters</h2>
        <div id="class5AllFields" class="class4-grid"></div>
      </div>

      <div class="section">
        <h2>Class 8: Control of Special Applications</h2>
        <div id="class8AllFields" class="class4-grid"></div>
      </div>

      <div class="actions">
        <button id="refreshBtn" type="button">Get Value</button>
        <button id="saveBtn" class="secondary" type="button">Set Value</button>
      </div>

      <div id="status" class="status"></div>
      <div id="raw" class="mono"></div>
    </section>

    <script>
    const STATUS_BIT_FLAGS = __STATUS_BIT_FLAGS__;
    const ASF_BIT_FLAGS = __ASF_BIT_FLAGS__;
    const ASF_VENT_BIT_FLAGS = __ASF_VENT_BIT_FLAGS__;
    const STATUS_FIELD = {
      ...__STATUS_FIELD__,
      rawId: "statusRaw",
      containerId: "statusBitfieldFlags",
      flags: STATUS_BIT_FLAGS,
    };
    const CLASS1_FIELDS = __CLASS1_FIELDS__;
    const CLASS4_FIELDS = __CLASS4_FIELDS__;
    const CLASS5_FIELDS = __CLASS5_FIELDS__;
    const CLASS8_FIELDS = __CLASS8_FIELDS__;
    const VALUE_FIELDS = [...CLASS1_FIELDS, ...CLASS4_FIELDS, ...CLASS5_FIELDS, ...CLASS8_FIELDS];

    const statusBitfieldFlags = document.getElementById("statusBitfieldFlags");
    const class1AllFields = document.getElementById("class1AllFields");
    const class4AllFields = document.getElementById("class4AllFields");
    const class5AllFields = document.getElementById("class5AllFields");
    const class8AllFields = document.getElementById("class8AllFields");
    const statusEl = document.getElementById("status");
    const rawEl = document.getElementById("raw");

    function isFaultFlagsKind(kind) {
      return kind === "faultflags" || kind === "faultflags_vent";
    }

    function getFaultFlagsDef(field) {
      return field.kind === "faultflags_vent" ? ASF_VENT_BIT_FLAGS : ASF_BIT_FLAGS;
    }

    function renderBitfield(field, container) {
      container.innerHTML = field.flags.map((flag) => {
        const disabled = flag.readOnly ? " disabled" : "";
        const readOnlyBadge = flag.readOnly ? ' <span class="unit">read only</span>' : "";
        return '<div class="bitfield-item">'
          + '<label class="bitfield-toggle" for="' + field.key + '_' + flag.bit + '">'
          + '<input id="' + field.key + '_' + flag.bit + '" type="checkbox" data-bit="' + flag.bit + '"' + disabled + ' />'
          + '<span>Bit ' + flag.bit + '</span>'
          + '</label>'
          + '<div><strong>' + flag.label + '</strong><span class="badge">' + flag.bit + '</span>' + readOnlyBadge + '</div>'
          + '<div class="bitfield-meta">0: ' + flag.clear + '<br />1: ' + flag.set + '</div>'
          + '</div>';
      }).join("");

      for (const flag of field.flags) {
        const checkbox = container.querySelector(`input[data-bit="${flag.bit}"]`);
        if (checkbox) {
          checkbox.addEventListener("change", () => {
            const raw = readBitfieldValue(field);
            const rawInput = document.getElementById(field.rawId);
            if (rawInput) {
              rawInput.value = String(raw);
            }
          });
        }
      }
    }

    function readBitfieldValue(field) {
      const container = document.getElementById(field.containerId);
      let raw = 0;
      for (const flag of field.flags) {
        const checkbox = container.querySelector(`input[data-bit="${flag.bit}"]`);
        if (checkbox && checkbox.checked) {
          raw |= (1 << flag.bit);
        }
      }
      return raw & 0xFFFF;
    }

    function parseRawUint16(value) {
      if (value === null || value === undefined) {
        return 0;
      }
      if (typeof value === "number") {
        return value & 0xFFFF;
      }
      if (typeof value === "string") {
        const trimmed = value.trim();
        if (trimmed === "") {
          return 0;
        }
        const parsed = (trimmed.startsWith("0x") || trimmed.startsWith("0X"))
          ? Number.parseInt(trimmed.slice(2), 16)
          : Number.parseInt(trimmed, 10);
        if (Number.isFinite(parsed)) {
          return parsed & 0xFFFF;
        }
      }
      return 0;
    }

    function writeBitfieldValue(field, rawValue) {
      const container = document.getElementById(field.containerId);
      if (!container) {
        return;
      }

      const value = parseRawUint16(rawValue);
      const input = document.getElementById(field.rawId);
      if (input) {
        input.value = String(value);
      }

      for (const flag of field.flags) {
        const checkbox = container.querySelector(`input[data-bit="${flag.bit}"]`);
        if (checkbox) {
          checkbox.checked = (value & (1 << flag.bit)) !== 0;
        }
      }
    }

    function renderValueFields(fields, container) {
      container.innerHTML = fields.map((field) => {
        const unit = field.unit || (field.label.toLowerCase().includes("temperature") ? "°C" : "");
        const unitHtml = unit ? ' <span class="unit">' + unit + '</span>' : "";
        if (isFaultFlagsKind(field.kind)) {
          const flags = getFaultFlagsDef(field);
          const bitsHtml = flags.map((flag) => {
            const disabled = field.readOnly ? ' disabled' : '';
            const readOnlyBadge = field.readOnly ? ' <span class="unit">read only</span>' : '';
            return '<div class="bitfield-item">'
              + '<label class="bitfield-toggle" for="' + field.key + '_bit_' + flag.bit + '">'
              + '<input id="' + field.key + '_bit_' + flag.bit + '" type="checkbox" data-bit="' + flag.bit + '"' + disabled + ' />'
              + '<span>Bit ' + flag.bit + '</span>'
              + '</label>'
              + '<div><strong>' + flag.label + '</strong><span class="badge">' + flag.bit + '</span>' + readOnlyBadge + '</div>'
              + '<div class="bitfield-meta">0: ' + flag.clear + '<br />1: ' + flag.set + '</div>'
              + '</div>';
          }).join("");
          const oemControl = field.readOnly
            ? '<span id="' + field.key + '_oem" class="readonly-value">--</span>'
            : '<input id="' + field.key + '_oem" type="number" min="0" max="255" step="1" inputmode="numeric" />';
          return '<div class="class4-item faultflags-item">'
            + '<label for="' + field.key + '">' + field.label + ' <span class="badge">ID ' + field.otId + '</span></label>'
            + '<div id="' + field.key + '" class="faultflags-box">'
            + '<div class="faultflags-oem"><label for="' + field.key + '_oem">OEM fault code</label>' + oemControl + '</div>'
            + '<div class="bitfield-box faultflags-bits">' + bitsHtml + '</div>'
            + '</div>'
            + '</div>';
        }
        if (field.kind === "u8pair") {
          const parts = field.parts || [];
          const pairHtml = parts.map((part) => {
            const partUnit = part.unit ? ' <span class="unit">' + part.unit + '</span>' : "";
            return '<div class="pair-value" id="' + field.key + '_' + part.slot + '"><strong>' + part.label + '</strong><span>--</span>' + partUnit + '</div>';
          }).join("");
          return '<div class="class4-item">'
            + '<label for="' + field.key + '">' + field.label + ' <span class="badge">ID ' + field.otId + '</span></label>'
            + '<div class="pair-values">' + pairHtml + '</div>'
            + '</div>';
        }
        const controlHtml = field.readOnly
          ? '<span id="' + field.key + '" class="readonly-value"></span>'
          : '<input id="' + field.key + '" type="number" step="' + field.step + '" min="' + field.min + '" max="' + field.max + '" inputmode="decimal" />';

        return '<div class="class4-item">'
          + '<label for="' + field.key + '">' + field.label + unitHtml + ' <span class="badge">ID ' + field.otId + '</span></label>'
          + controlHtml
          + '</div>';
      }).join("");
    }

    function decodePairValue(rawValue) {
      if (rawValue === null || rawValue === undefined) {
        return { high: "", low: "" };
      }
      if (typeof rawValue === "number") {
        return {
          high: String((rawValue >> 8) & 0xFF),
          low: String(rawValue & 0xFF),
        };
      }
      if (typeof rawValue === "object") {
        const highRaw = rawValue.setpoint ?? rawValue.high ?? rawValue.high_byte;
        const lowRaw = rawValue.actual ?? rawValue.low ?? rawValue.low_byte;
        return {
          high: highRaw === undefined ? "" : String(parseRawUint16(highRaw) & 0xFF),
          low: lowRaw === undefined ? "" : String(parseRawUint16(lowRaw) & 0xFF),
        };
      }
      return { high: "", low: "" };
    }

    function writeFaultFlagsValue(field, rawValue) {
      const container = document.getElementById(field.key);
      if (!container) {
        return;
      }

      const value = parseRawUint16(rawValue);
      const upper = (value >> 8) & 0xFF;
      const oem = value & 0xFF;
      const flags = getFaultFlagsDef(field);

      for (const flag of flags) {
        const checkbox = container.querySelector(`input[data-bit="${flag.bit}"]`);
        if (checkbox) {
          checkbox.checked = (upper & (1 << flag.bit)) !== 0;
        }
      }

      const oemEl = document.getElementById(field.key + "_oem");
      if (oemEl) {
        if (field.readOnly) {
          oemEl.textContent = String(oem);
        } else {
          oemEl.value = String(oem);
        }
      }
    }

    function readFaultFlagsValue(field) {
      const container = document.getElementById(field.key);
      if (!container) {
        return 0;
      }

      let upper = 0;
      const flags = getFaultFlagsDef(field);
      for (const flag of flags) {
        const checkbox = container.querySelector(`input[data-bit="${flag.bit}"]`);
        if (checkbox && checkbox.checked) {
          upper |= (1 << flag.bit);
        }
      }

      const oemEl = document.getElementById(field.key + "_oem");
      const oemRaw = oemEl && oemEl.value !== "" ? Number.parseInt(oemEl.value, 10) : 0;
      const oem = Number.isFinite(oemRaw) ? (oemRaw & 0xFF) : 0;

      return ((upper & 0xFF) << 8) | oem;
    }

    function renderClass1Fields() {
      renderValueFields(CLASS1_FIELDS, class1AllFields);
    }

    function renderClass4Fields() {
      renderValueFields(CLASS4_FIELDS, class4AllFields);
    }

    function renderClass5Fields() {
      renderValueFields(CLASS5_FIELDS, class5AllFields);
    }

    function renderClass8Fields() {
      renderValueFields(CLASS8_FIELDS, class8AllFields);
    }

    function decodeOtFloat(value) {
      const raw16 = value & 0xFFFF;
      const signed = (raw16 & 0x8000) ? raw16 - 0x10000 : raw16;
      return Math.round(signed * 10 / 256.0) / 10.0;
    }

    function encodeOtFloat(tempC) {
      return Math.round(tempC * 256.0) & 0xFFFF;
    }

    function encodeFieldValue(field, rawValue) {
      if (field.readOnly) {
        return null;
      }
      if (field.kind === "bitfield") {
        return readBitfieldValue(field);
      }
      if (isFaultFlagsKind(field.kind)) {
        return readFaultFlagsValue(field);
      }
      if (field.kind === "u8") {
        return Number.parseInt(rawValue, 10) & 0xFF;
      }
      if (field.kind === "f8.8") {
        return encodeOtFloat(Number.parseFloat(rawValue));
      }
      return Number.parseInt(rawValue, 10) & 0xFFFF;
    }

    function decodeFieldValue(field, rawValue) {
      if (rawValue === null || rawValue === undefined) {
        return "";
      }
      if (field.kind === "bitfield") {
        return String(parseRawUint16(rawValue));
      }
      if (field.kind === "u8") {
        return String(Number(rawValue) & 0xFF);
      }
      if (isFaultFlagsKind(field.kind)) {
        return String(parseRawUint16(rawValue));
      }
      if (field.kind === "f8.8") {
        return decodeOtFloat(Number(rawValue)).toFixed(1);
      }
      if (field.kind === "s16") {
        const raw16 = Number(rawValue) & 0xFFFF;
        const signed = (raw16 & 0x8000) ? raw16 - 0x10000 : raw16;
        return String(signed);
      }
      return String(Number(rawValue) & 0xFFFF);
    }

    function setStatus(text, isError = false) {
      statusEl.textContent = text;
      statusEl.style.color = isError ? "#992915" : "#415866";
    }

    function setRaw(obj) {
      rawEl.textContent = JSON.stringify(obj, null, 2);
    }

    function syncStatusField(raw) {
      if (Object.prototype.hasOwnProperty.call(raw, STATUS_FIELD.key)) {
        writeBitfieldValue(STATUS_FIELD, raw[STATUS_FIELD.key]);
      } else {
        writeBitfieldValue(STATUS_FIELD, 0);
      }
    }

    function buildPayloadFromInputs() {
      const payload = {
        [STATUS_FIELD.key]: readBitfieldValue(STATUS_FIELD),
      };

      for (const field of VALUE_FIELDS) {
        if (field.readOnly) {
          continue;
        }
        if (isFaultFlagsKind(field.kind)) {
          payload[field.key] = encodeFieldValue(field, null);
          continue;
        }
        const input = document.getElementById(field.key);
        if (!input || input.value === "") {
          continue;
        }
        payload[field.key] = encodeFieldValue(field, input.value);
      }

      return payload;
    }

    async function getValue() {
      setStatus("Loading value...");
      try {
        const resp = await fetch("/api/testdata");
        const data = await resp.json();
        if (!resp.ok) {
          throw new Error(data.detail || "GET failed");
        }

        const raw = data.raw || {};
        const deviceRaw = data.raw_device || {};
        syncStatusField(raw);

        for (const field of VALUE_FIELDS) {
          if (isFaultFlagsKind(field.kind)) {
            const value = Object.prototype.hasOwnProperty.call(raw, field.key)
              ? raw[field.key]
              : 0;
            writeFaultFlagsValue(field, value);
            continue;
          }
          if (field.kind === "u8pair") {
            const pair = decodePairValue(deviceRaw[field.key] ?? raw[field.key]);
            const highEl = document.getElementById(field.key + "_setpoint");
            const lowEl = document.getElementById(field.key + "_actual");
            if (highEl) {
              const highSpan = highEl.querySelector("span:last-child");
              if (highSpan) {
                highSpan.textContent = pair.high;
              }
            }
            if (lowEl) {
              const lowSpan = lowEl.querySelector("span:last-child");
              if (lowSpan) {
                lowSpan.textContent = pair.low;
              }
            }
            continue;
          }
          const input = document.getElementById(field.key);
          if (!input) {
            continue;
          }
          const value = Object.prototype.hasOwnProperty.call(raw, field.key)
            ? decodeFieldValue(field, raw[field.key])
            : "";
          if (field.readOnly) {
            input.textContent = value;
          } else {
            input.value = value;
          }
        }

        if (Object.keys(raw).length > 0) {
          setStatus("Value loaded.");
        } else {
          setStatus("Value not found in response.", true);
        }
        setRaw(data.raw);
      } catch (err) {
        setStatus(String(err), true);
      }
    }

    async function setValue() {
      const payload = buildPayloadFromInputs();

      setStatus("Sending value...");
      try {
        const resp = await fetch("/api/testdata", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload),
        });
        const data = await resp.json();
        if (!resp.ok) {
          throw new Error(data.detail || "POST failed");
        }
        setStatus("Value sent.");
        setRaw(data.raw);
      } catch (err) {
        setStatus(String(err), true);
      }
    }

    document.getElementById("refreshBtn").addEventListener("click", getValue);
    document.getElementById("saveBtn").addEventListener("click", setValue);

    renderBitfield(STATUS_FIELD, statusBitfieldFlags);
    renderClass1Fields();
    renderClass4Fields();
    renderClass5Fields();
    renderClass8Fields();
    getValue();
    </script>
</body>
</html>
"""
    return (
        html.replace("__STATUS_BIT_FLAGS__", status_flags_json)
        .replace("__ASF_BIT_FLAGS__", asf_flags_json)
      .replace("__ASF_VENT_BIT_FLAGS__", asf_vent_flags_json)
        .replace("__STATUS_FIELD__", status_field_json)
      .replace("__CLASS1_FIELDS__", class1_fields_json)
        .replace("__CLASS4_FIELDS__", class4_fields_json)
        .replace("__CLASS5_FIELDS__", class5_fields_json)
      .replace("__CLASS8_FIELDS__", class8_fields_json)
    )


def create_app(device_host: str) -> FastAPI:
    app = FastAPI(title="OTThing Testdata Tool")
    app.state.device_base_url = normalize_base_url(device_host)

    @app.get("/", response_class=HTMLResponse)
    def index() -> str:
        return build_html()

    @app.get("/api/testdata")
    def get_testdata() -> JSONResponse:
        device_raw = call_device_json(app.state.device_base_url, "GET", "/testdata")
        unknown_items = find_unknown_testdata_items(device_raw)
        if unknown_items:
            print("[testdata] unknown GET items:", json.dumps(dict(unknown_items), separators=(",", ":")), flush=True)
        raw = normalize_testdata_raw(device_raw)
        burner_hours = raw.get("burner_op_hours")
        flow_raw = raw.get("flow_t")
        dhw_raw = raw.get("dhw_t")
        flow_c = decode_ot_float(flow_raw) if flow_raw is not None else None
        dhw_c = decode_ot_float(dhw_raw) if dhw_raw is not None else None
        return JSONResponse(
            {
                "burner_op_hours": burner_hours,
                "flow_t_raw": flow_raw,
                "flow_t_c": flow_c,
                "dhw_t_raw": dhw_raw,
                "dhw_t_c": dhw_c,
                "raw": raw,
                "raw_device": device_raw,
            }
        )

    @app.post("/api/testdata")
    def post_testdata(body: dict[str, Any]) -> JSONResponse:
        if not body:
            raise HTTPException(status_code=400, detail="at least one value is required")

        payload: dict[str, Any] = {}

        for key, value in body.items():
            if value is None:
                continue

            try:
                payload[key] = encode_ot_value(key, value)
            except ValueError as exc:
                raise HTTPException(
                status_code=400,
                detail=f"invalid value for '{key}' ({value!r}): {exc}",
              ) from exc

        if not payload:
            raise HTTPException(status_code=400, detail="at least one value is required")

        response = call_device_json(app.state.device_base_url, "POST", "/testdata", payload)
        return JSONResponse({"sent": payload, "raw": response})

    return app


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Set/get OTThing testdata via a small web frontend")
    parser.add_argument("--device-host", default="4.3.2.1", help="host where /testdata is available")
    parser.add_argument("--host", default="127.0.0.1", help="local host for this web tool")
    parser.add_argument("--port", type=int, default=8080, help="local port for this web tool")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    app = create_app(args.device_host)
    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
