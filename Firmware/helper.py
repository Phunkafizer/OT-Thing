import shutil
import gzip
import os
import esptool 
Import("env")

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

    upload_port = env.get("UPLOAD_PORT", None)
    if upload_port == None:
        env.AutodetectUploadPort()
        upload_port = env.get("UPLOAD_PORT", "none")
    print("Upload port", upload_port)
    with esptool.cmds.detect_chip(port=upload_port) as esp:
        mac = esp.read_mac()
        macstr = ":".join(map(lambda x: "%02X" % x, mac))
        print("MAC is ", macstr)


env.AddPreAction("$BUILD_DIR/src/portal.cpp.o", copy_html)
env.AddPostAction("buildprog", post_build)