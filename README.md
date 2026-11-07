# fefw-recomp

A static recompilation of Fire Emblem: Fortune's Weave (Nintendo Switch 2) for Windows. The game's AArch64 code is lifted to C++ and
compiled into a native `fefw.exe`, which runs on a host runtime that stands in for the console's OS services and its NVN2 graphics API
(on Vulkan 1.3).

**This repository and its releases contain no game code, no game data and no keys.** You need your own legally dumped copy of the game.
Every step that reads the game happens on your PC, from your dump: the lift, the compile and the shader translation. See [LEGAL.md](LEGAL.md).

Not affiliated with or endorsed by Nintendo or Intelligent Systems.

## Playing

You need Windows 10 or 11 (64-bit), a GPU with Vulkan 1.3, 16 GB of RAM, about 30 GB free disk space, and your own decrypted dump of
the game's update **131072**: a folder with `exefs\` (`main`, `sdk`, `subsdk0`, `subsdk1`, `subsdk2`, `rtld`) and `romfs\`, a dump root with
`base\` and `updates\<version>\`, or a [`.nup`](https://github.com/a55uka/nup) package. Nothing else needs to be installed.

1. Download `fefw-recomp-<version>-win64.zip` from the Releases page and extract it anywhere.
2. Run `fefw-launcher.exe`. On **Setup**, pick your dump folder (or **.nup**) and press **Set up**. It checks your files against the
   supported version, recompiles the game code into `fefw.exe` with the compiler that comes in the zip, and translates the shaders. The
   first run takes a while (most of it is the compile); **Pause** stops it and **Set up** continues later.
3. **Settings** edits `fefw.ini` and `controls.ini`: output resolution, render scale, upscaler, 60/120 fps, window mode, sound, button
   prompts. Every key is described in [runtime/docs/CONFIG.md](runtime/docs/CONFIG.md).
4. **Play**. F1 opens the in-game settings overlay. Saves are in `save\` next to the launcher.

## Building from source

For development. You need:

- Visual Studio 2022 Build Tools or newer, with "Desktop development with C++" (found with vswhere)
- LLVM 23 or newer (`clang-cl`), in `C:\Program Files\LLVM\bin`, or named by `NX_LLVM` (its bin folder) or `NX_TOOLCHAIN` (a folder with `llvm\bin`)
- CMake 3.25+, Ninja, Git, [uv](https://docs.astral.sh/uv/) (it fetches Python 3.13), .NET 10 SDK, PowerShell 7 (or Windows PowerShell)
- for the launcher: Rust (stable) and Node.js 20+
- optional: NASM 2.14+ (faster movie decoding; on PATH, or `NX_TOOLCHAIN\bin`), sccache

### With the launcher

```
git clone <this repository> fefw-recomp
cd fefw-recomp\launcher
npm install
npm run tauri build -- --no-bundle
```

Start `launcher\src-tauri\target\release\fefw-launcher.exe`. Built inside a checkout, it works on that checkout: the Setup page lists
which build tools it found and runs, in order, with the log on screen:

1. the Python environment (`tools\.venv`)
2. the game code from your dump: `game\dump\exefs` (the console's `main` is ZBIC-compressed and is written uncompressed)
3. the recompile: lift and compile the six modules into `work\build\fefw` (the long step)
4. the texture transcoder sources (astc-encoder, bc7enc_rdo)
5. `fefw.exe` in `game-build\fefw\build\release`
6. the shader translator (fetches Ryujinx's shader code, MIT, at a pinned revision, and builds it)
7. the shaders: found in your romfs and translated to SPIR-V in `work\shaders\spirv`
8. `fefw.ini` in the run folder (`work\play` unless you choose another)

### By hand, from a clean checkout

From the repository root, in a normal command prompt (the scripts set up the Visual Studio environment themselves):

```
:: 1. Python environment
uv venv tools\.venv --python 3.13
uv pip install --python tools\.venv -r recomp\requirements.txt

:: 2. your dump's ExeFS into game\dump\exefs (main written uncompressed; the dump itself is only read)
tools\.venv\Scripts\python -I tools\nso\prepare_exefs.py --exefs D:\dumps\fefw\updates\131072\exefs

:: 3. lift and compile the six modules into work\build\fefw (NX_JOBS = compile processes, default 20)
recomp\scripts\build_game.cmd fefw

:: 4. texture transcoder sources (optional, recommended), then fefw.exe
pwsh -File tools\textures\scripts\fetch_third_party.ps1
pwsh -File game-build\scripts\build_fefw.ps1

:: 5. the shader translator: Ryujinx's sources at the pinned revision, then the harness
git clone --filter=blob:none --no-checkout https://git.ryujinx.app/ryubing/ryujinx.git work\refs\ryubing
git -C work\refs\ryubing checkout a82350bb774f70fcbd41c9987bf67a3775409963
pwsh -File tools\shaders\scripts\setup_ryujinx.ps1 -Ryubing work\refs\ryubing
dotnet build tools\shaders\harness\SpirvHarness.csproj -c Release

:: 6. the shaders of your romfs, translated to SPIR-V
tools\.venv\Scripts\python -I tools\shaders\ampere\extract_corpus.py --romfs D:\dumps\fefw\updates\131072\romfs ^
    --main game\dump\exefs\main --exefs game\dump\exefs --out work\shaders\corpus --jobs 8
dotnet tools\shaders\build\bin\Release\net10.0\spirv_harness.dll --batch work\shaders\corpus --out work\shaders\spirv --jobs 8
```

Then make a run folder with a `fefw.ini` and start `fefw.exe` there:

```ini
[log]
path = fefw.log
[fs]
exefs = C:/src/fefw-recomp/game/dump/exefs
romfs = D:/dumps/fefw/updates/131072/romfs
save_dir = save
[nvn]
shader_cache = C:/src/fefw-recomp/work/shaders/spirv
bc7_cache = C:/src/fefw-recomp/work/bc7_cache
[dlss]
upscaler = taau
[fefw]
fps60 = 1
output = 1440p
render_ratio = 1.0
```

The toolchain can live anywhere: `NX_LLVM` (clang-cl's folder) for the lift, `NX_LLVM_BIN`, `NX_CMAKE_BIN`, `NX_NINJA_BIN`, `NX_VCVARS`
or one `NX_TOOLCHAIN` folder for the runtime (see `runtime\scripts\env.cmd`), and `-DNXRT_NASM=<nasm.exe>` for NASM.

Runtime tests: `pwsh -File runtime\scripts\build.ps1 -Test`.

### Making a release zip

The zip holds no game code: the runtime as `fefw_rt.dll`, the launcher, the lifter, and the tools the player's PC needs (Zig, Python,
the shader translator). Its `fefw.exe` is linked on the player's PC by `tools\build\fefw_build.py`.

```
pwsh -File game-build\scripts\build_fefw.ps1 -Target fefw_rt -BuildDir build\rtdll -CMakeArgs "-DFEFW_RT_DLL=ON"
cd launcher && npm install && npm run tauri build -- --no-bundle && cd ..
pwsh -File tools\shaders\scripts\setup_ryujinx.ps1 -Ryubing work\refs\ryubing
pwsh -File game-build\scripts\make-release.ps1 -Version 0.1.1
```

`make-release.ps1` downloads Zig (checked against ziglang.org's sha256), the Python embeddable package and the Python wheels into
`work\release\cache`, publishes the shader translator self-contained, refuses to pack anything game-derived, and writes
`work\release\fefw-recomp-<version>-win64.zip`.

## Layout

| Folder | What |
|---|---|
| `recomp/` | nxrecomp, the AArch64 to C++ lifter (Python), its CMake glue and the per-game config (`recomp/games/fefw`) |
| `runtime/` | the host runtime: kernel and services, NVN2 on Vulkan, audio, input, movies (`runtime/games/fefw`: this game's hooks) |
| `game-build/` | builds `fefw.exe` from the runtime and the lifted libraries |
| `tools/` | `nso/` (dump preparation), `shaders/` (Maxwell/Ampere shader translator), `textures/` (ASTC to BC7), `tiling/` |
| `launcher/` | the Tauri launcher; `launcher/icon/make_icon.py` draws the app icon |

Generated files stay in `game/` and `work/`, which are git-ignored and must never be committed.

## Reporting problems

Open an issue with `fefw.log` from the run where it happened (it is in the run folder), your GPU, driver and Windows version, and the
commit you built. Do not attach game files or anything made from them.

## Credits

This project stands on the work of others. Thank you.

- [nxrecomp](https://github.com/drakolordx7/nxrecomp) by drakolordx7: the recompiler this is built on.
- [pokopia-recomp](https://github.com/drakolordx7/pokopia-recomp) (drakolordx7) and [featureJosh/pokopia-recomp](https://github.com/featureJosh/pokopia-recomp)
  (featureJosh): the runtime, tools and launcher this repository was forked from (GPL-3.0).
- [drakolordx7/switch2-recomp](https://github.com/drakolordx7/switch2-recomp): the launcher design this launcher follows.
- [nxnative-penis6](https://git.eden-emu.dev/penis6/nxnative-penis6) by seedds (MIT): the movie decoder front end, its dav1d copy and
  several hardware facts (notices in `runtime/THIRD_PARTY_NOTICES.md`).
- [Ryujinx / Ryubing](https://git.ryujinx.app/ryubing/ryujinx) (MIT): the shader translator and tiling code; [Mesa](https://gitlab.freedesktop.org/mesa/mesa) NAK (MIT).
- SuperMarioWonderRecomp (MIT); smo-recomp, ScarletNX and ssbu-recomp (GPL-3.0); [yuzu and Eden](https://git.eden-emu.dev/eden-emu/eden)
  (GPL); libnx (ISC); LLVM libc++abi (Apache-2.0 with LLVM exceptions).
- [zbic](https://github.com/kinnay/zbic) by Yannik Marchand (GPLv2; installed by pip at setup, not included here): ZBIC decompression.
- [dav1d](https://code.videolan.org/videolan/dav1d) (BSD-2-Clause), [libvpx](https://chromium.googlesource.com/webm/libvpx) (BSD-3-Clause),
  [Zstandard](https://github.com/facebook/zstd) (BSD), [SDL3](https://github.com/libsdl-org/SDL) (zlib), volk, VulkanMemoryAllocator, Dear ImGui (MIT),
  Vulkan-Headers (Apache-2.0 or MIT).
- [astc-encoder](https://github.com/ARM-software/astc-encoder) (Apache-2.0), [bc7enc_rdo](https://github.com/richgel999/bc7enc_rdo)
  (MIT or public domain), ISPC (BSD-3-Clause).
- AMD FidelityFX SDK headers and FSR (MIT), NVIDIA Image Scaling (MIT), NVIDIA Streamline headers (MIT). No NVIDIA or AMD binaries are
  included; DLSS is not used or distributed.
- [Sascha Willems' Vulkan samples](https://github.com/SaschaWillems/Vulkan) (MIT): the stand-in shader test corpus.
- [Kenney input prompts](https://kenney.nl/assets/input-prompts) (CC0), DejaVu fonts, [Tauri](https://tauri.app), [Lucide](https://lucide.dev) icons (ISC).

Licence texts: `runtime/THIRD_PARTY_NOTICES.md`, `recomp/THIRD_PARTY.md`, `tools/*/licenses/`, `tools/textures/THIRD_PARTY.md` and the
licence files inside `runtime/third_party/`. All of these licences are compatible with the GPL-3.0 this project is released under.

## License

GPL-3.0 ([LICENSE](LICENSE)). Third-party code keeps its own licence.
