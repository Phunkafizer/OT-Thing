import shutil
import gzip
import os
import esptool
import time
import webbrowser
import requests
from serial.tools import list_ports
Import("env")


CONFIG = {
    "slaveApp": 0, # heat/cool
    "otMode": 1, # master
    "enableSlave": False,
    "boiler": {
        "dhwOn": True,
        "dhwTemperature": 50,
        "overrideDhw": False,
        "coolOn": False
    },
    "heating": [
        {
            "flow": 45,
            "roomtempcomp": 0,
            "chOn": True,
            "flowMax": 60,
            "exponent": 1.3,
            "gradient": 1.4,
            "offset": 0,
            "marker": [],
            "roomsetpoint": {
                "source": 0,
                "temp": 21
            },
            "roomtemp": {
                "source": 1
            },
            "overrideFlow": False
        },
        {
            "flow": 30,
            "roomtempcomp": 0,
            "chOn": True,
            "flowMax": 60,
            "exponent": 1.3,
            "gradient": 1.4,
            "offset": 0,
            "marker": [],
            "roomsetpoint": {
                "source": 0,
                "temp": 21
            },
            "roomtemp": {
                "source": 1
            },
            "overrideFlow": False
        }
    ],
    "vent": {
        "ventEnable": False,
        "openBypass": False,
        "autoBypass": False,
        "freeVentEnable": False,
        "setpoint": 3
    },
    "outsideTemp": {
        "source": 0,
        "apikey": "undefined",
        "lat": None,
        "lon": None,
        "interval": None
    },
    "mqtt": {
        "host": "",
        "port": 1883,
        "user": "",
        "pass": "",
        "tls": False
    },
    "masterMemberId": 8
}


platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32") # works for ESP8266 and ESP32

def copy_html(source, target, env):
    with open(os.path.join(env["PROJECT_DATA_DIR"], "index.html"), "r") as fin:
        with open(os.path.join(env["PROJECT_DIR"], "include/html.h"), "w") as fout:
            fout.write('const char html[] PROGMEM = R"html(')
            for line in fin:
                fout.write(line)
            fout.write('\n)html";')
    
def post_build(source, target, env):
    print("Version: " + env.GetProjectOption("custom_version"))
    print("project dir: " + env["PROJECT_DIR"])
    print("build: " + env["BUILD_DIR"])

def before_upload(source, target, env):
    devices = list_ports.comports()
    for d in devices:
        if (d.vid == 12346) and (d.pid == 4097):
            env.Replace(UPLOAD_PORT=d[0])
            break

def after_upload(source, target, env):
    upload_port = env.get("UPLOAD_PORT", None)
    if upload_port == None:
        env.AutodetectUploadPort()
        upload_port = env.get("UPLOAD_PORT", "none")
    print("Upload port", upload_port)
    with esptool.cmds.detect_chip(port=upload_port) as esp:
        mac = esp.read_mac()
        macstr = ":".join(map(lambda x: "%02X" % x, mac))
        print("MAC is ", macstr)
        esp.hard_reset()
        
        print("Bring target to config mode & press enter")
        input("")
        os.system('cmd /c netsh wlan connect name = "OT Thing"')
        time.sleep(2)
        webbrowser.open('http://4.3.2.1')
        print("Press enter to send default config to target")
        input("")
        print(requests.post("http://4.3.2.1/config", json=CONFIG))
        r = requests.get('http://4.3.2.1/config', timeout=3)
        conf = r.json()
        print(conf['otMode'])




env.AddPreAction("$BUILD_DIR/src/portal.cpp.o", copy_html)
env.AddPreAction("upload", before_upload)
env.AddPostAction("buildprog", post_build)
env.AddPostAction("upload", after_upload)