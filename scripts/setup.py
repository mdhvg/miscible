#!/bin/python3
import pathlib
import requests
import hashlib
import zipfile

import os

# Set cwd as project base dirs
__src_path = pathlib.Path(__file__)
os.chdir(str(__src_path.parent.parent.absolute()))

FILES = [
    {
        "url": "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h",
        "path": "stb_image.h",
        "parent": "deps/stb"
    },
    {
        "url": "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h",
        "path": "stb_image_write.h",
        "parent": "deps/stb"
    },
    {
        "url": "https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h",
        "path": "stb_image_resize2.h",
        "parent": "deps/stb"
    },
    {
        "url": "https://raw.githubusercontent.com/juliettef/IconFontCppHeaders/refs/heads/main/IconsLucide.h",
        "path": "IconsLucide.h",
        "parent": "deps/icons"
    },
    {
        "url": "https://sqlite.org/2025/sqlite-amalgamation-3490100.zip",
        "path": "sqlite.zip",
        "parent": "deps/",
        "hash": "6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3",
        "extract": "deps/sqlite"
    }
]

def get_sha256hash(path):
    hasher = hashlib.sha256()
    with open(path, 'rb') as file:
        for chunk in iter(lambda: file.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()

def extract_files_flat(zip_path, target_dir):
    target_dir.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(zip_path, 'r') as z:
        for member in z.infolist():
            if member.is_dir():
                continue

            filename = pathlib.Path(member.filename).name
            if not filename:
                continue

            out_path = target_dir / filename

            with z.open(member) as src, open(out_path, 'wb') as dst:
                dst.write(src.read())

for file in FILES:
    pth = pathlib.Path(file["parent"]) / file["path"]
    file_path = str(pth)
    if not pathlib.Path.exists(pth):
        pathlib.Path.mkdir(pth.parent, parents=True, exist_ok=True)
        print(f"Downloading {file_path=}")
        r = requests.get(file["url"])
        with open(file_path, "wb") as f:
            f.write(r.content)
            f.close()
        if "hash" in file:
            calculated_hash = get_sha256hash(file_path)
            if calculated_hash != file["hash"]:
                print(f"Hashes don't match for {file_path=}")
                print(f"expected_hash={file['hash']}")
                print(f"{calculated_hash=}")
    if "extract" in file:
        extract_path = pathlib.Path(file["extract"])
        print(f"Extracting {file_path=} to {str(extract_path)}")
        extract_files_flat(pth, extract_path)