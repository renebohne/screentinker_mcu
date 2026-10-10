import os
import shutil

Import("env")

# Ensure C files (like ricmoo/QRCode) compile with -std=gnu11 for GCC 15/16 and C23 compatibility
env.Append(CFLAGS=["-std=gnu11"])

def after_build(source, target, env):
    prog_path = str(target[0])
    dest_path = os.path.join(env.get("PROJECT_DIR"), "screentinker_sim")
    try:
        shutil.copy2(prog_path, dest_path)
        print(f"[Simulator] Created binary: {dest_path}")
    except Exception as e:
        print(f"[Simulator] Warning: Could not copy binary to root: {e}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}", after_build)
