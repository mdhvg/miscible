#!/bin/python3
import pathlib
import hashlib
import zipfile
import urllib.request

# Set cwd as project base dirs
import os
__src_path = pathlib.Path(__file__)
os.chdir(str(__src_path.parent.parent.absolute()))

FILES = [
    {
        "url": "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h",
        "filename": "stb_image.h",
        "parent": "deps/stb",
    },
    {
        "url": "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h",
        "filename": "stb_image_write.h",
        "parent": "deps/stb",
    },
    {
        "url": "https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h",
        "filename": "stb_image_resize2.h",
        "parent": "deps/stb",
    },
    {
        "url": "https://raw.githubusercontent.com/juliettef/IconFontCppHeaders/refs/heads/main/IconsLucide.h",
        "filename": "IconsLucide.h",
        "parent": "deps/icons",
    },
    {
        "url": "https://sqlite.org/2025/sqlite-amalgamation-3490100.zip",
        "filename": "sqlite.zip",
        "parent": "deps/sqlite",
        "hash": "6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3",
    },
    {
        "url": "https://unpkg.com/lucide-static@latest/font/lucide.ttf",
        "filename": "lucide.ttf",
        "parent": "fonts",
    },
    {
        "url": "https://raw.githubusercontent.com/doctest/doctest/1da23a3e8119ec5cce4f9388e91b065e20bf06f5/doctest/doctest.h",
        "filename": "doctest.h",
        "parent": "deps/doctest",
    },
    {
        "url": "https://raw.githubusercontent.com/ogay/sha2/b90991f90967a46d0955dc981e9e3cd53c13b061/sha2.c",
        "filename": "sha2.c",
        "parent": "deps/sha2",
    },
    {
        "url": "https://raw.githubusercontent.com/ogay/sha2/b90991f90967a46d0955dc981e9e3cd53c13b061/sha2.h",
        "filename": "sha2.h",
        "parent": "deps/sha2",
    },
    {
        "url": "https://raw.githubusercontent.com/zserge/jsmn/refs/heads/master/jsmn.h",
        "filename": "jsmn.h",
        "parent": "deps/jsmn"
    }
]


def get_sha256hash(path):
    hasher = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def extract_files_flat(zip_path, target_dir):
    target_dir.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(zip_path, "r") as z:
        for member in z.infolist():
            if member.is_dir():
                continue

            filename = pathlib.Path(member.filename).name
            if not filename:
                continue

            out_path = target_dir / filename

            with z.open(member) as src, open(out_path, "wb") as dst:
                dst.write(src.read())


for file in FILES:
    parent = pathlib.Path(file["parent"])
    file_path = pathlib.Path(file["parent"]) / file["filename"]
    if not pathlib.Path.exists(file_path):
        pathlib.Path.mkdir(parent, parents=True, exist_ok=True)
        print(f"Downloading {str(file_path)=}")

        req = urllib.request.Request(file["url"])
        print(f"Fetching from {file["url"]}")
        with urllib.request.urlopen(req) as response:
            with open(str(file_path), "wb") as f:
                f.write(response.read())
        if "hash" in file:
            calculated_hash = get_sha256hash(str(file_path))
            if calculated_hash != file["hash"]:
                print(f"Hashes don't match for {str(file_path)=}")
                print(f"expected_hash={file['hash']}")
                print(f"{calculated_hash=}")
    if "extract" in file:
        print(f"Extracting {str(file_path)=} to {str(parent)=}")
        extract_files_flat(file_path, parent)
