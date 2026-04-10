import os
import time
import webbrowser
import requests
import minify_html
from serial.tools import list_ports
Import("env")
import esptool


CONFIG = {
    "slaveApp": 0, # heat/cool
    "otMode": 1, # master
    "enableSlave": False,
    "boiler": {
        "dhwOn": True,
        "dhwTemperature": 50,
        "overrideDhw": False,
        "coolOn": False,
        "maxModulation": 95,
        "otc": False,
        "summerMode": False,
        "dhmBlocking": False
    },
    "heating": [
        {
            "flow": 45,
            "roomtempcomp": 0,
            "chOn": True,
            "flowMax": 60,
            "flowMin": 20,
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
            "overrideFlow": False,
            "roomComp": {
                "enabled": False,
                "p": 1.0,
                "i": 0.5,
                "boost": 3.0
            },
            "enablyHyst": False,
            "hysteresis": 0.5
        },
        {
            "flow": 30,
            "roomtempcomp": 0,
            "chOn": True,
            "flowMax": 60,
            "flowMin": 20,
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
            "overrideFlow": False,
            "roomComp": {
                "enabled": False,
                "p": 1.0,
                "i": 0.5,
                "boost": 3.0
            },
            "enablyHyst": False,
            "hysteresis": 0.5
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
        "apikey": None,
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
    "masterMemberId": 8,
    "timezone": 3600,
    "hostname": "otthing",
    "haPrefix": "homeassistant",
    "aux": [
        {
            "mode": 4 # DQ: 1wire
        },
        {
            "mode": 0 # DI: not used
        }
    ]
}


platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32") # works for ESP8266 and ESP32

def copy_html():
    print("Creating html.h from index.html");
    with open(os.path.join(env["PROJECT_DATA_DIR"], "index.html"), "r", encoding="utf-8") as fin:
        content = fin.read()
        if (env["PIOENV"] == "release"):
            print("minify html");
            content = minify_html.minify(
                content,
                # --- JS / CSS ---
                minify_js=True,               # minify inline <script> content
                minify_css=True,              # minify inline <style> and style= attributes
                minify_doctype=True,         # shorten <!DOCTYPE html> to <!doctype html>
                # --- Attributes ---
                keep_input_type_text_attr=True,              # keep type="text" on <input> (default is removed as redundant)
                allow_noncompliant_unquoted_attribute_values=False,  # allow unquoted attribute values (faster, but non-standard)
                allow_removing_spaces_between_attributes=False,      # remove spaces between attributes (non-standard)
                # --- Tags ---
                keep_closing_tags=False,                # keep optional closing tags e.g. </li>, </td>
                keep_html_and_head_opening_tags=False,  # keep <html> and <head> opening tags (removed when optional)
                # --- Comments ---
                keep_comments=False,          # keep regular HTML comments
                keep_ssi_comments=False,      # keep SSI comments <!--# ... -->
                remove_bangs=False,           # remove <!...> declarations (e.g. <!DOCTYPE>)
                remove_processing_instructions=False,  # remove <?...?> processing instructions
                # --- Entities ---
                allow_optimal_entities=False, # use shortest entity representation (may change semantics in edge cases)
                # --- Template syntax preservation ---
                preserve_brace_template_syntax=False,          # preserve {{ }}, {% %}, {# #} (Jinja, Handlebars, etc.)
                preserve_chevron_percent_template_syntax=False, # preserve <% %> (EJS, ERB, JSP, etc.)
            )
        with open(os.path.join(env["PROJECT_DIR"], "include/html.h"), "w", encoding="utf-8") as fout:
            fout.write('const char html[] PROGMEM = R"html(')
            fout.write(content)
            fout.write('\n)html";')
            
def post_build(source, target, env):
    print("Version: " + env.GetProjectOption("custom_version"))
    print("project dir: " + env["PROJECT_DIR"])
    print("build: " + env["BUILD_DIR"])

def before_upload(source, target, env):
    devices = list_ports.comports()
    for d in devices:
        if (d.vid == 0x303A) and (d.pid == 0x1001):
            env.Replace(UPLOAD_PORT=d[0])
            break

def after_upload(source, target, env):
    upload_port = env.get("UPLOAD_PORT")
    if upload_port == None:
        env.AutodetectUploadPort()
        upload_port = env.get("UPLOAD_PORT")
    print("Upload port", upload_port)
    with esptool.cmds.detect_chip(port=upload_port) as esp:
        mac = esp.read_mac()
        macstr = ":".join(map(lambda x: "%02X" % x, mac))
        print("MAC is ", macstr)
        esp.hard_reset()
        
        print("Bring target to config mode & press enter")
        input("")
        os.system('cmd /c netsh wlan connect name = "OTthing"')
        time.sleep(2)
        webbrowser.open('http://4.3.2.1')
        print("Press enter to send default config to target")
        input("")
        print(requests.post("http://4.3.2.1/config", json=CONFIG))
        time.sleep(1)
        r = requests.get('http://4.3.2.1/config', timeout=3)
        conf = r.json()
        print("OT mode:", conf['otMode'])


copy_html()
env.AddPreAction("upload", before_upload)
env.AddPostAction("buildprog", post_build)
env.AddPostAction("upload", after_upload)