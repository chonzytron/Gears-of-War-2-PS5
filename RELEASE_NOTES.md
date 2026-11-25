# Release notes

## 1.0 (2026-11-25)

The first release. The Xbox 360 version of Gears of War 2 (Title Update 6) as a native homebrew app for
jailbroken PS5s: **native 4K at 60 frames a second** in most of the game played so far. Built on
[COGBox](https://github.com/Shagg88/COGBox)'s Gears of War 2 recompilation by Shagg88.

**No game files are included. You need your own copy of the game.**

Download: [`GearsOfWar2-PS5-1.0.zip`](https://github.com/chonzytron/Gears-of-War-2-PS5/releases/tag/v1.0)
(the app, `PPSA94120/`), SHA-256 `3190caba2b5fb42d71c69094e1ded956a710e77aa3a0139c772bc907eaa5e4bd`.

### Install
1. Unzip `GearsOfWar2-PS5-1.0.zip` and copy the `PPSA94120` folder to `/data/homebrew/` on the PS5.
2. Put your files in `/data/homebrew/PPSA94120/install/`:
   - your Gears of War 2 US disc image (`.iso`, game version 0.0.0.5)
   - Title Update 6, the package file from `Content/0000000000000000/4D53082D/000B0000/`, as it is
   - optional: your DLC packages (`.../4D53082D/00000002/`)
3. Start the app. The first start checks your files and prepares the game, then offers to delete the
   packages it no longer needs (Cross deletes, Circle keeps). Later starts go straight to the game.

No settings file is needed. `gow2.example.toml` in the folder lists every setting (copy it to `gow2.toml`).

**Updating from a test build:** copy the new folder over the old one, leaving out `install/`. Delete an old
test `gow2.toml` if you have one, since its settings would override the new defaults.

### What's in it
- **The game on the PS5:** COGBox's recompiled Gears of War 2 and its game layer (untiled rendering, texture
  pool, GPU-wait and frame-rate fixes), with the PS5 platform layer for ReXGlue carried over from the
  [Dead Space 2 PS5 port](https://github.com/chonzytron/Dead-Space-2-PS5). The game is prepared on the console
  from your own files, with a first-start screen.
- **60 frames a second** (30 on the Xbox 360): the game no longer waits for every second refresh, and the
  engine's own limiter is set to 60.
- **Native 4K** on the SDK's Xenos GPU emulation. The speed-ups below were each checked picture for picture
  against the plain path:
  - Direct3D's clears without the Xbox 360's 4x MSAA trick;
  - resolved pictures written straight into the textures that read them, and no longer into memory where
    nothing else reads it: the shadow atlas (including each light's partial shadow), bloom, HDR, the depth of
    field's 10-bit picture, the scene depth and the 8-bit pictures;
  - EDRAM copies left out where the next draw covers every pixel;
  - shader constants bound by dynamic offsets;
  - larger GPU submissions (every 1,000 draws);
  - cheaper command-processor timing and memory tracking.
- **Fixes found on the way:**
  - the network layer off Windows (the solo campaign runs as a network session): socket options and errors,
    FreeBSD's socket addresses, non-blocking sockets the PS5 allows;
  - write watches on 16 KB host pages;
  - the pipeline store not growing on the PS5's C library;
  - from santiquiroz/gears-of-war-2-pc-port: new surfaces flashing white (texture exponent bias), the Vulkan
    resolve barrier, a teal cast (unclipped draws), stretched armour (vertex buffers requested each frame).
- **Defaults:** motion blur off, logo videos skipped, a quiet log.

### What to expect
- 60 in most play at native 4K. Chapter 3's real-time opening cinematic runs at 57-60. The heaviest scenes
  (4,000+ draws a frame) dip to the mid 50s.
- Short hitches when the game streams a new area or first meets a shader. They get rarer as the pipeline
  store in `UserData/` fills.
- Played on one PS5 over 31 test runs: the menus, Training, the solo campaign from the menus, Chapter 1 to
  the hospital's ending cinematic, Chapter 2 and the start of Chapter 3.
- **Not tested:** the rest of the campaign, split-screen and online co-op, multiplayer, Horde and DLC content.
- Motion blur is off by default (`ps5_motion_blur = true` turns it on). The 60 fps figures were measured
  without it.

Please report problems with the chapter, what you saw, and the `logs/` folder.

### Credits
COGBox by Shagg88 and its contributors (the recompilation and game layer this is made from);
santiquiroz's gears-of-war-2-pc-port (four SDK fixes); ReXGlue and Xenia; elripalda's GTA IV PS5 port and the
Dead Space 2 PS5 port (the PS5 layer); bifftannen1's Dead Space 2 Recomp; Nazky's ps5-plume-triangle;
mihawk-99's PS5_Mesa and PS5_PayloadSDK; ps5-payload-dev's SDK and pacbrew-repo. The full list with links is in
the [README](README.md#credits).

An unofficial fan project, not affiliated with Microsoft, Epic Games or The Coalition.
