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
        "resetInfo": "Power on",
        "USB_connected": True,
        "numWifiDisc": 0,
        "outsideTemp": 8.4,
        "owResult": "ok",
        "wifi": {
            "mac": "AA:BB:CC:DD:EE:FF",
            "ipsta": "192.168.1.50",
            "sta_ssid": "MockWiFi",
            "hostname": "otthing-mock",
            "status": "connected",
            "rssi": -54,
        },
        "mqtt": {
            "connected": True,
            "numDisc": 0,
            "basetopic": "otthing/mock",
        },
        "master": {
            "status": {
                "ch_enable": True,
                "ch2_enable": True,
                "dhw_enable": True,
                "otc_active": False,
            },
            "ch_set_t": 44.0,
            "ch_set_t2": 38.0,
            "dhw_set_t": 50.0,
            "max_set_t": 70.0,
            "room_t": 20.2,
            "room_set_t": 20.5,
            "room_t2": 21.5,
            "room_set_t2": 22.0,
            "outside_t": 8.4,
            "max_rel_mod": 100,
            "memberIdOk": True,
            "master_config_member": {
                "memberId": 8,
                "smartPowerImplemented": False,
            },
            "smartPower": False,
            "txCount": 1000,
            "rxCount": 995,
        },
        "slave": {
            "status": {
                "flame": False,
                "diagnostic": False,
                "ch_mode": True,
                "ch2_mode": True,
                "dhw_mode": False,
            },
            "slave_config_member": {
                "ch2_present": True,
                "dhw_present": True,
                "cooling_config": False,
            },
            "flow_t": 41.0,
            "flow_t2": 36.0,
            "dhw_t": 47.0,
            "outside_t": 8.4,
            "return_t": 33.2,
            "rel_mod": 22,
            "txCount": 1000,
            "rxCount": 995,
            "timeouts": 1,
            "flameStats": {
                "duty": 12.5,
                "lastOnTime": 5,
                "onTime": 7,
                "offTime": 22,
                "freq": 1.8,
            },
        },
        "heatercircuit": [
            {
                "roomsetpoint": 20.5,
                "roomMode": "heat",
                "roomtemp": 20.2,
                "suspended": False,
                "roomcompInteg": 0.0,
                "reduction": 0.0,
                "returnTemp": 33.2,
            },
            {
                "roomsetpoint": 22.0,
                "roomMode": "auto",
                "roomtemp": 21.6,
                "suspended": False,
                "roomcompInteg": 0.0,
                "reduction": 0.0,
                "returnTemp": 31.0,
            },
        ],
        "1wire": {
            "28FF001": 21.1,
            "28FF002": 20.7,
        },
        "BLE": {},
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
  button.set-btn { background: #0f9b8e; color: #fff; border: none; padding: 3px 10px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.78em; font-weight: bold; }
  button.set-btn:hover { background: #4fc3f7; color: #000; }
  #toast { position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%); background: #0f9b8e; color: #fff; padding: 7px 22px; border-radius: 20px; font-size: 0.88em; opacity: 0; transition: opacity 0.3s; pointer-events: none; white-space: nowrap; }
  #toast.show { opacity: 1; }
  #toast.err { background: #ef5350; }
  #reload-btn { margin-bottom: 16px; background: #333; color: #aaa; border: 1px solid #444; padding: 5px 14px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.82em; }
  #reload-btn:hover { background: #444; color: #fff; }
</style>
</head>
<body>
<h1>OTThing Mock Control Panel</h1>
<button id="reload-btn" onclick="reload()">↺ Reload values from /status</button>
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
    { key: "heatercircuit.0.suspended",    label: "Suspended",          type: "bool" },
  ]},
  { section: "Heater circuit 2", rows: [
    { key: "heatercircuit.1.roomsetpoint", label: "Room setpoint (°C)", type: "number", step: 0.5 },
    { key: "heatercircuit.1.roomtemp",     label: "Room temp (°C)",     type: "number", step: 0.1 },
    { key: "heatercircuit.1.roomMode",     label: "Room mode",          type: "select", options: ["off","heat","auto"] },
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
    if (r.ok) showToast('✓  ' + key + ' = ' + JSON.stringify(value));
    else showToast('Error ' + r.status, true);
  } catch(e) { showToast(e.message, true); }
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
        inputHtml = `<input type="checkbox" id="${id}" ${val ? 'checked' : ''}>`;
      } else if (row.type === 'select') {
        const opts = (row.options || []).map(o => `<option${o === val ? ' selected' : ''}>${o}</option>`).join('');
        inputHtml = `<select id="${id}">${opts}</select>`;
      } else {
        inputHtml = `<input type="number" id="${id}" value="${val ?? ''}" step="${row.step ?? 1}">`;
      }
      div.innerHTML = `<label for="${id}">${row.label}</label>${inputHtml}<button class="set-btn" data-key="${row.key}" data-type="${row.type}">Set</button>`;
      card.appendChild(div);
    }
    grid.appendChild(card);
  }
  grid.addEventListener('click', async (e) => {
    const btn = e.target.closest('button.set-btn');
    if (!btn) return;
    const key = btn.dataset.key;
    const type = btn.dataset.type;
    const el = document.getElementById('f_' + key.replace(/\\./g, '_'));
    let value;
    if (type === 'bool') value = el.checked;
    else if (type === 'number') value = parseFloat(el.value);
    else value = el.value;
    await sendValue(key, value);
  });
}

async function reload() {
  const r = await fetch('/status');
  applyStatus(await r.json());
  showToast('Reloaded from /status');
}

(async () => {
  const r = await fetch('/status');
  buildUI(await r.json());
})();
</script>
</body>
</html>"""


@app.get("/admin")
def get_admin() -> HTMLResponse:
    return HTMLResponse(_ADMIN_HTML)


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
        webbrowser.open("http://127.0.0.1:8080/")
        webbrowser.open("http://127.0.0.1:8080/admin")

    threading.Thread(target=_open_browser, daemon=True).start()
    uvicorn.run("mock_otthing:app", host="127.0.0.1", port=8080, reload=True)
