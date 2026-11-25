# Building Gears of War 2 for PS5

A native PS5 homebrew app made from COGBox's Gears of War 2 recompilation (Title Update 6), the way
the [Dead Space 2 PS5 port](https://github.com/chonzytron/Dead-Space-2-PS5) was made from its
recompilation: the same toolchain, the same kind of static program, the same link and packaging steps.
Built on Linux.

> **Status: release 1.0, playable on a PS5 at native 4K / 60 in most of what was played.** See [Status](#status).

## What you need

- clang, lld, CMake 3.25+, Ninja, Python 3, git
- Nazky's [ps5-plume-triangle](https://github.com/Nazky/ps5-plume-triangle) at `d54d9adb`, built once
  with `tools/build.sh` (`ps5/tools/build_ps5.sh toolchain` does it; it is looked for at
  `~/Desktop/ps5-plume-triangle` first)
- **Your own** Gears of War 2 US disc image (game version 0.0.0.5) and its Title Update 6 package:
  unlike Dead Space 2's, COGBox's repository has no recompiled code in it, so the build makes it
  from your `default.xex` + `default.xexp`. Nothing made from them is ever committed
  (`Gears2-Native/.gitignore`: `game/`, `generated/`).

## 1. The SDK

```
cd Gears2-Native
ps5/tools/fetch_sdk.sh
```

Clones ReXGlue at the tag COGBox's PC runtime is built from (`ps5/sdk/REXGLUE_BASE.txt`), moves
the submodules COGBox moves (`patches/sdk-pins.txt`), applies COGBox's SDK patches in the order
`patches/README.md` gives (commits the result and tags it `gow2-patch-base`), then
`ps5/sdk/rexglue_ps5.patch` (the PS5 platform layer, carried over from Dead Space 2) and
`ps5/sdk/ffmpeg_ps5.patch`, into `third_party/rexglue-sdk-ps5`.

## 2. The recompiled game

```
python3 ps5/tools/x360_extract.py get "<your disc image>.iso" default.xex game/tu6/patched/default.xex
python3 ps5/tools/x360_extract.py get <your Title Update 6 package> default.xexp game/tu6/patched/default.xexp
cmake -S third_party/rexglue-sdk-ps5 -B out/host-tools -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DREXGLUE_ENABLE_TRACY=OFF \
  -DSDL_X11_XTEST=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
ninja -C out/host-tools rexglue
third_party/rexglue-sdk-ps5/out/linux-amd64/rexglue codegen gow2_manifest_tu6.toml
```

`x360_extract.py get` prints each file's SHA-1: they must be COGBox's pins,
`b6860bf31e55013467b53eca963d12314c0d78da` (default.xex) and
`8689bf52b2a47908eb9281d05c5e86653db0cf11` (default.xexp). There may be several Title Update 6
packages on a console (one per build of the disc); `x360_extract.py info <package>` shows the
base version each is for, and the right one for the US disc is base version 5. The code generator
writes `generated/tu6/` (446 files, about 230 MB) in under a minute, with COGBox's manifest as it is.

## 3. Build

```
ps5/tools/build_ps5.sh compile package zip
```

The app ends up in `out/ps5/ps5-app/<title id>` and `out/ps5/GearsOfWar2-PS5-<version>.zip`. Copy the
folder to `/data/homebrew/` on the PS5 and put your disc image and Title Update 6 package in its
`install/` folder (`package/install/README.txt`). The title id is a provisional `PPSA94120` (third
argument of `ps5_package.sh`).

Until the release, the package holds a `gow2.toml` for test runs (`package/gow2.test.toml`): the frame
counter, detailed logs (`ps5_perf_log`), a picture every 4 s (`ps5_capture`) and the mark button
(`ps5_mark_button`: L3+R3 saves that picture as `logs/mark_<n>_f<frame>.bmp`, writes a `ps5 mark` line
and logs the next frame's draws). `GOW2_TEST_SETTINGS=0 ps5/tools/build_ps5.sh package zip` leaves it
out for a release, which then needs no settings file. Copy a new package over the old folder minus
`install/` (`unzip -o GearsOfWar2-PS5-<version>.zip -x 'PPSA94120/install/*'`).

## On Linux

The same program built for Linux, to try a change on a PC before it goes on a console: the same app,
the same patched SDK, the runtime and GPU backend linked in as on the PS5, the PS5's log lines and
diagnostics, compiled for the PS5's processor (`-march=znver2`). An AMD GPU uses RADV, the PS5's driver.

```
ps5/tools/linux_test.sh build
ps5/tools/linux_test.sh run "<your disc image>.iso" <your Title Update 6 package> [-- --setting=value ...]
```

It runs from `~/.local/share/gow2-ps5-linux` (`gow2.toml`, `logs/`, `game/`, `UserData/` there, and
`install/` holding links to your files).

## On the PS5

Everything is in the app's folder, `/data/homebrew/<title id>/`:

- `install/`: your disc image and Title Update 6 (`install/README.txt` says exactly what). The first
  start unpacks the title update (2.8 MB) and any DLC, with a screen that says how far it is.
- `game/update/`: Title Update 6's files (mounted as `update:`); the disc image is read where it is
  (mounted as `game:` and `d:`).
- `UserData/`: saves, installed DLC, caches.
- `gow2.toml`: settings, optional (copy `gow2.example.toml`). A test package (`GOW2_TEST_SETTINGS=1`, the
  default of `ps5_package.sh`) carries `ps5/package/gow2.test.toml` as it; a release (`GOW2_TEST_SETTINGS=0`) none.
- `logs/`: one log per start (`gow2_<n>.log`), the pictures (`capture_<n>.bmp`, `mark_<n>_f<frame>.bmp`), with the `ps5 diag` lines, `ps5 stack:` when nothing was drawn for
  30 s, and `[ps5 crash]` lines (`ps5/tools/ps5_symbolize.py` names the addresses, against
  `out/ps5/ps5-link/build/llvm-pie.elf` of the same build).

## Where things are

| | |
|---|---|
| `ps5/sdk/rexglue_ps5.patch` | the PS5 platform layer, on top of COGBox's SDK patches (`gow2-patch-base`) |
| `ps5/sdk/ffmpeg_ps5.patch` | FFmpeg's build configuration for the PS5 (from the GTA IV port) |
| `ps5/cogbox_ps5.patch` | the `[ps5 port]` changes to COGBox's own sources (Windows-only calls given POSIX versions), as a diff against COGBox 0.6.6 |
| `ps5/CMakeLists.txt`, `ps5/cmake/` | the build: SDK, recompiled game, COGBox's game layer and the app as one static program |
| `ps5/src/gow2_ps5_app.h` | the app: start-up settings, the preparing and missing-files screens, mounting the disc image |
| `ps5/src/gow2_ps5_install.*` | finding and preparing the game from the player's files |
| `ps5/src/gow2_ps5_cogbox_glue.cpp` | answers for the parts of COGBox's layer 0 not in this program (its renderer, in-game menu) |
| `ps5/src/gow2_ps5_pch.h` | the recompiled code's precompiled header with the PS5's memory offset |
| `ps5/tools/` | SDK fetch, extraction, link and packaging, Linux test runs, log symbolizer |
| `ps5/package/` | home screen icon and background, `install/README.txt`, the settings template, the test settings |

## Status

Release 1.0 (31 console runs on one PS5): boots, installs from the player's files, menus, Training and the
solo campaign from the menus play; Chapter 1 to the hospital's ending cinematic, Chapter 2 and the start of
Chapter 3 played. Native 4K: Chapter 3's real-time opening cinematic 57-60 per 10 s, the heaviest play (4,000+
draws a frame) mid 50s, the rest 60. Not tested: the rest of the campaign, co-op, multiplayer, Horde, DLC.

Every console run (what limited the frame, what changed, the numbers before and after, the regressions and how
they were found) is in [`../../CLAUDE.md`](../../CLAUDE.md)'s Project section: the optimisation log, as Dead
Space 2's `NATIVE_RENDERER.md`.

## Licence

The PS5 layer is derived from the GTA IV and Dead Space 2 PS5 ports (GPL-3.0) and uses the PS5 payload
SDK's platform library (GPL-3.0): the PS5 files here (`ps5/`, `ps5/sdk/rexglue_ps5.patch`) are under the
GPL-3.0. COGBox's own code keeps its MIT licence and ReXGlue its BSD-style one. None of these cover the game.
