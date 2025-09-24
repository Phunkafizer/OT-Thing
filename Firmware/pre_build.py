import shutil
import gzip
import os
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

env.AddPreAction("$BUILD_DIR/src/portal.cpp.o", copy_html)