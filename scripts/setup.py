#!/bin/python3
import pathlib
import hashlib
import zipfile
import urllib.request 
from concurrent.futures import ThreadPoolExecutor


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
        "parent": "deps/",
        "hash": "6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3",
        "extract": "deps/sqlite",
    },
    {
        "url": "https://unpkg.com/lucide-static@latest/font/lucide.ttf",
        "filename": "lucide.ttf",
        "parent": "fonts",
    },
    {
        "url": "https://raw.githubusercontent.com/gouwsxander/easy-args/1b776957e13200a8d0d192dc909c46672baeb065/includes/easyargs.h",
        "filename": "easyargs.h",
        "parent": "deps/easy-args",
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
    }
]


def get_sha256hash(path: str) -> str:

    hasher = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def extract_files_flat(zip_path: pathlib.Path, target_dir: pathlib.Path) -> None:
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


def setup_file(file: dict) -> None:
    pth = pathlib.Path(file["parent"]) / file["filename"]
    file_path = str(pth)
    if not pth.exists():
        pathlib.Path.mkdir(pth.parent, parents=True, exist_ok=True)
        print(f"Downloading {file_path=}")

        req = urllib.request.Request(
            file["url"],
        )
        with urllib.request.urlopen(req) as response:
            with open(file_path, "wb") as f:
                f.write(response.read())
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
        # Cleanup zip file
        print(f"Cleaning up {file_path=}")
        pth.unlink()

def supervisor() -> None:
    with ThreadPoolExecutor() as executor:
        executor.map(setup_file, FILES)
        
if __name__ == "__main__":
    supervisor()
    