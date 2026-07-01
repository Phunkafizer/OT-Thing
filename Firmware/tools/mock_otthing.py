from pathlib import Path
import asyncio
import copy
import hashlib
import os
import secrets
import time

from fastapi import FastAPI, Request, UploadFile, WebSocket
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, PlainTextResponse

app = FastAPI(title="OTthing Mock Server")

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
INDEX_FILE = DATA_DIR / "index.html"
SESSION_COOKIE = "OTSESSID"
SESSION_TTL_SEC = 30 * 60
MOCK_CONFIG_MODE = os.getenv("OTTHING_MOCK_CONFIG_MODE", "0") == "1"

state = {
    "config": {
        "hostname": "otthing-mock",
        "haPrefix": "homeassistant",
        "slaveApp": 0,
        "otMode": 4,
        "enableSlave": False,
        "otDelay": 100,
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
                "roomComp": {
                    "enabled": True,
                    "p": 1.0,
                    "i": 0.5,
                    "boost": 1,
                },
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
                "roomComp": {
                    "enabled": True,
                    "p": 1.0,
                    "i": 0.5,
                    "boost": 1,
                },
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
        "coolingCtrl": 35.0,
        "coolingMode": "off",
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
                "result": True,
                "data": {
                    "value": "1a00",
                    "ch_enable": False,
                    "ch2_enable": True,
                    "dhw_enable": True,
                    "summer_mode": False,
                    "dhw_blocking": False,
                    "cooling_enable": False,
                    "otc_active": True,
                },
            },
            "vent_status": {
                "result": True,
                "data": {
                    "value": "100",
                    "vent_enable": True,
                    "open_bypass": False,
                    "auto_bypass": False,
                    "free_vent_enable": False,
                },
            },
            "ch_set_t": {"result": True, "data": 44.0},
            "ch_set_t2": {"result": True, "data": 22.3},
            "dhw_set_t": {"result": True, "data": 49.0},
            "max_set_t": {"result": True, "data": 65.0},
            "room_t": {"result": True, "data": 20.1},
            "room_set_t": {"result": True, "data": 21.3},
            "room_t2": {"result": True, "data": 21.1},
            "room_set_t2": {"result": True, "data": 22.3},
            "outside_t": {"result": True, "data": 15.0},
            "max_rel_mod": {"result": True, "data": 100},
            "cooling_ctrl": {"result": True, "data": 35.0},
            "rel_vent_set": {"result": True, "data": 50},
            "memberIdOk": True,
            "master_ot_version": {"result": True, "data": "4.2"},
            "master_prod_version": {"result": True, "data": "1.0"},
            "master_config_member": {
                "result": True,
                "data": {
                    "value": "108",
                    "memberId": 8,
                    "smartPowerImplemented": True,
                },
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
                "heat_cool_ctrl": False,
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
                "currentOnTime": 0.0,
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

auth = {
    "salt": "",
    "hash": "",
    "session": "",
    "expiry": 0.0,
}


def auth_configured() -> bool:
    return bool(auth["salt"] and auth["hash"])


def password_hash(salt: str, password: str) -> str:
    return hashlib.sha256(f"{salt}:{password}".encode("utf-8")).hexdigest()


def set_credentials(password: str) -> None:
    auth["salt"] = secrets.token_hex(16)
    auth["hash"] = password_hash(auth["salt"], password)


def clear_credentials() -> None:
    auth["salt"] = ""
    auth["hash"] = ""
    auth["session"] = ""
    auth["expiry"] = 0.0


def verify_credentials(password: str) -> bool:
    if not auth_configured():
        return True
    return password_hash(auth["salt"], password) == auth["hash"]


def session_valid(request: Request) -> bool:
    if MOCK_CONFIG_MODE:
        return True
    if not auth_configured():
        return True

    token = request.cookies.get(SESSION_COOKIE, "")
    if not token or token != auth["session"]:
        return False
    if time.time() >= auth["expiry"]:
        return False

    auth["expiry"] = time.time() + SESSION_TTL_SEC
    return True


def unauthorized_response() -> JSONResponse:
    return JSONResponse({"detail": "unauthorized"}, status_code=401)


def require_auth(request: Request) -> JSONResponse | None:
    if session_valid(request):
        return None
    return unauthorized_response()


def auth_cookie_value() -> str:
    auth["session"] = secrets.token_hex(16)
    auth["expiry"] = time.time() + SESSION_TTL_SEC
    return auth["session"]


def set_session_cookie(response, token: str) -> None:
    response.set_cookie(
        key=SESSION_COOKIE,
        value=token,
        max_age=SESSION_TTL_SEC,
        path="/",
        samesite="strict",
    )


def clear_session_cookie(response) -> None:
    auth["session"] = ""
    auth["expiry"] = 0.0
    response.delete_cookie(key=SESSION_COOKIE, path="/")


def ensure_heatercircuit(idx: int) -> None:
    while len(state["status"]["heatercircuit"]) <= idx:
        state["status"]["heatercircuit"].append({})


@app.get("/")
def get_index() -> FileResponse:
    return FileResponse(INDEX_FILE)


@app.get("/status")
def get_status(request: Request) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

    state["status"]["runtime"] += 1
    return JSONResponse(state["status"])


@app.get("/config")
def get_config(request: Request) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

    return JSONResponse(copy.deepcopy(state["config"]))


@app.post("/config")
async def post_config(request: Request) -> PlainTextResponse:
    denied = require_auth(request)
    if denied:
        return PlainTextResponse("unauthorized", status_code=401)

    cfg = await request.json()
    if not isinstance(cfg, dict):
        return PlainTextResponse("bad request", status_code=400)

    state["config"] = cfg
    return PlainTextResponse("ok")


@app.get("/auth/state")
def get_auth_state(request: Request) -> JSONResponse:
    configured = auth_configured()
    logged_in = True if (MOCK_CONFIG_MODE or not configured) else session_valid(request)
    body = {
        "configured": configured,
        "loggedIn": logged_in,
        "bypass": MOCK_CONFIG_MODE,
    }
    return JSONResponse(body)


@app.post("/auth/login")
async def post_auth_login(request: Request) -> PlainTextResponse:
    if MOCK_CONFIG_MODE:
        return PlainTextResponse("ok")

    if not auth_configured():
        resp = PlainTextResponse("ok")
        set_session_cookie(resp, auth_cookie_value())
        return resp

    form = await request.form()
    password = str(form.get("password", ""))
    if not verify_credentials(password):
        return PlainTextResponse("unauthorized", status_code=401)

    resp = PlainTextResponse("ok")
    set_session_cookie(resp, auth_cookie_value())
    return resp


@app.post("/auth/logout")
def post_auth_logout() -> PlainTextResponse:
    resp = PlainTextResponse("ok")
    clear_session_cookie(resp)
    return resp


@app.post("/auth/setup")
async def post_auth_setup(request: Request) -> PlainTextResponse:
    if auth_configured() and not MOCK_CONFIG_MODE:
        denied = require_auth(request)
        if denied:
            return PlainTextResponse("unauthorized", status_code=401)

    form = await request.form()
    password = str(form.get("password", ""))

    if not password:
        clear_credentials()
        resp = PlainTextResponse("ok")
        clear_session_cookie(resp)
        return resp

    set_credentials(password)
    resp = PlainTextResponse("ok")
    if not MOCK_CONFIG_MODE:
        set_session_cookie(resp, auth_cookie_value())
    return resp


@app.post("/setwifi")
async def post_setwifi(request: Request) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

    form = await request.form()
    ssid = str(form.get("ssid", ""))
    if ssid:
        state["status"]["wifi"]["sta_ssid"] = ssid
    return JSONResponse({"ok": True})


@app.get("/scan")
def get_scan(request: Request) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

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
    request: Request,
    roomSetTemp: float | None = None,
    roomSetpoint1: float | None = None,
    roomSetpoint2: float | None = None,
    roomMode1: str | None = None,
    roomMode2: str | None = None,
    chSetTemp1: float | None = None,
    chSetTemp2: float | None = None,
    chMode1: str | None = None,
    chMode2: str | None = None,
    coolingCtrl: float | None = None,
    coolingMode: str | None = None,
) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

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
        state["status"]["master"]["ch_set_t"]["data"] = chSetTemp1

    effective_ch2_mode = chMode2 or (state["status"]["heatercircuit"][1].get("ctrlMode") if len(state["status"]["heatercircuit"]) > 1 else None)
    if chSetTemp2 is not None and effective_ch2_mode in {"heat", "on"}:
        state["status"]["master"]["ch_set_t2"]["data"] = chSetTemp2

    if coolingCtrl is not None:
        ctrl = max(0.0, min(100.0, coolingCtrl))
        state["status"]["coolingCtrl"] = ctrl
        state["status"]["master"]["cooling_ctrl"]["data"] = ctrl

    if coolingMode is not None:
        mode = str(coolingMode).lower()
        if mode in {"off", "cool"}:
            state["status"]["coolingMode"] = mode
            state["status"]["slave"]["status"]["cooling"] = (mode == "cool")
            state["status"]["master"]["status"]["data"]["cooling_enable"] = (mode == "cool")

    return JSONResponse({"ok": True})


@app.get("/reboot")
def get_reboot(request: Request) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

    return JSONResponse({"ok": True})


@app.get("/otitems")
def get_otitems(request: Request) -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

    return JSONResponse({"items": [{"id": 0, "name": "Mock item"}]})


@app.get("/slaverequest")
def get_slaverequest(request: Request, id: int = 0, rw: int = 0, data: str = "0000") -> JSONResponse:
    denied = require_auth(request)
    if denied:
        return denied

    # data low byte >= 0x80 means accepted in UI
    return JSONResponse({"id": id, "rw": rw, "data": "0080", "request": data})


@app.post("/update")
async def post_update(request: Request, firmware: UploadFile) -> PlainTextResponse:
    denied = require_auth(request)
    if denied:
        return PlainTextResponse("unauthorized", status_code=401)

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
    .grid-section-title { grid-column: 1 / -1; margin-top: 4px; margin-bottom: -4px; color: #9ddfff; font-size: 0.86em; font-weight: 700; letter-spacing: 0.08em; text-transform: uppercase; border-bottom: 1px solid #2f4157; padding-bottom: 4px; }
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
    .quick-set input[type=text], .quick-set input[type=number], .quick-set select { background: #0d1b2a; color: #eee; border: 1px solid #334; border-radius: 4px; padding: 5px 8px; font-family: inherit; font-size: 0.82em; }
    .quick-set input[type=checkbox].value-input { width: 18px; height: 18px; min-width: 18px; accent-color: #4fc3f7; }
    .quick-set .path-input { flex: 2; min-width: 260px; }
    .quick-set .value-input { flex: 1; min-width: 180px; }
    .quick-set .quick-btn { background: #1a2a3a; color: #4fc3f7; border: 1px solid #334; padding: 5px 14px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.82em; }
    .quick-set .quick-btn:hover { background: #0d1b2a; color: #fff; }
    .quick-set .hint { width: 100%; font-size: 0.78em; color: #889; }
    .json-panes { display: grid; grid-template-columns: 1fr; gap: 12px; margin-bottom: 14px; }
    .json-pane { border: 1px solid #334; border-radius: 8px; background: #1a1a2a; overflow: hidden; }
    .json-pane-head { display: flex; align-items: center; justify-content: space-between; gap: 8px; padding: 8px 10px; border-bottom: 1px solid #2a3a4a; }
    .json-pane-head span { color: #4fc3f7; font-size: 0.76em; letter-spacing: 1px; text-transform: uppercase; }
    .json-controls { display: flex; gap: 6px; }
    .json-controls button { background: #1a2a3a; color: #9ddfff; border: 1px solid #334; padding: 2px 7px; border-radius: 4px; cursor: pointer; font-family: inherit; font-size: 0.73em; }
    .json-controls button:hover { background: #0d1b2a; color: #fff; }
    .json-tree { max-height: 260px; overflow: auto; padding: 8px; font-size: 0.82em; line-height: 1.35; }
    .json-node { margin: 1px 0; }
    .json-row { display: flex; align-items: center; gap: 4px; padding: 1px 2px; border-radius: 4px; }
    .json-row:hover { background: #20253a; }
    .json-toggle { width: 16px; min-width: 16px; height: 16px; border: none; background: transparent; color: #89d7ff; cursor: pointer; padding: 0; font-family: inherit; font-size: 0.95em; }
    .json-toggle.leaf { visibility: hidden; cursor: default; }
    .json-key { color: #f7c66b; }
    .json-type { color: #9ba0bc; }
    .json-value.string { color: #8be9b3; }
    .json-value.number { color: #9fc5ff; }
    .json-value.boolean { color: #ffd685; }
    .json-value.null { color: #a0a7c2; font-style: italic; }
    .json-value.object { color: #ddd; }
    .json-children { margin-left: 14px; padding-left: 6px; border-left: 1px dotted #334; }
    @media (min-width: 980px) { .json-panes { grid-template-columns: 1fr 1fr; } }
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
<div class="json-panes">
    <div class="json-pane" id="statusJsonPane">
        <div class="json-pane-head">
            <span>Status JSON</span>
            <div class="json-controls">
                <button type="button" data-json-expand="status">Expand all</button>
                <button type="button" data-json-collapse="status">Collapse all</button>
            </div>
        </div>
        <div class="json-tree" id="statusJsonTree"></div>
    </div>
    <div class="json-pane" id="configJsonPane">
        <div class="json-pane-head">
            <span>Config JSON</span>
            <div class="json-controls">
                <button type="button" data-json-expand="config">Expand all</button>
                <button type="button" data-json-collapse="config">Collapse all</button>
            </div>
        </div>
        <div class="json-tree" id="configJsonTree"></div>
    </div>
</div>
<div class="grid" id="grid"></div>
<div id="toast"></div>
<script>
const FIELDS = [
    { section: "General", rows: [
        { key: "outsideTemp",  label: "Outside temp (°C)", type: "number", step: 0.1 },
        { key: "runtime",      label: "Runtime (s)",       type: "number", step: 1 },
        { key: "coolingCtrl",  label: "Cooling ctrl (%)",  type: "number", step: 1 },
        { key: "coolingMode",  label: "Cooling mode",      type: "select", options: ["off","cool"] },
        { key: "USB_connected", label: "USB connected",    type: "bool" },
        { key: "bypass",        label: "Bypass",           type: "bool" },
        { key: "summerMode",    label: "Summer mode",      type: "bool" },
        { key: "dhwBlocking",   label: "DHW blocking",     type: "bool" },
    ]},
    { section: "DHW", rows: [
        { key: "dhw.ctrlMode", label: "DHW ctrl mode", type: "select", options: ["off","heat","auto"] },
        { key: "dhw.action",   label: "DHW action",    type: "select", options: ["off","heating","cooling","idle"] },
    ]},
    { section: "OT Master status", rows: [
        { key: "master.status.data.ch_enable",      label: "CH enable",      type: "bool" },
        { key: "master.status.data.ch2_enable",     label: "CH2 enable",     type: "bool" },
        { key: "master.status.data.dhw_enable",     label: "DHW enable",     type: "bool" },
        { key: "master.status.data.cooling_enable", label: "Cooling enable", type: "bool" },
        { key: "master.status.data.otc_active",     label: "OTC active",     type: "bool" },
        { key: "master.status.data.summer_mode",    label: "Summer mode",    type: "bool" },
        { key: "master.status.data.dhw_blocking",   label: "DHW blocking",   type: "bool" },
    ]},
    { section: "OT Master vent status", rows: [
        { key: "master.vent_status.data.vent_enable",      label: "Vent enable",      type: "bool" },
        { key: "master.vent_status.data.open_bypass",      label: "Open bypass",      type: "bool" },
        { key: "master.vent_status.data.auto_bypass",      label: "Auto bypass",      type: "bool" },
        { key: "master.vent_status.data.free_vent_enable", label: "Free vent enable", type: "bool" },
    ]},
    { section: "OT Master", rows: [
        { key: "master.room_t.data",       label: "Room temp 1 (°C)",       type: "number", step: 0.1 },
        { key: "master.room_set_t.data",   label: "Room setpoint 1 (°C)",   type: "number", step: 0.1 },
        { key: "master.room_t2.data",      label: "Room temp 2 (°C)",       type: "number", step: 0.1 },
        { key: "master.room_set_t2.data",  label: "Room setpoint 2 (°C)",   type: "number", step: 0.1 },
        { key: "master.dhw_set_t.data",    label: "DHW setpoint (°C)",      type: "number", step: 0.1 },
        { key: "master.ch_set_t.data",     label: "CH flow setpoint (°C)",  type: "number", step: 0.1 },
        { key: "master.ch_set_t2.data",    label: "CH2 flow setpoint (°C)", type: "number", step: 0.1 },
        { key: "master.cooling_ctrl.data", label: "Cooling ctrl (%)",       type: "number", step: 1 },
    ]},
    { section: "OT Slave status", rows: [
        { key: "slave.status.fault",      label: "Fault",      type: "bool" },
        { key: "slave.status.flame",      label: "Flame",      type: "bool" },
        { key: "slave.status.diagnostic", label: "Diagnostic", type: "bool" },
        { key: "slave.status.ch_mode",    label: "CH mode",    type: "bool" },
        { key: "slave.status.ch2_mode",   label: "CH2 mode",   type: "bool" },
        { key: "slave.status.dhw_mode",   label: "DHW mode",   type: "bool" },
        { key: "slave.status.cooling",    label: "Cooling",    type: "bool" },
    ]},
    { section: "OT Slave vent status", rows: [
        { key: "slave.vent_status.fault",       label: "Vent fault",      type: "bool" },
        { key: "slave.vent_status.vent_active", label: "Vent active",     type: "bool" },
        { key: "slave.vent_status.bypass_open", label: "Bypass open",     type: "bool" },
        { key: "slave.vent_status.bypass_auto", label: "Bypass auto",     type: "bool" },
        { key: "slave.vent_status.free_vent",   label: "Free vent",       type: "bool" },
        { key: "slave.vent_status.diagnostic",  label: "Vent diagnostic", type: "bool" },
    ]},
    { section: "OT Slave config", rows: [
        { key: "slave.slave_config_member.ch2_present",            label: "CH2 present",              type: "bool" },
        { key: "slave.slave_config_member.dhw_present",            label: "DHW present",              type: "bool" },
        { key: "slave.slave_config_member.cooling_config",         label: "Cooling config",           type: "bool" },
        { key: "slave.slave_config_member.heat_cool_ctrl",         label: "Heat/cool master ctrl",    type: "bool" },
        { key: "slave.slave_config_member.dhw_config",             label: "DHW config",               type: "bool" },
        { key: "slave.slave_config_member.master_lowoff_pumpctrl", label: "Master low/off pump ctrl", type: "bool" },
        { key: "slave.slave_config_member.memberId",               label: "Member ID",                type: "number", step: 1 },
        { key: "slave.slave_config_member.ctrl_type",              label: "Ctrl type",                type: "bool" },
    ]},
    { section: "OT slave sensors", rows: [
        { key: "slave.flow_t",      label: "Flow temp (°C)",      type: "number", step: 0.1 },
        { key: "slave.flow_t2",     label: "Flow temp 2 (°C)",    type: "number", step: 0.1 },
        { key: "slave.dhw_t",       label: "DHW temp (°C)",       type: "number", step: 0.1 },
        { key: "slave.return_t",    label: "Return temp (°C)",    type: "number", step: 0.1 },
        { key: "slave.exhaust_t",   label: "Exhaust temp (°C)",   type: "number", step: 0.1 },
        { key: "slave.ch_pressure", label: "Pressure (bar)",      type: "number", step: 0.1 },
        { key: "slave.rel_mod",     label: "Rel. modulation (%)", type: "number", step: 1 },
    ]},
    { section: "Heater circuit 1", rows: [
        { key: "heatercircuit.0.roomsetpoint", label: "Room setpoint (°C)", type: "number", step: 0.5 },
        { key: "heatercircuit.0.roomtemp",     label: "Room temp (°C)",     type: "number", step: 0.1 },
        { key: "heatercircuit.0.returnTemp",   label: "Return temp (°C)",   type: "number", step: 0.1 },
        { key: "heatercircuit.0.roomcompInteg", label: "RoomComp integ",     type: "number", step: 0.1 },
        { key: "heatercircuit.0.retLimitInteg", label: "ReturnLimit integ",  type: "number", step: 0.1 },
        { key: "heatercircuit.0.roomMode",     label: "Room mode",          type: "select", options: ["off","heat","auto"] },
        { key: "heatercircuit.0.ctrlMode",     label: "CH ctrl mode",       type: "select", options: ["off","heat","auto"] },
        { key: "heatercircuit.0.action",       label: "Action",             type: "select", options: ["off","heating","cooling","idle"] },
        { key: "heatercircuit.0.roomAction",   label: "Room action",        type: "select", options: ["off","heating","cooling","idle"] },
        { key: "heatercircuit.0.suspended",    label: "Suspended",          type: "bool" },
    ]},
    { section: "Heater circuit 2", rows: [
        { key: "heatercircuit.1.roomsetpoint", label: "Room setpoint (°C)", type: "number", step: 0.5 },
        { key: "heatercircuit.1.roomtemp",     label: "Room temp (°C)",     type: "number", step: 0.1 },
        { key: "heatercircuit.1.returnTemp",   label: "Return temp (°C)",   type: "number", step: 0.1 },
        { key: "heatercircuit.1.roomcompInteg", label: "RoomComp integ",     type: "number", step: 0.1 },
        { key: "heatercircuit.1.retLimitInteg", label: "ReturnLimit integ",  type: "number", step: 0.1 },
        { key: "heatercircuit.1.roomMode",     label: "Room mode",          type: "select", options: ["off","heat","auto"] },
        { key: "heatercircuit.1.ctrlMode",     label: "CH ctrl mode",       type: "select", options: ["off","heat","auto"] },
        { key: "heatercircuit.1.action",       label: "Action",             type: "select", options: ["off","heating","cooling","idle"] },
        { key: "heatercircuit.1.roomAction",   label: "Room action",        type: "select", options: ["off","heating","cooling","idle"] },
        { key: "heatercircuit.1.suspended",    label: "Suspended",          type: "bool" },
    ]},
];

function deepGet(obj, path) {
  return path.split('.').reduce((o, k) => o != null ? (Array.isArray(o) ? o[+k] : o[k]) : undefined, obj);
}

function deepSet(obj, path, value) {
    if (!obj || !path) return;
    const keys = path.split('.');
    let cur = obj;
    for (let i = 0; i < keys.length - 1; i++) {
        const k = keys[i];
        cur = Array.isArray(cur) ? cur[+k] : cur[k];
        if (cur == null) return;
    }
    const last = keys[keys.length - 1];
    if (Array.isArray(cur)) cur[+last] = value;
    else cur[last] = value;
}

function valueToInputString(value) {
    if (value === undefined || value === null) return '';
    if (typeof value === 'object') return JSON.stringify(value);
    return String(value);
}

function inferCustomType(value) {
    if (typeof value === 'boolean') return 'bool';
    if (typeof value === 'number' && Number.isFinite(value)) return 'number';
    if (typeof value === 'string') return 'string';
    return 'json';
}

function setCustomValueWidget(type, value) {
    const valueEl = document.getElementById('customValue');
    if (!valueEl) return;

    if (type === 'bool') {
        valueEl.type = 'checkbox';
        valueEl.checked = !!value;
        valueEl.title = valueEl.checked ? 'true' : 'false';
        return;
    }

    if (type === 'number') {
        valueEl.type = 'number';
        valueEl.step = 'any';
        valueEl.value = (typeof value === 'number' && Number.isFinite(value)) ? String(value) : '';
        valueEl.title = '';
        return;
    }

    valueEl.type = 'text';
    valueEl.value = valueToInputString(value);
    valueEl.title = '';
}

let latestStatus = null;
let latestConfig = null;
const jsonExpanded = {
    status: new Set(['$']),
    config: new Set(['$']),
};

function encodeJsonPathSegment(seg) {
    return String(seg).replaceAll('~', '~0').replaceAll('/', '~1');
}

function toJsonPath(base, seg) {
    return base + '/' + encodeJsonPathSegment(seg);
}

function jsonPathToDotPath(path) {
    if (!path || path === '$')
        return '';
    const parts = String(path).split('/').slice(1).map((seg) =>
        seg.replaceAll('~1', '/').replaceAll('~0', '~')
    );
    return parts.join('.');
}

function collectJsonPaths(value, currentPath, output) {
    if (value === null || value === undefined || typeof value !== 'object')
        return;

    output.add(currentPath);
    const entries = Array.isArray(value)
        ? value.map((entry, idx) => [idx, entry])
        : Object.entries(value);
    entries.forEach(([key, child]) => {
        collectJsonPaths(child, toJsonPath(currentPath, key), output);
    });
}

function getJsonValueClass(value) {
    if (value === null)
        return 'null';
    return typeof value;
}

function formatJsonPrimitive(value) {
    if (typeof value === 'string')
        return '"' + value + '"';
    if (value === null)
        return 'null';
    return String(value);
}

function buildJsonTreeNode(key, value, path, expandedSet) {
    const node = document.createElement('div');
    node.className = 'json-node';

    const row = document.createElement('div');
    row.className = 'json-row';
    row.dataset.path = path;

    const isObject = value !== null && typeof value === 'object';
    const size = isObject ? Object.keys(value).length : 0;
    const hasChildren = isObject && size > 0;
    const expanded = hasChildren && expandedSet.has(path);

    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'json-toggle' + (hasChildren ? '' : ' leaf');
    toggle.textContent = hasChildren ? (expanded ? '▾' : '▸') : '';
    if (hasChildren)
        toggle.dataset.path = path;
    row.appendChild(toggle);

    if (key !== null) {
        const keyEl = document.createElement('span');
        keyEl.className = 'json-key';
        keyEl.textContent = String(key) + ': ';
        row.appendChild(keyEl);
    }

    if (!isObject) {
        const valueEl = document.createElement('span');
        valueEl.className = 'json-value ' + getJsonValueClass(value);
        valueEl.textContent = formatJsonPrimitive(value);
        row.appendChild(valueEl);
        node.appendChild(row);
        return node;
    }

    const typeEl = document.createElement('span');
    typeEl.className = 'json-type';
    if (size === 0)
        typeEl.textContent = Array.isArray(value) ? '[]' : '{}';
    else if (Array.isArray(value))
        typeEl.textContent = '[' + size + ' items]';
    else
        typeEl.textContent = '{' + size + ' keys}';
    row.appendChild(typeEl);
    node.appendChild(row);

    if (expanded && hasChildren) {
        const children = document.createElement('div');
        children.className = 'json-children';
        const entries = Array.isArray(value)
            ? value.map((entry, idx) => [idx, entry])
            : Object.entries(value);
        entries.forEach(([childKey, childValue]) => {
            children.appendChild(buildJsonTreeNode(childKey, childValue, toJsonPath(path, childKey), expandedSet));
        });
        node.appendChild(children);
    }

    return node;
}

function renderJsonTree(target, rootValue) {
    const tree = document.getElementById(target + 'JsonTree');
    if (!tree)
        return;
    tree.innerHTML = '';
    tree.appendChild(buildJsonTreeNode(null, rootValue, '$', jsonExpanded[target]));
}

function renderJsonBoxes() {
    renderJsonTree('status', latestStatus || {});
    renderJsonTree('config', latestConfig || {});
}

function initJsonInspectorControls() {
    ['status', 'config'].forEach((target) => {
        const tree = document.getElementById(target + 'JsonTree');
        const pane = document.getElementById(target + 'JsonPane');
        if (tree) {
            tree.addEventListener('click', (ev) => {
                const btn = ev.target.closest('button[data-path]');
                if (btn) {
                    const path = btn.dataset.path;
                    if (!path)
                        return;
                    if (jsonExpanded[target].has(path))
                        jsonExpanded[target].delete(path);
                    else
                        jsonExpanded[target].add(path);
                    renderJsonTree(target, target === 'status' ? (latestStatus || {}) : (latestConfig || {}));
                    return;
                }

                if (target !== 'status')
                    return;

                const row = ev.target.closest('.json-row[data-path]');
                const dotPath = jsonPathToDotPath(row?.dataset?.path || '');
                if (!dotPath)
                    return;

                const pathEl = document.getElementById('customPath');
                if (!pathEl)
                    return;

                pathEl.value = dotPath;
                syncCustomValueFromPath();

                const valueEl = document.getElementById('customValue');
                if (!valueEl)
                    return;

                valueEl.focus();
                if (valueEl.type !== 'checkbox' && typeof valueEl.select === 'function')
                    valueEl.select();
            });
        }
        if (pane) {
            const expand = pane.querySelector('button[data-json-expand="' + target + '"]');
            const collapse = pane.querySelector('button[data-json-collapse="' + target + '"]');
            if (expand) {
                expand.addEventListener('click', () => {
                    const next = new Set();
                    collectJsonPaths(target === 'status' ? (latestStatus || {}) : (latestConfig || {}), '$', next);
                    jsonExpanded[target] = next;
                    renderJsonTree(target, target === 'status' ? (latestStatus || {}) : (latestConfig || {}));
                });
            }
            if (collapse) {
                collapse.addEventListener('click', () => {
                    jsonExpanded[target] = new Set(['$']);
                    renderJsonTree(target, target === 'status' ? (latestStatus || {}) : (latestConfig || {}));
                });
            }
        }
    });

    renderJsonBoxes();
}

function syncCustomValueFromPath() {
    const pathEl = document.getElementById('customPath');
    const typeEl = document.getElementById('customType');
    const valueEl = document.getElementById('customValue');
    if (!pathEl || !valueEl || !latestStatus) return;

    const path = pathEl.value.trim();
    if (!path) return;

    const val = deepGet(latestStatus, path);
    if (val === undefined) return;

    const detectedType = inferCustomType(val);
    if (typeEl)
        typeEl.value = detectedType;
    setCustomValueWidget(detectedType, val);
}

function syncCustomValueInputFromType() {
    const pathEl = document.getElementById('customPath');
    const typeEl = document.getElementById('customType');
    const valueEl = document.getElementById('customValue');
    if (!typeEl || !valueEl) return;

    const path = pathEl ? pathEl.value.trim() : '';
    const pathValue = (latestStatus && path) ? deepGet(latestStatus, path) : undefined;
    if (pathValue !== undefined) {
        const t = typeEl.value === 'auto' ? inferCustomType(pathValue) : typeEl.value;
        setCustomValueWidget(t, pathValue);
        return;
    }

    if (typeEl.value === 'bool') {
        valueEl.type = 'checkbox';
        valueEl.checked = false;
        valueEl.title = 'false';
        return;
    }
    if (typeEl.value === 'number') {
        valueEl.type = 'number';
        valueEl.step = 'any';
        valueEl.value = '';
        valueEl.title = '';
        return;
    }
    valueEl.type = 'text';
    valueEl.title = '';
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
            deepSet(latestStatus, key, value);
                        applyStatus(latestStatus);
                        updatePathSuggestions(latestStatus);
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
        let effectiveType = typeEl.value;
        if (effectiveType === 'auto' && latestStatus) {
            const existing = deepGet(latestStatus, path);
            if (existing !== undefined)
                effectiveType = inferCustomType(existing);
        }

        if (effectiveType === 'bool' && valueEl.type === 'checkbox') {
            value = valueEl.checked;
        } else {
            value = parseCustomValue(valueEl.value, effectiveType);
        }
    }
    catch (err) {
        showToast('Invalid value: ' + err.message, true);
        return;
    }

    await sendValue(path, value);
}

function applyStatus(status) {
        latestStatus = status;
    renderJsonBoxes();
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
        syncCustomValueFromPath();
}

function buildUI(status) {
  const grid = document.getElementById('grid');
        latestStatus = status;
    renderJsonBoxes();
    updatePathSuggestions(status);
  grid.innerHTML = '';

    const sectionKind = (sectionName) => {
        const lower = String(sectionName || '').toLowerCase();
        if (lower.includes('master')) return 'master';
        if (lower.includes('slave')) return 'slave';
        return 'other';
    };

    const groups = [
        { title: 'Other', kind: 'other' },
        { title: 'Master', kind: 'master' },
        { title: 'Slave', kind: 'slave' },
    ];

    const renderSectionCard = (section) => {
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
    };

    for (const group of groups) {
        const groupedSections = FIELDS.filter((section) => sectionKind(section.section) === group.kind);
        if (groupedSections.length === 0)
            continue;

        const title = document.createElement('div');
        title.className = 'grid-section-title';
        title.textContent = group.title;
        grid.appendChild(title);

        groupedSections.forEach(renderSectionCard);
    }

        syncCustomValueFromPath();
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
        if (target === 'status') {
            const s = await fetch('/status');
            buildUI(await s.json());
        } else {
            const c = await fetch('/config');
            latestConfig = await c.json();
            renderJsonBoxes();
        }
  } else {
    showToast(`Upload failed (${r.status})`, true);
  }
}

(async () => {
    const customValue = document.getElementById('customValue');
    const customPath = document.getElementById('customPath');
    const customType = document.getElementById('customType');
    if (customValue) {
        customValue.addEventListener('keydown', (ev) => {
            if (ev.key === 'Enter')
                setCustomPath();
        });
        customValue.addEventListener('change', () => {
            if (customValue.type === 'checkbox')
                customValue.title = customValue.checked ? 'true' : 'false';
        });
    }
    if (customPath) {
        customPath.addEventListener('change', syncCustomValueFromPath);
        customPath.addEventListener('input', syncCustomValueFromPath);
    }
    if (customType)
        customType.addEventListener('change', syncCustomValueInputFromType);

        initJsonInspectorControls();

    const [statusResp, configResp] = await Promise.all([
            fetch('/status'),
            fetch('/config'),
    ]);
    latestConfig = await configResp.json();
    buildUI(await statusResp.json());
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
    uvicorn.run(
        "mock_otthing:app",
        host="127.0.0.1",
        port=8081,
        reload=True,
        reload_dirs=["."]
    )
