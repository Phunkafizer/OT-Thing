import shutil
import gzip
import os
Import("env")

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


env.AddPreAction("$BUILD_DIR/src/portal.cpp.o", copy_html)
env.AddPostAction("buildprog", post_build)