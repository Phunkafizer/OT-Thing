from pathlib import Path
import asyncio

from fastapi import FastAPI, Request, UploadFile, WebSocket
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, PlainTextResponse

app = FastAPI(title="OTthing Mock Server")

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
INDEX_FILE = DATA_DIR / "index.html"

state = {
    "config": {
        "hostname": "otthing-mock",
        "haPrefix": "homeassistant",
        "slaveApp": 0,
        "otMode": 4,
        "enableSlave": False,
        "timezone": 3600,
        "boiler": {
            "dhwOn": True,
            "dhwTemperature": 50,
            "overrideDhw": False,
            "coolOn": False,
            "otc": False,
            "summerMode": False,
            "dhwBlocking": False,
            "maxModulation": 100,
        },
        "heating": [
            {
                "curveMode": 0,
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
                "enableHyst": False,
                "hysteresis": 0.5,
                "minSuspend": False,
                "suspOffset": 0.0,
                "returnLimit": {"source": 1, "deltaT": 0.0},
            },
            {
                "curveMode": 0,
                "flow": 35,
                "chOn": True,
                "flowMax": 45,
                "flowMin": 10,
                "exponent": 1.3,
                "gradient": 1.0,
                "offset": 0,
                "marker": [],
                "roomsetpoint": {"source": 0, "temp": 22},
                "roomtemp": {"source": 1},
                "overrideFlow": False,
                "enableHyst": False,
                "hysteresis": 0.5,
                "minSuspend": False,
                "suspOffset": 0.0,
                "returnLimit": {"source": 1, "deltaT": 0.0},
            },
        ],
        "vent": {
            "ventEnable": False,
            "openBypass": False,
            "autoBypass": False,
            "freeVentEnable": False,
            "setpoint": 3,
        },
        "outsideTemp": {
            "source": 1,
            "lat": 49.4771,
            "lon": 10.9887,
            "interval": 300,
        },
        "mqtt": {
            "host": "",
            "tls": False,
            "port": 1883,
            "user": "",
            "pass": "",
            "keepAlive": 15,
        },
        "masterMemberId": 8,
        "aux": [{"mode": 4}, {"mode": 0}],
    },
    "status": {
        "fw_version": "mock-1.0.0",
        "runtime": 0,
        "freeHeap": 62388,
        "largestBlock": 31732,
        "resetInfo": 21,
        "reset_reason0": 21,
        "USB_connected": False,
        "numWifiDisc": 0,
        "dateTime": "23.04.2026 22:35:59",
        "outsideTemp": 8.4,
        "wifi": {
            "status": 3,
            "mode": 1,
            "mac": "AA:BB:CC:DD:EE:FF",
            "ipsta": "192.168.1.50",
            "sta_ssid": "MockWiFi",
            "hostname": "otthing-mock",
            "rssi": -54,
        },
        "mqtt": {
            "connected": True,
            "numDisc": 0,
            "basetopic": "otthing/mock",
        },
        "master": {
            "status": {
                "value": "1a00",
                "ch_enable": False,
                "ch2_enable": True,
                "dhw_enable": True,
                "cooling_enable": False,
                "otc_active": True,
            },
            "vent_status": {
                "value": "100",
                "vent_enable": True,
                "open_bypass": False,
                "auto_bypass": False,
                "free_vent_enable": False,
            },
            "ch_set_t": 44.0,
            "ch_set_t2": 22.3,
            "dhw_set_t": 49.0,
            "max_set_t": 65.0,
            "room_t": 20.1,
            "room_set_t": 21.3,
            "room_t2": 21.1,
            "room_set_t2": 22.3,
            "outside_t": 15.0,
            "max_rel_mod": 100,
            "rel_vent_set": 50,
            "memberIdOk": True,
            "master_ot_version": "4.2",
            "master_prod_version": "1.0",
            "master_config_member": {
                "value": "108",
                "memberId": 8,
                "smartPowerImplemented": True,
            },
            "smartPower": "low",
            "txCount": 1000,
            "rxCount": 995,
            "invalidCount": 0,
        },
        "slave": {
            "status": {
                "value": "24",
                "fault": False,
                "flame": False,
                "diagnostic": False,
                "ch_mode": False,
                "ch2_mode": True,
                "dhw_mode": True,
                "cooling": False,
            },
            "vent_status": {
                "value": "1e",
                "fault": False,
                "vent_active": True,
                "bypass_open": True,
                "bypass_auto": True,
                "free_vent": True,
                "diagnostic": False,
            },
            "slave_config_member": {
                "value": "2501",
                "ch2_present": True,
                "dhw_present": True,
                "cooling_config": True,
                "dhw_config": False,
                "master_lowoff_pumpctrl": False,
                "memberId": 1,
                "ctrl_type": False,
            },
            "slave_ot_version": "2.2",
            "slave_prod_version": "4.4",
            "max_cap_min_mod": {
                "max_capacity": 20,
                "min_modulation": 5,
            },
            "dhw_bounds": {"max": 60, "min": 40},
            "ch_bounds": {"max": 60, "min": 25},
            "rp_flags": {
                "value": "101",
                "dhw_setpoint_rw": True,
                "max_ch_setpoint_rw": False,
                "dhw_setpoint_trans": True,
                "max_ch_setpoint_trans": False,
            },
            "remote_override_function": {
                "value": "0",
                "manual_change_priority": False,
                "program_change_priority": False,
            },
            "fault_flags": {
                "value": "0",
                "service_request": False,
                "lockout_reset": False,
                "low_water_pressure": False,
                "gas_flame_fault": False,
                "air_pressure_fault": False,
                "water_over_temp": False,
                "oem_fault_code": 0,
            },
            "vent_fault_flags": {
                "value": "f33",
                "service_request": True,
                "exhaust_fan_fault": True,
                "inlet_fan_fault": True,
                "frost_protection": True,
                "oem_vent_fault_code": 51,
            },
            "flow_t": 48.5,
            "flow_t2": 48.6,
            "dhw_t": 37.5,
            "dhw_t2": 37.6,
            "outside_t": 15.0,
            "return_t": 41.7,
            "exhaust_t": 90.0,
            "boiler_heat_ex_t": 48.5,
            "ch_pressure": 1.3,
            "dhw_flow_rate": 2.4,
            "tr_override": 0,
            "tr_override2": 0,
            "rel_mod": 33.3,
            "rel_vent": 55,
            "rel_hum_exhaust": 45,
            "co2_exhaust": 1450,
            "supply_inlet_t": 22.1,
            "supply_outlet_t": 22.2,
            "exhaust_inlet_t": 22.3,
            "exhaust_outlet_t": 22.1,
            "exhaust_fan_speed": 2300,
            "supply_fan_speed": 2400,
            "boiler_fan": {"setpoint": 20, "actual": 21},
            "flame_current": 96.8,
            "oem_diag_code": 123,
            "power_cycles": 159,
            "unsuccessful_burner_starts": 19,
            "num_flame_signal_low": 4,
            "burner_starts": 9999,
            "ch_pump_starts": 7777,
            "dhw_pump_starts": 5544,
            "dhw_burner_starts": 9955,
            "burner_op_hours": 8888,
            "chpump_op_hours": 6666,
            "dhwpump_op_hours": 5555,
            "dhw_burner_op_hours": 2222,
            "vent_ot_version": "1.5",
            "vent_prod_version": "1.7",
            "brand": "Seegel Systeme",
            "brand_version": "mock-1.0.0",
            "brand_serial": "AA:BB:CC:DD:EE:FF",
            "connected": True,
            "txCount": 1000,
            "rxCount": 995,
            "timeouts": 0,
            "flameStats": {
                "duty": 65.0,
                "lastOnTime": 6.9,
                "onTime": 6.9,
                "offTime": 3.8,
                "freq": 5.7,
            },
        },
        "heatercircuit": [
            {
                "ovrdTemp": False,
                "ovrdOn": False,
                "ctrlMode": "auto",
                "roomMode": "heat",
                "action": "off",
                "roomAction": "off",
                "roomsetpoint": 17.0,
                "roomtemp": 20.1,
                "suspended": True,
                "roomcompInteg": 0.0,
                "retLimitInteg": 0.0,
                "flowMin": 25,
                "reduction": 0.0,
                "returnTemp": 41.7,
            },
            {
                "ovrdTemp": False,
                "ovrdOn": False,
                "ctrlMode": "auto",
                "roomMode": "heat",
                "action": "idle",
                "roomAction": "off",
                "roomsetpoint": 26.0,
                "roomtemp": 21.1,
                "suspended": False,
                "roomcompInteg": 0.0,
                "retLimitInteg": 0.0,
                "flowMin": 20,
                "reduction": 0.0,
                "returnTemp": 41.7,
            },
        ],
        "dhw": {
            "ovrd": False,
            "ctrlMode": "heat",
            "action": "heating",
        },
        "bypass": False,
        "1wire": {
            "28FF001": 21.1,
            "28FF002": 20.7,
        },
        "BLE": {
            "04a86a8d1f38": {"temp": 13.2, "rh": 38, "bat": 100, "rssi": -100},
            "1bc8dc8d1f38": {"temp": 19.2, "rh": 43, "bat": 30, "rssi": -94},
        },
        "aux1": {},
        "aux2": {},
    },
}


def ensure_heatercircuit(idx: int) -> None:
    while len(state["status"]["heatercircuit"]) <= idx:
        state["status"]["heatercircuit"].append({})


@app.get("/")
def get_index() -> FileResponse:
    return FileResponse(INDEX_FILE)


@app.get("/status")
def get_status() -> JSONResponse:
    state["status"]["runtime"] += 1
    return JSONResponse(state["status"])


@app.get("/config")
def get_config() -> JSONResponse:
    return JSONResponse(state["config"])


@app.post("/config")
async def post_config(request: Request) -> PlainTextResponse:
    state["config"] = await request.json()
    return PlainTextResponse("ok")


@app.post("/setwifi")
async def post_setwifi(request: Request) -> JSONResponse:
    form = await request.form()
    ssid = str(form.get("ssid", ""))
    if ssid:
        state["status"]["wifi"]["sta_ssid"] = ssid
    return JSONResponse({"ok": True})


@app.get("/scan")
def get_scan() -> JSONResponse:
    return JSONResponse(
        {
            "status": 1,
            "results": [
                {"ssid": "MockNet", "channel": 1, "rssi": -54},
                {"ssid": "MockGuest", "channel": 6, "rssi": -71},
                {"ssid": "MockIoT", "channel": 11, "rssi": -78},
            ],
        }
    )


@app.get("/set")
def get_set(
    roomSetTemp: float | None = None,
    roomSetpoint1: float | None = None,
    roomSetpoint2: float | None = None,
    roomMode1: str | None = None,
    roomMode2: str | None = None,
    chSetTemp1: float | None = None,
    chSetTemp2: float | None = None,
    chMode1: str | None = None,
    chMode2: str | None = None,
) -> JSONResponse:
    # Backward-compatible shortcut used earlier in UI experiments.
    if roomSetTemp is not None:
        roomSetpoint1 = roomSetTemp

    if roomSetpoint1 is not None:
        ensure_heatercircuit(0)
        state["status"]["heatercircuit"][0]["roomsetpoint"] = roomSetpoint1

    if roomSetpoint2 is not None:
        ensure_heatercircuit(1)
        state["status"]["heatercircuit"][1]["roomsetpoint"] = roomSetpoint2

    if roomMode1 is not None:
        ensure_heatercircuit(0)
        state["status"]["heatercircuit"][0]["roomMode"] = roomMode1

    if roomMode2 is not None:
        ensure_heatercircuit(1)
        state["status"]["heatercircuit"][1]["roomMode"] = roomMode2

    if chMode1 is not None:
        ensure_heatercircuit(0)
        state["status"]["heatercircuit"][0]["ctrlMode"] = chMode1

    if chMode2 is not None:
        ensure_heatercircuit(1)
        state["status"]["heatercircuit"][1]["ctrlMode"] = chMode2

    effective_ch1_mode = chMode1 or (state["status"]["heatercircuit"][0].get("ctrlMode") if state["status"]["heatercircuit"] else None)
    if chSetTemp1 is not None and effective_ch1_mode in {"heat", "on"}:
        state["status"]["master"]["ch_set_t"] = chSetTemp1

    effective_ch2_mode = chMode2 or (state["status"]["heatercircuit"][1].get("ctrlMode") if len(state["status"]["heatercircuit"]) > 1 else None)
    if chSetTemp2 is not None and effective_ch2_mode in {"heat", "on"}:
        state["status"]["master"]["ch_set_t2"] = chSetTemp2

    return JSONResponse({"ok": True})


@app.get("/reboot")
def get_reboot() -> JSONResponse:
    return JSONResponse({"ok": True})


@app.get("/otitems")
def get_otitems() -> JSONResponse:
    return JSONResponse({"items": [{"id": 0, "name": "Mock item"}]})


@app.get("/slaverequest")
def get_slaverequest(id: int = 0, rw: int = 0, data: str = "0000") -> JSONResponse:
    # data low byte >= 0x80 means accepted in UI
    return JSONResponse({"id": id, "rw": rw, "data": "0080", "request": data})


@app.post("/update")
async def post_update(firmware: UploadFile) -> PlainTextResponse:
    _ = await firmware.read()
    return PlainTextResponse("ok")


@app.websocket("/ws")
async def websocket_logs(ws: WebSocket) -> None:
    await ws.accept()
    i = 0
    try:
        while True:
            i += 1
            await ws.send_text(f"[mock] log line {i}")
            await asyncio.sleep(1.0)
    except Exception:
        pass


# ---------------------------------------------------------------------------
# Admin / mock-control panel
# ---------------------------------------------------------------------------

def _deep_set(obj, path: str, value) -> None:
    """Set a nested value using dot-separated path, e.g. 'slave.status.flame'."""
    keys = path.split(".")
    for k in keys[:-1]:
        obj = obj[int(k)] if isinstance(obj, list) else obj[k]
    last = keys[-1]
    if isinstance(obj, list):
        obj[int(last)] = value
    else:
        obj[last] = value


_ADMIN_HTML = """<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Mock Control</title>
<style>
  *, *::before, *::after { box-sizing: border-box; }
  body { font-family: monospace; background: #12121f; color: #ddd; margin: 0; padding: 20px; }
  h1 { color: #4fc3f7; margin-top: 0; font-size: 1.2em; letter-spacing: 1px; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; }
  .card { background: #1e1e30; border-radius: 8px; padding: 14px 16px; }
  .card h3 { margin: 0 0 10px; font-size: 0.75em; text-transform: uppercase; color: #4fc3f7; letter-spacing: 1px; }
  .row { display: flex; align-items: center; margin-bottom: 6px; gap: 8px; }
  .row label { flex: 1; font-size: 0.82em; color: #aaa; }
  .row input[type=number], .row input[type=text] { width: 88px; background: #0d1b2a; color: #eee; border: 1px solid #334; border-radius: 4px; padding: 3px 6px; font-family: inherit; font-size: 0.82em; }
  .row select { background: #0d1b2a; color: #eee; border: 1px solid #334; border-radius: 4px; padding: 3px 6px; font-family: inherit; font-size: 0.82em; }
  .row input[type=checkbox] { width: 16px; height: 16px; accent-color: #4fc3f7; }

  #toast { position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%); background: #0f9b8e; color: #fff; padding: 7px 22px; border-radius: 20px; font-size: 0.88em; opacity: 0; transition: opacity 0.3s; pointer-events: none; white-space: nowrap; }
  #toast.show { opacity: 1; }
  #toast.err { background: #ef5350; }
  #reload-btn { margin-bottom: 16px; background: #333; color: #aaa; border: 1px solid #444; padding: 5px 14px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.82em; }
  #reload-btn:hover { background: #444; color: #fff; }
  .load-btn { background: #1a2a3a; color: #4fc3f7; border: 1px solid #334; padding: 5px 14px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.82em; margin-bottom: 16px; margin-right: 8px; }
  .load-btn:hover { background: #0d1b2a; color: #fff; }
    .quick-set { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; margin-bottom: 14px; padding: 10px; border: 1px solid #334; border-radius: 8px; background: #1a1a2a; }
    .quick-set input[type=text], .quick-set select { background: #0d1b2a; color: #eee; border: 1px solid #334; border-radius: 4px; padding: 5px 8px; font-family: inherit; font-size: 0.82em; }
    .quick-set .path-input { flex: 2; min-width: 260px; }
    .quick-set .value-input { flex: 1; min-width: 180px; }
    .quick-set .quick-btn { background: #1a2a3a; color: #4fc3f7; border: 1px solid #334; padding: 5px 14px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.82em; }
    .quick-set .quick-btn:hover { background: #0d1b2a; color: #fff; }
    .quick-set .hint { width: 100%; font-size: 0.78em; color: #889; }
</style>
</head>
<body>
<h1>OTThing Mock Control Panel</h1>
<button id="reload-btn" onclick="reload()">↺ Reload values from /status</button>
<button class="load-btn" onclick="document.getElementById('file-status').click()">📂 Load status JSON</button>
<input type="file" id="file-status" accept=".json,application/json" style="display:none" onchange="loadJsonFile(this,'status')">
<button class="load-btn" onclick="document.getElementById('file-config').click()">📂 Load config JSON</button>
<input type="file" id="file-config" accept=".json,application/json" style="display:none" onchange="loadJsonFile(this,'config')">
<div class="quick-set">
    <input id="customPath" class="path-input" list="statusPathList" type="text" placeholder="status path, e.g. slave.status.flame">
    <datalist id="statusPathList"></datalist>
    <select id="customType" title="value type">
        <option value="auto" selected>auto</option>
        <option value="bool">bool</option>
        <option value="number">number</option>
        <option value="string">string</option>
        <option value="json">json</option>
    </select>
    <input id="customValue" class="value-input" type="text" placeholder="value">
    <button class="quick-btn" onclick="setCustomPath()">Apply</button>
    <div class="hint">Use dot notation paths (for arrays: heatercircuit.0.roomsetpoint). Values can be auto-detected or forced with the type selector.</div>
</div>
<div class="grid" id="grid"></div>
<div id="toast"></div>
<script>
const FIELDS = [
  { section: "General", rows: [
    { key: "outsideTemp",  label: "Outside temp (°C)", type: "number", step: 0.1 },
    { key: "runtime",      label: "Runtime (s)",       type: "number", step: 1 },
  ]},
  { section: "Slave status", rows: [
    { key: "slave.status.flame",      label: "Flame",      type: "bool" },
    { key: "slave.status.ch_mode",    label: "CH mode",    type: "bool" },
    { key: "slave.status.ch2_mode",   label: "CH2 mode",   type: "bool" },
    { key: "slave.status.dhw_mode",   label: "DHW mode",   type: "bool" },
    { key: "slave.status.diagnostic", label: "Diagnostic", type: "bool" },
  ]},
  { section: "Slave temps & modulation", rows: [
    { key: "slave.flow_t",   label: "Flow temp (°C)",        type: "number", step: 0.1 },
    { key: "slave.flow_t2",  label: "Flow temp 2 (°C)",      type: "number", step: 0.1 },
    { key: "slave.dhw_t",    label: "DHW temp (°C)",         type: "number", step: 0.1 },
    { key: "slave.return_t", label: "Return temp (°C)",      type: "number", step: 0.1 },
    { key: "slave.rel_mod",  label: "Rel. modulation (%)",   type: "number", step: 1 },
  ]},
  { section: "Heater circuit 1", rows: [
    { key: "heatercircuit.0.roomsetpoint", label: "Room setpoint (°C)", type: "number", step: 0.5 },
    { key: "heatercircuit.0.roomtemp",     label: "Room temp (°C)",     type: "number", step: 0.1 },
    { key: "heatercircuit.0.roomMode",     label: "Room mode",          type: "select", options: ["off","heat","auto"] },
    { key: "heatercircuit.0.ctrlMode",     label: "CH ctrl mode",       type: "select", options: ["off","heat","auto"] },
    { key: "heatercircuit.0.action",       label: "Action",             type: "select", options: ["off","heating","cooling","idle"] },
    { key: "heatercircuit.0.roomAction",   label: "Room action",        type: "select", options: ["off","heating","cooling","idle"] },
    { key: "heatercircuit.0.suspended",    label: "Suspended",          type: "bool" },
  ]},
  { section: "Heater circuit 2", rows: [
    { key: "heatercircuit.1.roomsetpoint", label: "Room setpoint (°C)", type: "number", step: 0.5 },
    { key: "heatercircuit.1.roomtemp",     label: "Room temp (°C)",     type: "number", step: 0.1 },
    { key: "heatercircuit.1.roomMode",     label: "Room mode",          type: "select", options: ["off","heat","auto"] },
    { key: "heatercircuit.1.ctrlMode",     label: "CH ctrl mode",       type: "select", options: ["off","heat","auto"] },
    { key: "heatercircuit.1.action",       label: "Action",             type: "select", options: ["off","heating","cooling","idle"] },
    { key: "heatercircuit.1.roomAction",   label: "Room action",        type: "select", options: ["off","heating","cooling","idle"] },
    { key: "heatercircuit.1.suspended",    label: "Suspended",          type: "bool" },
  ]},
  { section: "Master", rows: [
    { key: "master.room_t",      label: "Room temp 1 (°C)",     type: "number", step: 0.1 },
    { key: "master.room_set_t",  label: "Room setpoint 1 (°C)", type: "number", step: 0.1 },
    { key: "master.room_t2",     label: "Room temp 2 (°C)",     type: "number", step: 0.1 },
    { key: "master.room_set_t2", label: "Room setpoint 2 (°C)", type: "number", step: 0.1 },
    { key: "master.ch_set_t",    label: "CH flow setpoint (°C)",type: "number", step: 0.1 },
    { key: "master.ch_set_t2",   label: "CH2 flow setpoint (°C)",type:"number", step: 0.1 },
  ]},
];

function deepGet(obj, path) {
  return path.split('.').reduce((o, k) => o != null ? (Array.isArray(o) ? o[+k] : o[k]) : undefined, obj);
}

let toastTimer;
function showToast(msg, err) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'show' + (err ? ' err' : '');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.className = '', 2200);
}

async function sendValue(key, value) {
  try {
    const r = await fetch('/admin/state', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({[key]: value})
    });
        if (r.ok) {
            showToast('✓  ' + key + ' = ' + JSON.stringify(value));
            return;
        }

        const body = await r.json().catch(() => null);
        const details = body?.failed ? (' ' + JSON.stringify(body.failed)) : '';
        showToast('Error ' + r.status + details, true);
  } catch(e) { showToast(e.message, true); }
}

function collectLeafPaths(obj, prefix = '') {
    const paths = [];
    if (obj === null || obj === undefined)
        return paths;

    if (Array.isArray(obj)) {
        obj.forEach((entry, idx) => {
            const key = prefix ? `${prefix}.${idx}` : String(idx);
            if (entry !== null && typeof entry === 'object')
                paths.push(...collectLeafPaths(entry, key));
            else
                paths.push(key);
        });
        return paths;
    }

    if (typeof obj === 'object') {
        Object.keys(obj).forEach((k) => {
            const value = obj[k];
            const key = prefix ? `${prefix}.${k}` : k;
            if (value !== null && typeof value === 'object')
                paths.push(...collectLeafPaths(value, key));
            else
                paths.push(key);
        });
        return paths;
    }

    if (prefix)
        paths.push(prefix);
    return paths;
}

function updatePathSuggestions(status) {
    const list = document.getElementById('statusPathList');
    if (!list)
        return;

    list.innerHTML = '';
    collectLeafPaths(status).forEach((path) => {
        list.append(new Option(path, path));
    });
}

function parseCustomValue(rawValue, mode) {
    const raw = String(rawValue ?? '').trim();

    if (mode === 'string')
        return rawValue;

    if (mode === 'bool') {
        const low = raw.toLowerCase();
        if (low === 'true' || low === '1' || low === 'on') return true;
        if (low === 'false' || low === '0' || low === 'off') return false;
        throw new Error('expected true/false');
    }

    if (mode === 'number') {
        const num = Number.parseFloat(raw);
        if (!Number.isFinite(num))
            throw new Error('expected number');
        return num;
    }

    if (mode === 'json')
        return JSON.parse(raw);

    if (raw === '')
        return '';

    const low = raw.toLowerCase();
    if (low === 'true') return true;
    if (low === 'false') return false;
    if (low === 'null') return null;
    if (/^-?\\d+(\\.\\d+)?$/.test(raw)) return Number.parseFloat(raw);

    if (raw.startsWith('{') || raw.startsWith('[')) {
        try {
            return JSON.parse(raw);
        }
        catch (err) {
        }
    }

    return rawValue;
}

async function setCustomPath() {
    const pathEl = document.getElementById('customPath');
    const typeEl = document.getElementById('customType');
    const valueEl = document.getElementById('customValue');
    if (!pathEl || !typeEl || !valueEl)
        return;

    const path = pathEl.value.trim();
    if (!path) {
        showToast('Path is required', true);
        return;
    }

    let value;
    try {
        value = parseCustomValue(valueEl.value, typeEl.value);
    }
    catch (err) {
        showToast('Invalid value: ' + err.message, true);
        return;
    }

    await sendValue(path, value);
}

function applyStatus(status) {
  for (const section of FIELDS) {
    for (const row of section.rows) {
      const id = 'f_' + row.key.replace(/\\./g, '_');
      const el = document.getElementById(id);
      if (!el) continue;
      const val = deepGet(status, row.key);
      if (val === undefined) continue;
      if (row.type === 'bool') el.checked = !!val;
      else el.value = val;
    }
  }
}

function buildUI(status) {
  const grid = document.getElementById('grid');
    updatePathSuggestions(status);
  grid.innerHTML = '';
  for (const section of FIELDS) {
    const card = document.createElement('div');
    card.className = 'card';
    const h3 = document.createElement('h3');
    h3.textContent = section.section;
    card.appendChild(h3);
    for (const row of section.rows) {
      const val = deepGet(status, row.key);
      const id = 'f_' + row.key.replace(/\\./g, '_');
      const div = document.createElement('div');
      div.className = 'row';
      let inputHtml;
      if (row.type === 'bool') {
        inputHtml = `<input type="checkbox" id="${id}" data-key="${row.key}" data-type="${row.type}" ${val ? 'checked' : ''}>`;
      } else if (row.type === 'select') {
        const opts = (row.options || []).map(o => `<option${o === val ? ' selected' : ''}>${o}</option>`).join('');
        inputHtml = `<select id="${id}" data-key="${row.key}" data-type="${row.type}">${opts}</select>`;
      } else {
        inputHtml = `<input type="number" id="${id}" data-key="${row.key}" data-type="${row.type}" value="${val ?? ''}" step="${row.step ?? 1}">`;
      }
      div.innerHTML = `<label for="${id}">${row.label}</label>${inputHtml}`;
      card.appendChild(div);
    }
    grid.appendChild(card);
  }
    if (!grid.dataset.changeBound) {
        grid.addEventListener('change', async (e) => {
            const el = e.target;
            if (!el.dataset.key) return;
            const key = el.dataset.key;
            const type = el.dataset.type;
            let value;
            if (type === 'bool') value = el.checked;
            else if (type === 'number') value = parseFloat(el.value);
            else value = el.value;
            await sendValue(key, value);
        });
        grid.dataset.changeBound = '1';
    }
}

async function reload() {
  const r = await fetch('/status');
    const status = await r.json();
    applyStatus(status);
    updatePathSuggestions(status);
  showToast('Reloaded from /status');
}

async function loadJsonFile(input, target) {
  const file = input.files[0];
  input.value = '';
  if (!file) return;
  let data;
  try { data = JSON.parse(await file.text()); }
  catch (err) { showToast('Invalid JSON: ' + err.message, true); return; }
  const url = target === 'config' ? '/config' : '/admin/status';
  const r = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) });
  if (r.ok) {
    showToast(`Loaded ${target} from ${file.name}`);
    if (target === 'status') { const s = await fetch('/status'); buildUI(await s.json()); }
  } else {
    showToast(`Upload failed (${r.status})`, true);
  }
}

(async () => {
    const customValue = document.getElementById('customValue');
    if (customValue) {
        customValue.addEventListener('keydown', (ev) => {
            if (ev.key === 'Enter')
                setCustomPath();
        });
    }

  const r = await fetch('/status');
  buildUI(await r.json());
})();
</script>
</body>
</html>"""


@app.get("/admin")
def get_admin() -> HTMLResponse:
    return HTMLResponse(_ADMIN_HTML)


@app.post("/admin/status")
async def post_admin_status(request: Request) -> PlainTextResponse:
    state["status"] = await request.json()
    return PlainTextResponse("ok")


@app.post("/admin/state")
async def post_admin_state(request: Request) -> JSONResponse:
    updates = await request.json()
    failed = {}
    for path, value in updates.items():
        try:
            _deep_set(state["status"], path, value)
        except (KeyError, IndexError, TypeError, ValueError) as exc:
            failed[path] = str(exc)
    if failed:
        return JSONResponse({"ok": False, "failed": failed}, status_code=400)
    return JSONResponse({"ok": True, "updated": list(updates.keys())})


# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import threading
    import webbrowser
    import uvicorn

    def _open_browser() -> None:
        import time
        time.sleep(1.2)  # wait for uvicorn to be ready
        webbrowser.open("http://127.0.0.1:8081/")
        webbrowser.open("http://127.0.0.1:8081/admin")

    threading.Thread(target=_open_browser, daemon=True).start()
    uvicorn.run("mock_otthing:app", host="127.0.0.1", port=8081, reload=True)
