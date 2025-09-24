import shutil
import gzip
import os
try:
    import esptool 
except:
    pass
Import("env")

platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32") # works for ESP8266 and ESP32
    
def post_build(source, target, env):
    print("Version: " + env.GetProjectOption("custom_version"))
    print("project dir: " + env["PROJECT_DIR"])
    print("build: " + env["BUILD_DIR"])

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

env.AddPostAction("buildprog", post_build)
env.AddPostAction("upload", after_upload)