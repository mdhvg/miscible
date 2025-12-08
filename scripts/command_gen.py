#!/bin/python3
import pathlib
import json
import glob
import os

# Set cwd as project base dir
__src_path = pathlib.Path(__file__)
os.chdir(str(__src_path.parent.parent.absolute()))

BUILD_DIR = "build"
COMPILE_COMMANDS_FILE = os.path.join(BUILD_DIR, "compile_commands.json")

common_flags = [
    "/std:c++17",
    "-Od",
    "-nologo",
    "-FC",
    "-WX",
    "-W4",
    "-wd4201",
    "-wd4100",
    "-wd4505",
    "-wd4189",
    "-wd4457",
    "-wd4456",
    "-wd4819",
    "-wd5287",
    "-wd4244",
    "-MTd",
    "-Oi",
    "-GR-",
    "-Gm-",
    "-EHa-",
    "-Zi",
]

root_dir = os.getcwd().replace("\\", "/")

defines = ["-DDEBUG=1", f'-DROOT_DIR="""{root_dir}"""', "-D_CRT_SECURE_NO_WARNINGS=1"]

compiler = "cl.exe"
paths = os.environ.get("PATH", "").split(os.pathsep)
for path in paths:
    potential_path = os.path.join(path, compiler)
    if os.path.isfile(potential_path):
        compiler = potential_path
        break

dirs = ["deps/sqlite", "deps/stb"]

headers = [f"-I{os.path.abspath('src')}"]
sources = [os.path.abspath("src/main.cpp")]

for d in dirs:
    sources.extend(glob.glob(f"{d}/**/*.c", recursive=True) + glob.glob(f"{d}/**/*.cpp", recursive=True))
    headers.append(f"-I{os.path.abspath(d)}")

commands = []
for src in sources:
    cmd = [f"\"{compiler}\"", *common_flags, *defines, *headers, "/c", src]
    commands.append({
        "directory": os.path.join(root_dir, BUILD_DIR),
        "command": " ".join(cmd),
        "file": os.path.abspath(src).replace("\\", "/"),
    })

with open(COMPILE_COMMANDS_FILE, "w") as f:
    json.dump(commands, f, indent=4)
    f.close()

print(f"Generated {len(commands)} commands in {COMPILE_COMMANDS_FILE}")