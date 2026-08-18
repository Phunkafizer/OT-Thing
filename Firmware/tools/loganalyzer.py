"""OTThing log analyzer.

Connects to the firmware websocket (/ws), parses OpenTherm frames from the log
lines and serves a small web frontend showing raw lines and interpreted data.
The operating mode (master/repeater) is read once from the device /config
endpoint and can be overridden with --mode.

Usage:
    python tools/loganalyzer.py [--device otthing.local] [--port 8080] [--mode master]
"""

from __future__ import annotations

import argparse
import asyncio
import json
import re
import threading
import time
import urllib.request
import webbrowser
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager, suppress
from typing import Any

import uvicorn
import websockets
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse

# source char + 8 hex digits, optional trailing text already decoded by firmware
LINE_RE = re.compile(r"^([A-Za-z])([0-9A-Fa-f]{8})(?:\s+(.*))?$")

MSG_TYPES = {
    0: "READ_DATA",
    1: "WRITE_DATA",
    2: "INVALID_DATA",
    3: "RESERVED",
    4: "READ_ACK",
    5: "WRITE_ACK",
    6: "DATA_INVALID",
    7: "UNKNOWN_DATA_ID",
}

# category of each source char, per operating mode (order defines display order)
MODE_SOURCES: dict[str, dict[str, str]] = {
    "master": {
        "T": "OTThing (master) -> boiler",
        "B": "Boiler -> OTThing",
        "S": "Roomunit -> OTThing",
        "P": "OTThing (slave) -> roomunit",
    },
    "repeater": {
        "T": "Roomunit -> OTThing",
        "B": "Boiler -> OTThing",
    },
}
DEFAULT_MODE = "master"

# 'A' is a boiler answer modified by the gateway, 'R' a request forwarded to the boiler
SOURCE_ALIASES = {"A": "B", "R": "T"}


def normalize_source(src: str | None) -> str:
    if not src:
        return "?"
    up = src.upper()
    return SOURCE_ALIASES.get(up, up)


def source_name(src: str | None) -> str:
    if not src:
        return "other"
    return MODE_SOURCES[settings["mode"]].get(normalize_source(src), f"other ({src})")


def source_order(src: str | None) -> int:
    order = list(MODE_SOURCES[settings["mode"]])
    key = normalize_source(src)
    return order.index(key) if key in order else len(order)


def categories() -> list[dict[str, str]]:
    cats = [{"source": s, "name": n} for s, n in MODE_SOURCES[settings["mode"]].items()]
    cats.append({"source": "?", "name": "other"})
    return cats


MAX_HISTORY = 500

# bit definitions per message id, keyed by byte ("hb"/"lb"), bit -> name
FLAG_BITS: dict[int, dict[str, dict[int, str]]] = {
    0: {
        "hb": {0: "CH enable", 1: "DHW enable", 2: "cooling enable", 3: "OTC active",
               4: "CH2 enable", 5: "summer mode", 6: "DHW blocking"},
        "lb": {0: "fault", 1: "CH mode", 2: "DHW mode", 3: "flame", 4: "cooling",
               5: "CH2 mode", 6: "diagnostic"},
    },
    2: {"hb": {0: "smart power"}},
    3: {
        "hb": {0: "DHW present", 1: "control type on/off", 2: "cooling", 3: "DHW storage",
               4: "low-off & pump ctrl", 5: "CH2 present", 6: "remote water filling",
               7: "heat/cool mode ctrl"},
    },
    5: {"hb": {0: "service request", 1: "lockout-reset", 2: "low water pressure",
               3: "gas/flame fault", 4: "air pressure fault", 5: "water over-temp"}},
    6: {
        "hb": {0: "DHW setpoint transfer", 1: "max CH setpoint transfer"},
        "lb": {0: "DHW setpoint writable", 1: "max CH setpoint writable"},
    },
    70: {
        "hb": {0: "ventilation enable", 1: "bypass position", 2: "bypass mode",
               3: "free ventilation mode"},
        "lb": {0: "fault", 1: "ventilation mode", 2: "bypass status",
               3: "bypass automatic", 4: "free ventilation status", 6: "diagnostic"},
    },
    100: {"hb": {0: "manual change priority", 1: "program change priority"}},
}

# clear text names (see include/otvalues.h)
OT_LABELS: dict[int, str] = {
    0: "Master/slave status",
    1: "Control setpoint (Tset)",
    2: "Master configuration",
    3: "Slave configuration",
    4: "Remote request",
    5: "ASF flags / OEM fault code",
    6: "Remote-parameter transfer-enable flags",
    7: "Cooling control signal",
    8: "Control setpoint 2 (TsetCH2)",
    9: "Remote override room setpoint",
    10: "Number of transparent slave parameters",
    11: "TSP index/value",
    12: "Size of fault buffer",
    13: "FHB entry index/value",
    14: "Maximum relative modulation level setting",
    15: "Max boiler capacity & min modulation level",
    16: "Room setpoint",
    17: "Relative modulation level",
    18: "CH water pressure",
    19: "DHW flow rate",
    20: "Day of week & time of day",
    21: "Date",
    22: "Year",
    23: "Room setpoint CH2",
    24: "Room temperature",
    25: "Boiler water temperature",
    26: "DHW temperature",
    27: "Outside temperature",
    28: "Return water temperature",
    29: "Solar storage temperature",
    30: "Solar collector temperature",
    31: "Flow temperature CH2",
    32: "DHW2 temperature",
    33: "Exhaust temperature",
    34: "Boiler heat exchanger temperature",
    35: "Boiler fan speed (setpoint/actual)",
    36: "Flame current",
    37: "Room temperature CH2",
    38: "Relative humidity",
    39: "Remote override room setpoint 2",
    48: "DHW setpoint upper/lower bound",
    49: "Max CH setpoint upper/lower bound",
    56: "DHW setpoint",
    57: "Max CH water setpoint",
    70: "Master status ventilation/heat-recovery",
    71: "Relative ventilation position (Vset)",
    72: "ASF flags / OEM fault code (ventilation)",
    73: "OEM diagnostic code ventilation/heat-recovery",
    74: "Configuration ventilation/heat-recovery",
    75: "OpenTherm version ventilation/heat-recovery",
    76: "Ventilation product version & type",
    77: "Relative ventilation",
    78: "Relative humidity (exhaust)",
    79: "CO2 level",
    80: "Supply inlet temperature",
    81: "Supply outlet temperature",
    82: "Exhaust inlet temperature",
    83: "Exhaust outlet temperature",
    84: "Actual exhaust fan speed",
    85: "Actual inlet fan speed",
    86: "Remote-parameter flags ventilation/heat-recovery",
    87: "Nominal ventilation value",
    88: "Number of TSPs ventilation/heat-recovery",
    89: "TSP index/value ventilation/heat-recovery",
    90: "Size of fault buffer ventilation/heat-recovery",
    91: "FHB entry index/value ventilation/heat-recovery",
    93: "Brand index",
    94: "Brand version index",
    95: "Brand serial number index",
    96: "Cooling operation hours",
    97: "Power cycles",
    98: "Type of sensor",
    99: "Remote override operating mode heating",
    100: "Remote override room setpoint function",
    101: "Solar storage status",
    102: "Solar storage fault flags",
    103: "Solar storage configuration",
    104: "Solar storage product version & type",
    105: "Number of TSPs solar storage",
    106: "TSP index/value solar storage",
    107: "Size of fault buffer solar storage",
    108: "FHB entry index/value solar storage",
    109: "Electricity producer starts",
    110: "Electricity producer hours",
    111: "Electricity production",
    112: "Cumulative electricity production",
    113: "Number of un-successful burner starts",
    114: "Number of times flame signal was too low",
    115: "OEM diagnostic code",
    116: "Successful burner starts",
    117: "CH pump starts",
    118: "DHW pump/valve starts",
    119: "DHW burner starts",
    120: "Burner operation hours",
    121: "CH pump operation hours",
    122: "DHW pump/valve operation hours",
    123: "DHW burner operation hours",
    124: "OpenTherm version master",
    125: "OpenTherm version slave",
    126: "Master product version & type",
    127: "Slave product version & type",
}

# id -> (name, type, unit, group)
OT_IDS: dict[int, tuple[str, str, str, str]] = {
    0: ("Status", "flag8/flag8", "", "control"),
    1: ("TSet", "f8.8", "C", "control"),
    2: ("MConfigMMemberIDcode", "flag8/u8", "", "config"),
    3: ("SConfigSMemberIDcode", "flag8/u8", "", "config"),
    4: ("RemoteRequest", "u8/u8", "", "config"),
    5: ("ASFflags", "flag8/u8", "", "config"),
    6: ("RBPflags", "flag8/flag8", "", "config"),
    7: ("CoolingControl", "f8.8", "%", "control"),
    8: ("TsetCH2", "f8.8", "C", "control"),
    9: ("TrOverride", "f8.8", "C", "control"),
    10: ("TSP", "u8/u8", "", "config"),
    11: ("TSPindexTSPvalue", "u8/u8", "", "config"),
    12: ("FHBsize", "u8/u8", "", "config"),
    13: ("FHBindexFHBvalue", "u8/u8", "", "config"),
    14: ("MaxRelModLevelSetting", "f8.8", "%", "control"),
    15: ("MaxCapacityMinModLevel", "u8/u8", "", "config"),
    16: ("TrSet", "f8.8", "C", "control"),
    17: ("RelModLevel", "f8.8", "%", "sensor"),
    18: ("CHPressure", "f8.8", "bar", "sensor"),
    19: ("DHWFlowRate", "f8.8", "l/min", "sensor"),
    20: ("DayTime", "special", "", "config"),
    21: ("Date", "u8/u8", "", "config"),
    22: ("Year", "u16", "", "config"),
    23: ("TrSetCH2", "f8.8", "C", "control"),
    24: ("Tr", "f8.8", "C", "sensor"),
    25: ("Tboiler", "f8.8", "C", "sensor"),
    26: ("Tdhw", "f8.8", "C", "sensor"),
    27: ("Toutside", "f8.8", "C", "sensor"),
    28: ("Tret", "f8.8", "C", "sensor"),
    29: ("Tstorage", "f8.8", "C", "sensor"),
    30: ("Tcollector", "f8.8", "C", "sensor"),
    31: ("TflowCH2", "f8.8", "C", "sensor"),
    32: ("Tdhw2", "f8.8", "C", "sensor"),
    33: ("Texhaust", "s16", "C", "sensor"),
    34: ("TboilerHeatExchanger", "f8.8", "C", "sensor"),
    35: ("BoilerFanSpeedSetpointAndActual", "u8/u8", "", "sensor"),
    36: ("FlameCurrent", "f8.8", "uA", "sensor"),
    37: ("TrCH2", "f8.8", "C", "sensor"),
    38: ("RelativeHumidity", "f8.8", "%", "sensor"),
    39: ("TrOverride2", "f8.8", "C", "control"),
    48: ("TdhwSetUBTdhwSetLB", "s8/s8", "C", "config"),
    49: ("MaxTSetUBMaxTSetLB", "s8/s8", "C", "config"),
    56: ("TdhwSet", "f8.8", "C", "control"),
    57: ("MaxTSet", "f8.8", "C", "control"),
    70: ("StatusVentilationHeatRecovery", "flag8/flag8", "", "ventilation"),
    71: ("Vset", "-/u8", "%", "ventilation"),
    72: ("ASFflagsOEMfaultCodeVent", "flag8/u8", "", "ventilation"),
    73: ("OEMDiagnosticCodeVent", "u16", "", "ventilation"),
    74: ("SConfigSMemberIDCodeVent", "flag8/u8", "", "ventilation"),
    75: ("OpenThermVersionVent", "version", "", "ventilation"),
    76: ("VentilationHeatRecoveryVersion", "version", "", "ventilation"),
    77: ("RelVentLevel", "-/u8", "%", "ventilation"),
    78: ("RHexhaust", "-/u8", "%", "ventilation"),
    79: ("CO2exhaust", "u16", "ppm", "ventilation"),
    80: ("Tsi", "f8.8", "C", "ventilation"),
    81: ("Tso", "f8.8", "C", "ventilation"),
    82: ("Tei", "f8.8", "C", "ventilation"),
    83: ("Teo", "f8.8", "C", "ventilation"),
    84: ("RPMexhaust", "u16", "rpm", "ventilation"),
    85: ("RPMsupply", "u16", "rpm", "ventilation"),
    86: ("RBPflagsVent", "flag8/flag8", "", "ventilation"),
    87: ("NominalVentilationValue", "u8/-", "%", "ventilation"),
    88: ("TSPvent", "u8/u8", "", "ventilation"),
    89: ("TSPindexTSPvalueVent", "u8/u8", "", "ventilation"),
    90: ("FHBsizeVent", "u8/u8", "", "ventilation"),
    91: ("FHBindexFHBvalueVent", "u8/u8", "", "ventilation"),
    93: ("Brand", "u8/u8", "", "config"),
    94: ("BrandVersion", "u8/u8", "", "config"),
    95: ("BrandSerialNumber", "u8/u8", "", "config"),
    96: ("CoolingOperationHours", "u16", "h", "counter"),
    97: ("PowerCycles", "u16", "", "counter"),
    98: ("RFsensorStatusInformation", "special", "", "config"),
    99: ("RemoteOverrideOperatingMode", "special", "", "config"),
    100: ("RemoteOverrideFunction", "flag8/-", "", "config"),
    101: ("StatusSolarStorage", "flag8/flag8", "", "solar"),
    102: ("ASFflagsOEMfaultCodeSolar", "flag8/u8", "", "solar"),
    103: ("SConfigSMemberIDcodeSolar", "flag8/u8", "", "solar"),
    104: ("SolarStorageVersion", "version", "", "solar"),
    105: ("TSPSolarStorage", "u8/u8", "", "solar"),
    106: ("TSPindexTSPvalueSolar", "u8/u8", "", "solar"),
    107: ("FHBsizeSolarStorage", "u8/u8", "", "solar"),
    108: ("FHBindexFHBvalueSolar", "u8/u8", "", "solar"),
    109: ("ElectricityProducerStarts", "u16", "", "counter"),
    110: ("ElectricityProducerHours", "u16", "h", "counter"),
    111: ("ElectricityProduction", "u16", "W", "counter"),
    112: ("CumulativElectricityProduction", "u16", "kWh", "counter"),
    113: ("UnsuccessfulBurnerStarts", "u16", "", "counter"),
    114: ("FlameSignalTooLowNumber", "u16", "", "counter"),
    115: ("OEMDiagnosticCode", "u16", "", "counter"),
    116: ("SuccessfulBurnerStarts", "u16", "", "counter"),
    117: ("CHPumpStarts", "u16", "", "counter"),
    118: ("DHWPumpValveStarts", "u16", "", "counter"),
    119: ("DHWBurnerStarts", "u16", "", "counter"),
    120: ("BurnerOperationHours", "u16", "h", "counter"),
    121: ("CHPumpOperationHours", "u16", "h", "counter"),
    122: ("DHWPumpValveOperationHours", "u16", "h", "counter"),
    123: ("DHWBurnerOperationHours", "u16", "h", "counter"),
    124: ("OpenThermVersionMaster", "version", "", "config"),
    125: ("OpenThermVersionSlave", "version", "", "config"),
    126: ("MasterVersion", "version", "", "config"),
    127: ("SlaveVersion", "version", "", "config"),
}

COUNTED_TYPES = ("READ_DATA", "READ_ACK", "WRITE_DATA", "WRITE_ACK")
# WRITE_ACK echoes the accepted value, so it carries data too
UPDATING_TYPES = ("READ_ACK", "WRITE_DATA", "WRITE_ACK")
# status messages carry the master flags already in the READ request
READ_CARRIES_DATA = (0, 70, 101)


def _s8(v: int) -> int:
    return v - 0x100 if v & 0x80 else v


def _s16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


def id_info(data_id: int) -> tuple[str, str, str, str]:
    name, vtype, unit, group = OT_IDS.get(data_id, (f"ID {data_id}", "u16", "", "unknown"))
    return name, vtype, "\u00b0C" if unit == "C" else unit, group


def id_label(data_id: int) -> str:
    return OT_LABELS.get(data_id, id_info(data_id)[0])


def decode_bits(data_id: int, byte: str, value: int) -> list[dict[str, Any]]:
    names = FLAG_BITS.get(data_id, {}).get(byte, {})
    return [
        {"bit": b, "byte": byte, "name": names.get(b, f"{byte}.b{b}"),
         "set": bool(value & (1 << b))}
        for b in range(7, -1, -1)
    ]


def decode_value(data_id: int, data: int) -> dict[str, Any]:
    """Decode the 16 bit payload according to the type of the message id."""
    name, vtype, unit, group = id_info(data_id)
    hb = (data >> 8) & 0xFF
    lb = data & 0xFF
    out: dict[str, Any] = {
        "id": data_id,
        "name": id_label(data_id),
        "code": name,
        "type": vtype,
        "unit": unit,
        "group": group,
        "value": None,
        "text": "",
        "bits": None,
    }

    if vtype == "f8.8":
        val = _s16(data) / 256.0
        out["value"] = val
        out["text"] = f"{val:.2f} {unit}".strip()
    elif vtype == "u16":
        out["value"] = data
        out["text"] = f"{data} {unit}".strip()
    elif vtype == "s16":
        out["value"] = _s16(data)
        out["text"] = f"{_s16(data)} {unit}".strip()
    elif vtype == "u8/u8":
        out["value"] = data
        out["text"] = f"HB {hb} / LB {lb}"
    elif vtype == "s8/s8":
        out["value"] = data
        out["text"] = f"HB {_s8(hb)} / LB {_s8(lb)} {unit}".strip()
    elif vtype == "-/u8":
        out["value"] = lb
        out["text"] = f"{lb} {unit}".strip()
    elif vtype == "u8/-":
        out["value"] = hb
        out["text"] = f"{hb} {unit}".strip()
    elif vtype == "version":
        out["value"] = data
        out["text"] = f"{hb}.{lb}"
    elif vtype == "flag8/flag8":
        out["value"] = data
        out["bits"] = decode_bits(data_id, "hb", hb) + decode_bits(data_id, "lb", lb)
        out["text"] = f"{hb:08b} {lb:08b}"
    elif vtype == "flag8/u8":
        out["value"] = data
        out["bits"] = decode_bits(data_id, "hb", hb)
        out["text"] = f"{hb:08b} / {lb}"
    elif vtype == "flag8/-":
        out["value"] = hb
        out["bits"] = decode_bits(data_id, "hb", hb)
        out["text"] = f"{hb:08b}"
    else:  # special / unknown
        out["value"] = data
        out["text"] = f"0x{data:04X} (HB {hb} / LB {lb})"

    return out


def parse_frame(frame: int) -> dict[str, Any]:
    """Split a 32 bit OpenTherm frame into its fields."""
    parity = (frame >> 31) & 0x01
    msg_type = (frame >> 28) & 0x07
    spare = (frame >> 24) & 0x0F
    data_id = (frame >> 16) & 0xFF
    data = frame & 0xFFFF
    hb = (data >> 8) & 0xFF
    lb = data & 0xFF
    parity_ok = bin(frame & 0x7FFFFFFF).count("1") % 2 == parity
    decoded = decode_value(data_id, data)

    return {
        "parity": parity,
        "parityOk": parity_ok,
        "msgType": msg_type,
        "msgTypeName": MSG_TYPES.get(msg_type, "?"),
        "spare": spare,
        "dataId": data_id,
        "dataValue": data,
        "hb": hb,
        "lb": lb,
        "s16": _s16(data),
        "f88": _s16(data) / 256.0,
        "idName": decoded["name"],
        "idCode": decoded["code"],
        "valueType": decoded["type"],
        "decoded": decoded,
        "detail": decoded["text"],
    }


def parse_line(line: str) -> dict[str, Any]:
    entry: dict[str, Any] = {
        "ts": time.time(),
        "raw": line,
        "source": None,
        "sourceName": None,
        "frame": None,
        "text": None,
        "frameInfo": None,
    }
    m = LINE_RE.match(line.strip())
    if not m:
        return entry

    src, hexstr, rest = m.group(1), m.group(2), m.group(3)
    frame = int(hexstr, 16)
    entry["source"] = src
    entry["sourceName"] = source_name(src)
    entry["frame"] = hexstr.upper()
    entry["text"] = rest
    entry["frameInfo"] = parse_frame(frame)
    return entry


class Cards:
    """One record per source category and OpenTherm message id."""

    def __init__(self) -> None:
        self.cards: dict[tuple[str, int], dict[str, Any]] = {}

    def _card(self, src: str, data_id: int) -> dict[str, Any]:
        card = self.cards.get((src, data_id))
        if card is None:
            name, vtype, unit, group = id_info(data_id)
            card = {
                "key": f"{src}:{data_id}",
                "source": src,
                "sourceName": source_name(src),
                "id": data_id,
                "name": id_label(data_id),
                "code": name,
                "type": vtype,
                "unit": unit,
                "group": group,
                "value": None,
                "text": None,
                "bits": None,
                "raw": None,
                "lastUpdate": None,
                "lastSource": None,
                "lastSeen": None,
                "lastMsgType": None,
                "counts": {t: 0 for t in COUNTED_TYPES},
                "other": 0,
            }
            self.cards[(src, data_id)] = card
        return card

    def update(self, entry: dict[str, Any]) -> dict[str, Any] | None:
        f = entry.get("frameInfo")
        if not f:
            return None

        src = normalize_source(entry.get("source"))
        if src not in MODE_SOURCES[settings["mode"]]:
            src = "?"
        card = self._card(src, f["dataId"])
        mt = f["msgTypeName"]
        if mt in card["counts"]:
            card["counts"][mt] += 1
        else:
            card["other"] += 1
        card["lastSeen"] = entry["ts"]
        card["lastMsgType"] = mt

        if mt in UPDATING_TYPES or (mt == "READ_DATA" and f["dataId"] in READ_CARRIES_DATA):
            dec = f["decoded"]
            card["value"] = dec["value"]
            card["text"] = dec["text"]
            card["bits"] = dec["bits"]
            card["raw"] = f"0x{f['dataValue']:04X}"
            card["lastUpdate"] = entry["ts"]
            card["lastSource"] = entry.get("source")
        return card

    def snapshot(self) -> list[dict[str, Any]]:
        keys = sorted(self.cards, key=lambda k: (source_order(k[0]), k[1]))
        return [self.cards[k] for k in keys]


class Hub:
    """Fans out parsed entries to connected browser clients."""

    def __init__(self) -> None:
        self.clients: set[WebSocket] = set()
        self.history: list[dict[str, Any]] = []
        self.cards = Cards()
        self.lock = asyncio.Lock()

    async def publish(self, entry: dict[str, Any]) -> None:
        self.history.append(entry)
        del self.history[:-MAX_HISTORY]
        card = self.cards.update(entry)
        msg: dict[str, Any] = {"type": "entry", "entry": entry}
        if card is not None:
            msg["card"] = card
        await self.broadcast(msg)

    async def clear(self, scope: str = "all") -> None:
        if scope in ("all", "log"):
            self.history.clear()
        if scope in ("all", "cards"):
            self.cards.cards.clear()
        await self.broadcast({"type": "clear", "scope": scope})

    async def broadcast(self, msg: dict[str, Any]) -> None:
        payload = json.dumps(msg)
        async with self.lock:
            dead = []
            for ws in self.clients:
                try:
                    await ws.send_text(payload)
                except Exception:
                    dead.append(ws)
            for ws in dead:
                self.clients.discard(ws)

    async def add(self, ws: WebSocket) -> None:
        async with self.lock:
            self.clients.add(ws)

    async def remove(self, ws: WebSocket) -> None:
        async with self.lock:
            self.clients.discard(ws)


hub = Hub()
settings: dict[str, Any] = {"device": "", "url": "", "mode": DEFAULT_MODE,
                           "modeOverride": None, "rxTimeout": 2.0}


async def device_reader(url: str) -> None:
    timeout = settings["rxTimeout"] or None
    while True:
        try:
            async with websockets.connect(url, ping_interval=20) as ws:
                await hub.publish(parse_line(f"# connected to {url}"))
                while True:
                    try:
                        message = await asyncio.wait_for(ws.recv(), timeout)
                    except asyncio.TimeoutError:
                        await hub.publish(parse_line(f"# no data for {timeout:g} s, reconnecting"))
                        break

                    if isinstance(message, bytes):
                        message = message.decode("utf-8", "replace")
                    for line in message.splitlines():
                        if line.strip():
                            await hub.publish(parse_line(line))
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            await hub.publish(parse_line(f"# connection lost: {exc}"))
            await asyncio.sleep(3)


@asynccontextmanager
async def lifespan(_: FastAPI) -> AsyncIterator[None]:
    task = asyncio.create_task(device_reader(settings["url"])) if settings["url"] else None
    try:
        yield
    finally:
        if task:
            task.cancel()
            with suppress(asyncio.CancelledError):
                await task


app = FastAPI(title="OTThing Log Analyzer", lifespan=lifespan)


@app.websocket("/stream")
async def stream(ws: WebSocket) -> None:
    await ws.accept()
    await hub.add(ws)
    try:
        await ws.send_text(json.dumps({
            "type": "history",
            "mode": settings["mode"],
            "categories": categories(),
            "entries": hub.history,
            "cards": hub.cards.snapshot(),
        }))
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        pass
    except Exception:
        pass
    finally:
        await hub.remove(ws)


PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OTThing Log Analyzer</title>
<style>
html, body { height: 100%; }
body { font-family: Consolas, monospace; margin: 0; background: #111; color: #ddd;
       display: flex; flex-direction: column; overflow: hidden; }
header { padding: 8px 12px; background: #222; display: flex; gap: 12px; align-items: center; }
#status { font-weight: bold; }
#panes { flex: 1; display: flex; flex-direction: column; min-height: 0; }
#cardPane { flex: 1 1 55%; overflow: auto; padding: 8px 12px; min-height: 60px; }
#logPane { flex: 1 1 45%; overflow: auto; min-height: 60px; }
#logBar { position: sticky; top: 0; z-index: 2; display: flex; gap: 12px; align-items: center;
          padding: 6px 12px; background: #1b1b1b; border-bottom: 1px solid #2a2a2a; }
#split { height: 6px; background: #2a2a2a; cursor: row-resize; flex: none; }
#split:hover { background: #8ab4f8; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th, td { text-align: left; padding: 2px 8px; border-bottom: 1px solid #222; white-space: nowrap; }
th { position: sticky; top: 33px; background: #1b1b1b; z-index: 1; }
tr.err td { color: #ff8080; }
tr.hl-id td { background: rgba(138, 180, 248, 0.16); }
tr.hl-type td { background: rgba(126, 231, 135, 0.14); }
tr.hl-id.hl-type td { background: rgba(200, 160, 255, 0.20); }
tr.hl-id td:first-child { box-shadow: inset 3px 0 0 #8ab4f8; }
tr.hl-type td:last-child { box-shadow: inset -3px 0 0 #7ee787; }
td.pick { cursor: pointer; }
td.pick:hover { text-decoration: underline; }
.chip { border: 1px solid rgba(138, 180, 248, 0.6); background: rgba(138, 180, 248, 0.14);
        border-radius: 999px; padding: 1px 8px; font-size: 12px; cursor: pointer; margin-left: 4px; }
.chip:hover { background: rgba(138, 180, 248, 0.3); }
td.raw { color: #8ab4f8; }
td.detail { white-space: normal; color: #aaa; }
h2 { font-size: 13px; margin: 4px 0 8px; color: #888; text-transform: uppercase; }
.cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(230px, 1fr)); gap: 8px; }
.cat { margin-bottom: 14px; }
.cat.folded .cards { display: none; }
.cat h3 { font-size: 12px; margin: 6px 0; color: #8ab4f8; }
.cat h3.cat-head { cursor: pointer; user-select: none; }
.cat h3 .caret { display: inline-block; width: 12px; }
.card { cursor: pointer; }
.card { background: #1b1b1b; border: 1px solid #2a2a2a; border-radius: 6px; padding: 8px; }
.card.upd { border-color: #8ab4f8; }
.card .cid { color: #666; font-size: 11px; }
.card .cname { font-weight: bold; color: #eee; font-size: 13px; word-break: break-all; }
.card .cval { font-size: 19px; margin: 4px 0; color: #7ee787; }
.card .ctype { color: #666; font-size: 11px; }
.card .clast { color: #888; font-size: 11px; margin-top: 4px; }
.counts { display: flex; gap: 5px; font-size: 11px; margin-top: 6px; flex-wrap: wrap; }
.counts span { background: #262626; border-radius: 3px; padding: 1px 5px; color: #bbb; }
.bits { display: flex; flex-wrap: wrap; gap: 3px; margin-top: 4px; }
.bit { font-size: 10px; padding: 1px 4px; border-radius: 3px; background: #262626; color: #777; }
.bit.on { background: #2d4d2d; color: #9f9; }
</style>
</head>
<body>
<header>
  <span id="status">connecting...</span>
  <span id="device"></span>
  <button id="clearCards">clear items</button>
</header>
<div id="panes">
  <div id="cardPane">
    <h2>messages</h2>
    <div id="cats"></div>
  </div>
  <div id="split"></div>
  <div id="logPane">
    <div id="logBar">
      <label><input type="checkbox" id="follow" checked> follow</label>
      <button id="clearLog">clear log</button>
      <span id="filters"></span>
    </div>
    <table>
    <thead><tr>
    <th>time</th><th>raw</th><th>src</th><th>frame</th><th>type</th><th>id</th>
    <th>data</th><th>hb/lb</th><th>f8.8</th><th>parity</th><th>interpretation</th>
    </tr></thead>
    <tbody id="rows"></tbody>
    </table>
  </div>
</div>
<script>
const rows = document.getElementById('rows');
const statusEl = document.getElementById('status');
const followEl = document.getElementById('follow');
const cardPane = document.getElementById('cardPane');
const logPane = document.getElementById('logPane');
document.getElementById('clearCards').onclick = () => {
  fetch('/api/clear?scope=cards', { method: 'POST' }).catch(() => {});
};
document.getElementById('clearLog').onclick = () => {
  fetch('/api/clear?scope=log', { method: 'POST' }).catch(() => {});
};

let dragging = false;
document.getElementById('split').addEventListener('mousedown', () => dragging = true);
document.addEventListener('mouseup', () => dragging = false);
document.addEventListener('mousemove', (ev) => {
  if (!dragging) return;
  const top = document.getElementById('panes').getBoundingClientRect().top;
  const total = document.getElementById('panes').clientHeight;
  const h = Math.min(Math.max(ev.clientY - top, 60), total - 60);
  cardPane.style.flex = '0 0 ' + h + 'px';
  logPane.style.flex = '1 1 auto';
  ev.preventDefault();
});

function cell(text, cls) {
  const td = document.createElement('td');
  if (cls) td.className = cls;
  td.textContent = text === null || text === undefined ? '' : text;
  return td;
}

let selId = null;
let selType = null;
const filtersEl = document.getElementById('filters');

function renderFilters() {
  filtersEl.innerHTML = '';
  const add = (label, value, clear) => {
    const chip = document.createElement('span');
    chip.className = 'chip';
    chip.textContent = label + ': ' + value + ' \u00d7';
    chip.title = 'click to clear';
    chip.onclick = clear;
    filtersEl.appendChild(chip);
  };
  if (selId !== null) add('id', selId, () => { selId = null; applyHighlight(); });
  if (selType !== null) add('type', selType, () => { selType = null; applyHighlight(); });
}

function highlightRow(tr) {
  tr.classList.toggle('hl-id', selId !== null && tr.dataset.id === selId);
  tr.classList.toggle('hl-type', selType !== null && tr.dataset.type === selType);
}

function applyHighlight() {
  Array.from(rows.rows).forEach(highlightRow);
  renderFilters();
}

function setSelection(kind, value) {
  if (kind === 'id') selId = (selId === value) ? null : value;
  else selType = (selType === value) ? null : value;
  applyHighlight();
}

function pickCell(td, kind, value) {
  if (value === null || value === undefined || value === '') return td;
  td.classList.add('pick');
  td.title = 'click to highlight all ' + kind + ' ' + value;
  td.onclick = () => setSelection(kind, value);
  return td;
}

function addEntry(e) {
  const tr = document.createElement('tr');
  const f = e.frameInfo;
  const t = new Date(e.ts * 1000).toLocaleTimeString();
  tr.appendChild(cell(t));
  tr.appendChild(cell(e.raw, 'raw'));
  tr.appendChild(cell(e.source));
  tr.appendChild(cell(e.frame ? '0x' + e.frame : ''));
  tr.appendChild(pickCell(cell(f ? f.msgTypeName : ''), 'type', f ? f.msgTypeName : null));
  tr.appendChild(pickCell(cell(f ? f.dataId + ' ' + f.idCode : ''), 'id', f ? String(f.dataId) : null));
  tr.appendChild(cell(f ? '0x' + f.dataValue.toString(16).padStart(4, '0') : ''));
  tr.appendChild(cell(f ? f.hb + '/' + f.lb : ''));
  tr.appendChild(cell(f ? f.f88.toFixed(2) : ''));
  tr.appendChild(cell(f ? (f.parityOk ? 'ok' : 'BAD') : ''));
  tr.appendChild(cell(f && f.detail ? f.detail : (e.text || ''), 'detail'));
  if (f) {
    tr.dataset.id = String(f.dataId);
    tr.dataset.type = f.msgTypeName;
  }
  highlightRow(tr);
  if (f && !f.parityOk) tr.className = 'err';
  rows.appendChild(tr);
  while (rows.rows.length > 500) rows.deleteRow(0);
  if (followEl.checked) logPane.scrollTop = logPane.scrollHeight;
}

const catsEl = document.getElementById('cats');
const cardEls = new Map();
const gridEls = new Map();

function gridFor(src, name) {
  let grid = gridEls.get(src);
  if (grid) return grid;
  const wrap = document.createElement('div');
  wrap.className = 'cat';
  const h = document.createElement('h3');
  h.className = 'cat-head';
  h.title = 'click to fold/unfold';
  const caret = document.createElement('span');
  caret.className = 'caret';
  caret.textContent = '\u25be';
  h.appendChild(caret);
  h.appendChild(document.createTextNode(src + ' - ' + (name || src)));
  grid = document.createElement('div');
  grid.className = 'cards';
  h.onclick = () => {
    const folded = wrap.classList.toggle('folded');
    caret.textContent = folded ? '\u25b8' : '\u25be';
  };
  wrap.appendChild(h);
  wrap.appendChild(grid);
  catsEl.appendChild(wrap);
  gridEls.set(src, grid);
  return grid;
}

function cardNode(c) {
  let el = cardEls.get(c.key);
  if (el) return el;
  const grid = gridFor(c.source, c.sourceName);
  el = document.createElement('div');
  el.className = 'card';
  el.dataset.id = c.id;
  el.title = 'double-click to highlight this id in the log';
  el.ondblclick = () => setSelection('id', String(c.id));
  el.innerHTML = '<div class="cid"></div><div class="cname"></div>'
    + '<div class="cval"></div><div class="ctype"></div>'
    + '<div class="bits"></div><div class="clast"></div><div class="counts"></div>';
  cardEls.set(c.key, el);
  // keep cards ordered by message id inside the category
  const next = [...grid.children].find(n => Number(n.dataset.id) > c.id);
  grid.insertBefore(el, next || null);
  return el;
}

function updateCard(c, flash) {
  const el = cardNode(c);
  el.querySelector('.cid').textContent = 'ID ' + c.id + (c.group ? ' - ' + c.group : '');
  el.querySelector('.cname').textContent = c.name;
  el.querySelector('.cname').title = c.code || '';
  el.querySelector('.cval').textContent = c.text === null ? '-' : c.text;
  el.querySelector('.ctype').textContent = c.type + (c.raw ? '  ' + c.raw : '');

  const bitsEl = el.querySelector('.bits');
  bitsEl.innerHTML = '';
  (c.bits || []).forEach(b => {
    const s = document.createElement('span');
    s.className = 'bit' + (b.set ? ' on' : '');
    s.textContent = b.name;
    bitsEl.appendChild(s);
  });

  const upd = c.lastUpdate ? new Date(c.lastUpdate * 1000).toLocaleString() : '-';
  el.querySelector('.clast').textContent = upd + '  ' + (c.lastMsgType || '');

  const cEl = el.querySelector('.counts');
  cEl.innerHTML = '';
  const labels = { READ_DATA: 'RD', READ_ACK: 'RA', WRITE_DATA: 'WR', WRITE_ACK: 'WA' };
  Object.keys(labels).forEach(k => {
    const n = c.counts[k] || 0;
    if (n === 0) return;
    const s = document.createElement('span');
    s.textContent = labels[k] + ' ' + n;
    cEl.appendChild(s);
  });

  if (flash) {
    el.classList.add('upd');
    setTimeout(() => el.classList.remove('upd'), 400);
  }
}

function connect() {
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const ws = new WebSocket(proto + '://' + location.host + '/stream');
  ws.onopen = () => statusEl.textContent = 'connected';
  ws.onclose = () => { statusEl.textContent = 'disconnected'; setTimeout(connect, 2000); };
  ws.onmessage = (ev) => {
    const msg = JSON.parse(ev.data);
    if (msg.type === 'history') {
      if (msg.categories) msg.categories.forEach(c => gridFor(c.source, c.name));
      msg.entries.forEach(addEntry);
      (msg.cards || []).forEach(c => updateCard(c, false));
    } else if (msg.type === 'entry') {
      addEntry(msg.entry);
      if (msg.card) updateCard(msg.card, true);
    } else if (msg.type === 'clear') {
      const scope = msg.scope || 'all';
      if (scope === 'all' || scope === 'log') rows.innerHTML = '';
      if (scope === 'all' || scope === 'cards') {
        cardEls.clear();
        gridEls.forEach(grid => grid.innerHTML = '');
      }
    }
  };
}
fetch('/api/info').then(r => r.json()).then(i => {
  document.getElementById('device').textContent = i.device + ' [' + i.mode + ']';
});
connect();
</script>
</body>
</html>
"""


@app.get("/", response_class=HTMLResponse)
async def index() -> str:
    await refresh_mode()
    return PAGE


@app.get("/api/info")
async def info() -> dict[str, Any]:
    return {
        "device": settings["device"],
        "url": settings["url"],
        "mode": settings["mode"],
        "categories": categories(),
    }


@app.get("/api/cards")
async def cards() -> list[dict[str, Any]]:
    return hub.cards.snapshot()


@app.post("/api/clear")
async def clear(scope: str = "all") -> dict[str, str]:
    if scope not in ("all", "log", "cards"):
        scope = "all"
    await hub.clear(scope)
    return {"status": "ok", "scope": scope}


# otmode values reported by the firmware: 1/4 master, 2 repeater (3 = master variant)
OTMODE_TO_MODE = {1: "master", 2: "repeater", 3: "master", 4: "master"}


def fetch_mode(ws_url: str) -> str:
    """Read /config from the device once and derive the operating mode."""
    cfg_url = ws_url.replace("wss://", "https://").replace("ws://", "http://")
    cfg_url = cfg_url.rsplit("/ws", 1)[0] + "/config"
    try:
        with urllib.request.urlopen(cfg_url, timeout=5) as resp:
            cfg = json.loads(resp.read().decode("utf-8", "replace"))
    except Exception as exc:
        print(f"could not read {cfg_url} ({exc}), using mode {DEFAULT_MODE}")
        return DEFAULT_MODE

    otmode = next((cfg[k] for k in cfg if k.lower() == "otmode"), None)
    mode = OTMODE_TO_MODE.get(otmode)
    if mode is None:
        print(f"unknown otmode {otmode!r}, using mode {DEFAULT_MODE}")
        return DEFAULT_MODE
    print(f"otmode {otmode} from {cfg_url} -> {mode} mode")
    return mode


async def refresh_mode() -> str:
    """Re-read the mode from the device unless it was forced on the command line."""
    if settings.get("modeOverride") or not settings["url"]:
        return settings["mode"]

    mode = await asyncio.to_thread(fetch_mode, settings["url"])
    if mode != settings["mode"]:
        settings["mode"] = mode
        await hub.clear("cards")  # card categories depend on the mode
    return mode


def main() -> None:
    ap = argparse.ArgumentParser(description="OTThing websocket log analyzer")
    ap.add_argument("--device", default="otthing.local",
                    help="OTThing host or ip (or full ws:// url), default: otthing.local")
    ap.add_argument("--mode", choices=sorted(MODE_SOURCES), default=None,
                    help="override the mode detected from /config")
    ap.add_argument("--host", default="127.0.0.1", help="bind address for the web frontend")
    ap.add_argument("--port", type=int, default=8080, help="port for the web frontend")
    ap.add_argument("--rx-timeout", type=float, default=2.0,
                    help="reconnect when no data arrives for this many seconds (0 disables)")
    ap.add_argument("--no-browser", action="store_true", help="do not open a browser window")
    args = ap.parse_args()

    dev = args.device
    if dev.startswith(("ws://", "wss://")):
        url = dev
    else:
        url = f"ws://{dev}/ws"
    settings["device"] = dev
    settings["url"] = url
    settings["mode"] = args.mode or fetch_mode(url)
    settings["modeOverride"] = args.mode
    settings["rxTimeout"] = args.rx_timeout

    print(f"reading log from {url} (mode: {settings['mode']})")
    print(f"frontend on http://{args.host}:{args.port}")
    if not args.no_browser:
        host = "127.0.0.1" if args.host in ("0.0.0.0", "::") else args.host
        threading.Timer(1.0, webbrowser.open, [f"http://{host}:{args.port}"]).start()
    uvicorn.run(app, host=args.host, port=args.port, log_level="warning")


if __name__ == "__main__":
    main()
