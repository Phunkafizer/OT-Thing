import gzip
import os
try:
    import minify_html
except ImportError:
    minify_html = None
Import("env")


def get_bool_project_option(name, default=False):
    val = env.GetProjectOption(name, str(default)).strip().lower()
    return val in ("1", "true", "yes", "on")


platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")  # works for ESP8266 and ESP32

def copy_html():
    print("Creating html.h from index.html");
    with open(os.path.join(env["PROJECT_DATA_DIR"], "index.html"), "r", encoding="utf-8") as fin:
        content = fin.read()
        if env["PIOENV"] in ("release", "production") and minify_html is not None:
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
        elif env["PIOENV"] in ("release", "production"):
            print("minify_html not installed, skipping HTML minification")

        compressed = gzip.compress(content.encode("utf-8"), compresslevel=9)
        print(f"embed html: {len(content.encode('utf-8'))} bytes raw, {len(compressed)} bytes gzip")

        with open(os.path.join(env["PROJECT_DIR"], "include/html.h"), "w", encoding="utf-8") as fout:
            fout.write("#pragma once\n")
            fout.write("#include <pgmspace.h>\n\n")
            fout.write(f"const size_t html_gz_len = {len(compressed)};\n")
            fout.write("const uint8_t html_gz[] PROGMEM = {\n")

            for index in range(0, len(compressed), 16):
                chunk = compressed[index:index + 16]
                fout.write("    ")
                fout.write(", ".join(f"0x{byte:02x}" for byte in chunk))
                if index + 16 < len(compressed):
                    fout.write(",")
                fout.write("\n")

            fout.write("};\n")
            
def post_build(source, target, env):
    print("Version: " + env.GetProjectOption("custom_version"))
    print("project dir: " + env["PROJECT_DIR"])
    print("build: " + env["BUILD_DIR"])

def before_upload(source, target, env):
    """Detect OTthing device port if not explicitly set."""
    from serial.tools import list_ports
    
    TARGET_USB_VID = 0x303A
    TARGET_USB_PID = 0x1001
    
    # Check if port is explicitly set via environment
    forced_port = os.environ.get("OTTHING_UPLOAD_PORT")
    if forced_port:
        env.Replace(UPLOAD_PORT=forced_port)
        return

    # Try to detect device by VID/PID
    for d in list_ports.comports():
        if (d.vid == TARGET_USB_VID) and (d.pid == TARGET_USB_PID):
            env.Replace(UPLOAD_PORT=d.device)
            return


copy_html()
env.AddPreAction("upload", before_upload)
env.AddPostAction("buildprog", post_build)