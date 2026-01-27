# Miscible

Miscible is a smart (and personal) photo gallery. It aims to provide image gallery features provided by (Web based) applications like Google Photos, Windows Photos, etc, while keeping everything running locally (and smoothly).

<p align="center">
    <img src="docs/Screenshot.png" alt="Miscible Screenshot">
</p>
<p align="center">
    <i>Miscible running on Windows 10</i>
</p>

> [!CAUTION]
> Miscible is still very much under development. Many of the (even basic) features can cause crashes.

## Features

- **Automatic directory scanning**
- **Image thumbnail atlas generation for fast preview load**
- **Automatic image embedding creation**

## Building

> Currently there are only ways to build and no installation method on any OS.

### Requirements

- git
- cmake
- gcc or msvc (on Windows)
- python3 + python3-requests (maybe in a virtual environment)

- Clone the repo

```bash
git clone --recursive https://github.com/mdhvg/miscible
cd miscible

```

- Run the setup script

```bash
python3 ./scripts/setup.py

```

- Run the build script

*Linux*

```bash
bash ./scripts/build.sh

```

*Windows*
```powershell
.\scripts\build.ps1

```

## Usage

Binary is built in the `./build` directory and can be ran from there.

Setup script doesn't automatically download the CLIP model, so it can be manually downloaded from [Hugging Face🤗](https://huggingface.co/laion/CLIP-ViT-B-32-laion2B-s34B-b79K) and placed at `(project root)/CLIP-ViT-B-32-laion2B-s34B-b79K.gguf`.

#### Usage Options

Normal run

```bash
./build/Miscible
```

Skip embeddings

```bash
./build/Miscible --no-embed
```

## Credits

Although there are many things that need refactoring to follow the style (#TODO), the code style and build procedure in this software is heavily inspired by [@Casey Muratori's](https://caseymuratori.com/about) [Handmade Hero series](https://hero.handmade.network) ([YouTube](https://www.youtube.com/watch?v=Ee3EtYb8d1o)).

A lot of core code snippets come from [EpicGamesExt/raddebugger](https://github.com/EpicGamesExt/raddebugger).

### Other sources which were referenced

- `src/base/arena.*` from [Enter The Arena: Simplifying Memory Management (2023)](https://www.youtube.com/watch?v=TZ5a3gCCZYo) by [@Ryan Fleury](https://x.com/rfleury).
- `src/base/arena.*` also from [tsoding/arena](https://github.com/tsoding/arena) by [@tsoding](https://www.twitch.tv/tsoding).
- `src/base/string.*`, `src/base/array.h` from [Vjekoslav Krajačić – File Pilot: Inside the Engine – BSC 2025](https://youtube.com/watch?v=bUOOaXf9qIM) at [@BSC](https://www.youtube.com/@BetterSoftwareConference) 2025 talk by [@Vjekoslav Krajačić](https://x.com/vkrajacic).