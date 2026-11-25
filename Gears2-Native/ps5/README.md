# Gears of War 2 for PS5

A native PS5 homebrew app of Gears of War 2 (Xbox 360, Title Update 6), for jailbroken consoles. It is
made from [COGBox](https://github.com/Shagg88/COGBox)'s static recompilation of the game: the Xbox 360
code translated ahead of time into a native program, with the ReXGlue runtime and its Xenos GPU
emulation on the PS5's Vulkan driver. No emulator interprets the game while you play.

> **Status (1.0): playable on a PS5, native 4K (3840x2160) at 60 frames a second in most of what was
> played.** Tested on one console over 31 runs: the campaign from the menus (solo), Training, Chapter 1
> (Desperation) to the hospital's ending cinematic, Chapter 2 and the start of Chapter 3 (Roadblocks)
> including its real-time opening cinematic. That cinematic runs at 57-60 frames a second; the heaviest
> scenes of play (4,000+ draws a frame, e.g. an alley in Chapter 3) dip to the mid 50s. Not tested: the
> rest of the campaign, co-op, multiplayer/online, Horde, DLC content. "60 in play" is not "60
> everywhere": expect short hitches when the game streams a new area or first meets a shader (they get
> rarer as the pipeline store fills).

**You need your own copy of the game.** No game files come with this app, and none are downloaded.

## Install

1. Copy the app folder to `/data/homebrew/` on the PS5.
2. Put your files in its `install/` folder (`install/README.txt` says exactly what):
   - your Gears of War 2 disc image (`.iso`): the US disc, game version 0.0.0.5;
   - Gears of War 2 Title Update 6 for that disc, the package file from your Xbox 360
     (`Content/0000000000000000/4D53082D/000B0000/`), as it is;
   - optionally your Gears of War 2 DLC packages (`.../4D53082D/00000002/`).
3. Start the app. The first start checks your files against the ones this build was made from and
   unpacks the title update (and DLC), with a screen that says how far it is. Then it offers to delete
   the packages it no longer needs (Cross deletes, Circle keeps). Later starts go straight to the game.

If something is missing or isn't the right release, the app says so on screen.

## Settings

The app needs no settings file: it starts at native 4K and 60 frames a second. To change something, copy
`gow2.example.toml` to `gow2.toml` in the app's folder; every setting is explained there.

| Setting | Default | |
|---|---|---|
| `ps5_language` | `"auto"` | the game's language (follows the PS5's) |
| `resolution_scale` | `3` | 1 = 720p, 2 = 1440p, 3 = native 4K |
| `ps5_display_height` | `2160` | the TV mode: 2160, 1440 or 1080 |
| `gow2_max_fps` / `vsync` | `60` / `false` | 60 frames a second; `0` / `true` = the Xbox 360's 30 |
| `ps5_motion_blur` | `false` | the game's motion blur (the 60 was measured without it) |
| `ps5_show_fps` | `false` | a frame counter, top right |
| `ps5_skip_intro_videos` | `true` | straight to the title screen |
| `ps5_install_cleanup` | `"ask"` | after the first start: delete the packages no longer needed? |

## Controls

The Xbox 360 controller's, on the DualSense: Cross = A, Circle = B, Square = X, Triangle = Y, L1/R1 =
LB/RB, L2/R2 = LT/RT, Options = Start, touchpad click = Back, L3/R3 = the stick clicks.

## What the port adds

- Your files checked and prepared on the console itself (no PC step).
- COGBox's game fixes, as on the PC: rendering without the Xbox 360's predicated tiling, its texture
  pool and GPU-wait fixes, and its optional patches (`gow2_*` settings).
- 60 frames a second (the game ran at 30 on the Xbox 360) at native 4K, from work on the Xenos
  emulation for this game: Direct3D's clears without the 4x MSAA trick, resolved pictures (the shadow
  atlas, bloom, HDR, depth of field, the scene's depth) written straight into the textures that read them,
  EDRAM copies left out where the next draw covers them, shader constants bound by offset, larger GPU
  submissions. Each was checked picture for picture against the plain path.
- Fixes to the SDK found on the way: the network layer off Windows (the solo campaign runs as a network
  session), FreeBSD's socket addresses, 16 KB host pages and the 0xE0000000 view's write watches, the
  pipeline store growing, a texture exponent-bias bug that flashed new surfaces white.
- Diagnostics for console runs: a frame counter (`ps5_show_fps`), performance lines and a profiler
  (`ps5_perf_log`, `ps5_profile`), pictures for tests (`ps5_capture`, `ps5_mark_button`), crash lines in
  the log.

## Credits

- **COGBox / Gears2-Native by Shagg** and its contributors: the Gears of War 2 recompilation, the game
  fixes and the SDK patches this is built on.
- The PS5 layer comes from the **GTA IV PS5 port (elripalda)** through the **Dead Space 2 PS5 port**.
- **santiquiroz/gears-of-war-2-pc-port**: four of its SDK fixes (texture exponent bias, resolve write barrier,
  unclipped draws, vertex buffers each frame): no white flashes, no teal cast, no stretched armour.
- **mihawk-99** (PS5 payload SDK, PS5 Mesa/RADV), **Nazky** (ps5-plume-triangle), **ps5-payload-dev**.
- **ReXGlue** and the **Xenia** project, whose runtime and GPU emulation this runs on.
- **bifftannen1**'s Dead Space 2 Recomp, whose PDB-named code identified Gears 2's Direct3D clears.

The full list, with links: https://github.com/chonzytron/Gears-of-War-2-PS5#credits

An unofficial fan project, not affiliated with Microsoft, Epic Games or The Coalition. "Gears of War" and
"Xbox 360" are trademarks of Microsoft.

## Licence

The PS5 files (`ps5/`) are GPL-3.0 (derived from the GTA IV PS5 port and the PS5 payload SDK's platform
library). COGBox's own code is MIT, ReXGlue BSD-style. None of these cover the game.
