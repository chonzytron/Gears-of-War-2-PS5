# Porting a recompiled Xbox 360 game to the PS5

Copy this file to the root of a new porting repository as `CLAUDE.md`, then fill in the
**Project** section. It describes how two Xbox 360 static recompilations were made into
native, jailbroken-PS5 homebrew apps, what the PS5 needs that a PC build doesn't, how the work
was run with a console in the loop, and what was learnt about performance on the console.

## Project: Gears of War 2 for PS5

Goal: native 4K at 60 fps in play on a PS5, as Dead Space 2 reached.

- Game: Gears of War 2, US disc (En/Fr/De/Es/It/Zh/Ko/Pl/Ru/Cs/Hu), title id `4D53082D`, one disc,
  disc version 0.0.0.5, **Title Update 6** (0.0.6.5). COGBox builds TU6 only.
- Recompiled source tree: `Gears2-Native/generated/tu6/` (446 files, ~228 MB, **not committed**, made
  from the player's own XEX). Generated on Linux by the SDK's `rexglue codegen
  Gears2-Native/gow2_manifest_tu6.toml` (COGBox's manifest, unchanged: 36 mid-function hooks, all
  defined in `src/layer3_gow2/{game_patches,untiled,cheats}.cpp` and `src/layer1_xdk7645/d3d_watch.cpp`).
  The tool is built from the patched SDK (`out/host-tools`, binary in
  `third_party/rexglue-sdk-ps5/out/linux-amd64/rexglue`). Inputs staged in `game/tu6/patched/`.
- ReXGlue SDK: tag `nightly-20261002-bd833a2a` (commit `bd833a2a`, newer than DS2's v0.10.0), with
  COGBox's patch stack (`Gears2-Native/patches/`: `rexglue-sdk-full.diff` = 1-45, then 46-116 in
  number order; 109 after 111; 108's FFmpeg half in `thirdparty/FFmpeg`; 112 without its test
  hunks). `Gears2-Native/ps5/tools/fetch_sdk.sh` does all of it, commits and tags `gow2-patch-base`.
  Submodules moved per `patches/sdk-pins.txt`.
- PS5 layer: `Gears2-Native/ps5/sdk/rexglue_ps5.patch`, DS2's PS5 patch carried onto
  `gow2-patch-base` (87 files). Regenerate from the SDK work tree with
  `git add -N . && git -c core.abbrev=8 diff --binary gow2-patch-base -- . ':!thirdparty/FFmpeg'
  ':!.cogbox-patches-applied' ':!.cogbox-patches-progress' > ../../ps5/sdk/rexglue_ps5.patch`.
  It also makes COGBox's Windows-only online/trace code compile elsewhere (`xam_msg.cpp`
  `HostRegion`, `xam_net.cpp`, `master_client.cpp`).
- GPU path: **start with the SDK's Xenos emulation (Vulkan)**, `--gow2_gpu=sdk`, the path DS2 proved
  on the PS5. COGBox's own renderer (`src/layer0_xenos`, default `gow2_gpu=ours` on PC: D3D12 or
  Vulkan, ucode -> DXBC -> SPIR-V with its own converters, no DXC at run time for game shaders) is the
  candidate "native renderer" if emulation can't reach 4K60. It has never been built off Windows.
- COGBox already hooks the XDK's spinning GPU/query waits (`Gow2GpuWaitStep`, `Gow2QueryWaitStep`,
  `Gow2QueryPollYield`) and the frame-rate limit (`Gow2MaxTickRate`): the Gears 2 equivalents of
  DS2's gentle GPU waits and 60 fps hooks.
- PS5 title id for the package: not chosen yet (DS2 is `PPSA94108`; pick another `PPSA9xxxx`).
- Files the player supplies (SHA-1 = COGBox's pins in `cogbox/src/games.cpp`; SHA-256 for the PS5 installer):
  - disc image: `default.xex` 18,595,840 bytes, SHA-1 `b6860bf31e55013467b53eca963d12314c0d78da`,
    SHA-256 `e98c25b4b9d173ac7ff69c84e7f1b4240c6be77c5fd7f672041a95221f68247c` (XGD2, partition 0xFD90000)
  - Title Update 6 package (`LIVE`, content type 000B0000, base version 5): `default.xexp`
    2,074,624 bytes, SHA-1 `8689bf52b2a47908eb9281d05c5e86653db0cf11`, SHA-256
    `f219c7dc96a4f06b4fcb75441fa554e177a95c05a3196042c03b9da01583337a`. The update package also
    holds TU__*.ini/localisation files and `GearGame/Patches/ScriptPatch_*.bin` the game reads.
  - Other TU6 builds seen: base version 6 (`default.xexp` SHA-1 `acfe0507…`, COGBox "reported
    working"), base version 7 (SHA-1 `708ef468…`, unknown to COGBox).
- `Gears2-Native/ps5/tools/x360_extract.py`: list/extract a file from an XGD image or STFS package
  on the build machine (`info`, `list`, `get`).
- The app (`Gears2-Native/ps5/`, from DS2's `ps5/`; `ps5/BUILD.md`): `src/gow2_ps5_app.h` (DS2's
  app with COGBox's game-layer installs from its `gow2_app.h` OnPostSetup, minus its renderer/window/
  mouse/PC tools), `src/gow2_ps5_install.*` (ISO used in place as `game:`/`d:`; the whole TU6
  package unpacked to `game/update/` = `update:`; DLC unpacked to the content root),
  `src/gow2_ps5_cogbox_glue.cpp` (answers for COGBox's renderer/menu/crash-report functions the
  game layer calls), `src/gow2_ps5_pch.h` (PS5 +0x1000 E0 offset). COGBox's game layer (36 hook
  functions and what they need: 32 files of layers 0-3) is compiled as it is, listed in
  `ps5/CMakeLists.txt` `GOW2_COGBOX_SOURCES`. Several of its hooks are ON by default and change the
  game (`gow2_untiled`, `gow2_patch_gpu_wait=1`, `gow2_patch_query_wait=2`, Coalesced check skip,
  196 MB texture pool, texture stats off, RT pool growth, 720p UI layout): never stub them.
- COGBox source changes for non-Windows (`[ps5 port]`, Windows calls given POSIX versions; the
  guest page table replaces SEH/VirtualQuery safe reads in `objects.cpp`): `ps5/cogbox_ps5.patch`,
  a diff against the pristine copy in `out/cogbox-orig/src` (regenerate it from there).
- SDK hooks added for this port (in `rexglue_ps5.patch`): `rex::input::SetInputStateFilter` (test
  input scripts; DS2 hooked a PDB-named guest function instead), the entrypoint-XEX check also
  accepts a file the VFS finds (the app re-points `game:` at the ISO in `OnLoadXexImage`).
- Known perf item: with the SDK GPU nothing calls COGBox's `XenosNotifyGuestWrite`, so its default
  GPU-wait hook falls back to 1 ms sleeps after 64 spins. Profile on the console; a notify from the
  SDK command processor's fence/read-pointer writes, or a pause/yield mode, are the fixes to try.
- PS5 defaults set by the app: `gpu_plugin=xenos`, `host_present_from_non_ui_thread=false` (COGBox
  launcher), `menu_combo_guard=false` (COGBox patch 43 holds Back+Start for its PC menu),
  `mnk_mode=false`, `vulkan_allow_present_mode_immediate=false`, `vulkan_sparse_shared_memory=false`.
  DS2's renderer optimisations (`ps5_resolve_forward`, `ps5_draw_extent_from_viewport`,
  `clear_memory_page_state=false`, `ps5_reuse_texture_bindings`, `resolution_scale=3`) are in the
  SDK but OFF for Gears 2 until tested on it.
- **Release 1.0 (2026-11-25, `out/ps5/GearsOfWar2-PS5-1.0.zip`, ELF `out/ps5/llvm-pie-1.0.elf`):** the user called
  time on tuning (gains marginal). Run 31 (0.1.23, profiler and CP timers off = release speed): Chapter 3's opening
  cinematic ~60 per 10 s (54-60 at its start), the alley 54-56; no crash, no glitches. Every setting the console runs
  confirmed is now an app default (`DefaultFlag` in `gow2_ps5_app.h` OnPostInitLogging: 4K, vsync false + max fps 60
  + clock_no_scaling, all the forwarding/transfer/constant work, motion blur off, logo videos skipped, `ps5_perf_log`
  false); the release package has no gow2.toml (`GOW2_TEST_SETTINGS=0`), carries README.md, and gow2.example.toml
  shows those defaults. Linux run with no gow2.toml: the settings line equals run 31's. Left for later: the alley's
  command processor (registers ~18 %, IM_LOAD hashing ~2 %), streaming hitches, a seeded pipeline store (needs game
  shaders), the rest of the campaign/co-op/multiplayer untested.
  GitHub repo `chonzytron/Gears-of-War-2-PS5`: `out/ps5/GearsOfWar2-PS5-1.0-source.zip` holds only what the
  release is built from (root README = `README.PS5.md`, this file; `Gears2-Native/` with ps5/, the manifest,
  codegen-templates, the COGBox patches fetch_sdk.sh applies (full diff + 46..116), the 66 src/ files the PS5 and
  Linux builds compile or include (from `ninja -t deps`), licences). Proven from that tree alone: fetch_sdk.sh gives
  the same gow2-patch-base tree and regenerates rexglue_ps5.patch byte for byte, codegen gives the same generated
  code, the Linux and PS5 builds compile. The folder keeps the name Gears2-Native (the manifest's template_dir is
  `../Gears2-Native/codegen-templates`). `out/ps5/RELEASE_NOTES-1.0.md` is the release body.
- **Status (2026-11-23, build 0.1.5):** PS5: compiles with the real toolchain, links with RADV and signs
  (`out/ps5/ps5-app/PPSA94120/eboot.bin`). Not run on a console. Linux test build: installer works, menus, the
  opening cinematic and Chapter 1 (in-engine cutscene with Marcus, Anya, Dom, Carmine) draw correctly at 720p,
  both from a new campaign and from a save slot; 0.1.2 has no white flashes on the Chapter 1 route. Not yet played
  for long or measured.
- **Fixed in 0.1.1, the dark Chapter 1** (all 0.1.0 runs through the menus): Gears 2 runs a solo campaign started
  from the menus as a network session. Off Windows the SDK passed the game's Winsock numbers straight to the host:
  `setsockopt(0xFFFF, SO_BROADCAST 0x20 ...)` and `ioctl(FIONBIO 0x8004667E)` failed on Linux, so the game showed
  "Error initializing network layer" and then loaded a level that never showed (Marcus fell out of an empty world:
  "Objective Failed"). `XSocket::SetOption`/`IOControl` now translate to the host's numbers and swap the
  big-endian values (FreeBSD shares Winsock's numbers, so the PS5 may not have had the first half of this), and
  `xam_net.cpp` translates errno to Winsock codes off Windows (a non-blocking `recvfrom` with nothing to read said
  error 0, not WSAEWOULDBLOCK). A launch straight into a map (`gow2_launch`) never had the problem: no session.
- Test routes on Linux: `--ps5_skip_intro_videos=true` (or `--gow2_launch=-nostartupmovies`) skips the logos
  (title by ~10 s); `--gow2_launch="GearGame_P?loadcheckpoint=0?chapter=1 -nostartupmovies"` + `30:a` reaches
  Chapter 1 gameplay in ~30 s (COGBox's `docs/test-routes.md`). Menu route without logos:
  `12:start 18:a 24:a` = Solo campaign, save slot, Launch (with a save) - cinematic, then Chapter 1.
- Diagnostics: `ps5_dump_frame=N` (+ `ps5_dump_frames`) logs each draw (RTs, textures, shaders, viewport) and
  resolve of frame N. COGBox `config_install.cpp`: game-relative paths use '/' off Windows (`HostRel`). Coalesced
  ini edits and `ue3_mods` need an extracted game folder (they look in `game_data_root` = `game/update` here).
- **First console run (0.1.2, 2026-11-23):** boots, installs, menus, Training plays; ~45-55 fps at 720p
  (`resolution_scale = 1`), GPU 10-65% busy, no crash. Campaign from the menus: Chapter 1 dark (only the HUD), as on
  Linux before 0.1.1: `NetDll_bind: port 1000 refused by the host; any port refused too`. **Fixed in 0.1.3:** the SDK's
  native address mirrors (`N_XSOCKADDR`, `N_XSOCKADDR_IN` in `xsocket.h`) had Windows/Linux's two-byte family; FreeBSD
  (the PS5) starts with a length byte and a one-byte family, so the PS5 read family 0 and refused every bind. They
  follow the host now, with `static_assert`s against the host's `sockaddr_in` / `sockaddr` (`xsocket.cpp`).
- **Second console run (0.1.3):** bind works (port 1000 refused, falls back), but `XSocket: ioctl 8004667E refused by
  the host (errno 13)`: the PS5 refuses `ioctl` on sockets, so FIONBIO never made the session socket non-blocking and
  the campaign from the menus still didn't start. **Fixed in 0.1.4:** FIONBIO through `fcntl(O_NONBLOCK)`, FIONREAD
  falls back to a `MSG_PEEK` (`XSocket::IOControl`). Not yet confirmed on the console.
- **Fourth console run (0.1.5): story mode works** (film, in-engine scene, Chapter 1). 720p: ~54 fps, GPU 22-42% busy.
  The 54 is pacing, not work: the game waits for every 2nd vblank (30 fps on the 360) and adaptive vsync hands late
  frames early vblanks (109 vblanks/s logged). For 60: `vsync = false` (the SDK then gives a vblank every ms, as
  COGBox's renderer does with `gow2_gpu_vsync=false`) + `gow2_max_fps = 60` (engine limit; the game's own is 62) +
  FIFO presentation; `clock_no_scaling = true` as DS2. Not yet tried.
- **4K60 work (2026-11-23, Linux test build, Radeon 760M; `ps5/tools/bench/bench.sh <name> "<flags>"`: the Chapter 1
  walk, 125 s, profiler on, prints fps and the GPU/CP split).** 720p baseline 47.4 fps, GPU 98% busy: draws 6.2 ms,
  EDRAM transfers 6.0 (41/frame), texture loads 4.4, resolves 2.9. The transfers were Direct3D's clears at 4x MSAA
  (VS 1E6883FC, 3-vertex rect list, depth only): **`ps5_full_res_clears`** (`ps5/src/gow2_ps5_clears.cpp`, DS2's
  ps5_lowres_clears; D3D::ClearRect = sub_829A26E8, ClearRects = sub_829A23F8, matched against DS2's PDB-named code)
  -> 8 transfers, 54.6 fps, same pictures. Off by default, on in the test gow2.toml. At native 4K
  (`resolution_scale = 3`) 8.4 fps: draws 34.7 ms, texture loads 40.4 (all resolved pictures read back), resolves 27.4,
  transfers 11.4. DS2's resolve forwarding barely helps (40 -> 36): Gears 2 resolves its shadow views into parts of an
  800x800 depth atlas at 0EF90000 and reuses that memory for bloom (640x360 k_16_16_FLOAT, 328x184 16_16_16_16_FLOAT),
  which forwarding (whole picture into a same-size texture) gives up on. The profiler now times each resolve and
  texture load on its own (`ps5 gpu item N:` lines, `VulkanCommandProcessor::GpuItemScope`). **`ps5_forward_into_larger`**
  (with ps5_resolve_forward): a D24S8 resolve from the origin of an up-to-date k_24_8 texture as large or larger at the
  same address is also written by DS2's one-pass depth shader straight into it, which is then marked up to date
  (`BeginDepthWriteIntoLarger` / `EndDepthWriteIntoLarger`, `WriteDepthIntoTexture`; memory still written, so other
  writers still invalidate it). Atlas loads 5 -> 1.7 a frame, 4K texture loads 35.9 -> 26.5 ms, 8.9 -> 9.7 fps here;
  `ps5_resolve_forward_check` (extended to this path) 14 times 0 of 5,760,000 pixels different. Also seen: DS2's colour
  forwarding of the front buffer 1F5F8000 differs in up to 258 pixels of column 0 (max 21/255) - to look at.
  **Colour too (same setting):** bit-identical float pairs (k_16_16_FLOAT, k_16_16_16_16_FLOAT, k_32_FLOAT, no exp
  bias: the bloom chain) are image copies into the up-to-date texture (`BeginColorCopyIntoTexture`, texture may be
  smaller than the picture); 7e3 -> k_16_16_16_16_FLOAT with any exp bias (the HDR scene, bias -3) goes through
  `shaders/ps5/resolve_forward_7e3_rgba16f_image.cs.glsl` (the dump's 7e3 encode, the copy's decode, x 2^bias, into an
  RGBA16F storage view; such textures now get STORAGE usage, `storage_format_`). Every path repeats the scaled
  resolve's half-pixel offset fill (row/column 0 from 1: `GetResolveFill`; also fixed DS2's depth one-pass, whose
  shader now takes fill_x/fill_y) - without it row 0 and column 0 differed. `ps5_resolve_forward_check` covers all
  paths and rotates between pictures: atlas, bloom (two targets), HDR (7680x2160 as 32-bit words) and the front
  buffer, every check 0 values different. **4K here: 8.4 -> 10.2 fps; texture loads 40.4 -> 14.4 ms**, resolves 32.8
  (incl. the forwarding shaders), draws 34.4, EDRAM transfers 11.1 (9/frame: bloom over the shadow depth at tile
  1440/1624, HDR 7e3 over the 8888 picture at tile 720, depth reinterpreted kD24S8 <-> kD24FS8 - likely many dead
  copies of targets fully overwritten next; `ps5_edram_transfers` = all|none|color|depth only).
  **`ps5_skip_dead_transfers`**: for the draw that triggers a transfer into a 32bpp 1x colour target, if it can't leave
  a pixel as it was (no kill, alpha test, alpha-to-coverage, depth/stencil test; blend one/zero; all the format's
  channels written) and is one rectangle (rect list of 3, or a triangle list of 6 whose two triangles split one box
  along the same diagonal - `DrawExtentEstimator::EstimateRectangleBounds` runs the VS on the CPU for x and y), the
  covered rectangle (with the scissor) is a cutout: only what lies outside it is transferred
  (`FindDeadColorTransfers`, `PerformTransfersAndResolveClears(..., &covered)`; `ps5_log_kept_transfers` says why one is
  kept). 4K transfers 11.1 -> 4.1 ms, **11.0 fps** here. **Whole-frame check:** the pause screen of a direct Chapter 1
  launch with no pad is static - two plain runs differ only in the menu highlight box (~70..536 x 268..305 of the
  1280-wide capture); with every optimisation on, 0 pixels differ outside it (after adding the half-pixel fill to DS2's
  8888 colour forwarding too, which differed in column 0). Use this for any picture-changing work.
  **60 fps pacing (Linux, 720p):** `vsync = false` (guest only: vblanks every ms, the 2-vblank wait no longer holds it;
  host presentation stays FIFO) + `gow2_max_fps = 60` + `clock_no_scaling = true`: 54.7 -> 60.0 fps, 5 frames over 20 ms
  per 10 s. Game time keeping real time at 60 still to be confirmed on the console (cutscene/audio sync).
  **0.1.6 test package** (`out/ps5/GearsOfWar2-PS5-0.1.6.zip`): 4K (`resolution_scale = 3`), every change above on, 60 fps
  settings, profiler on, net trace off. Next after its console numbers: the resolves' memory writes when only forwarding
  reads them (careful: 0EF90000 is shared by the atlas and bloom),
  the scene depth D24FS8 -> k_24_8_FLOAT (resolve 3.7 + load 4.2), then 60 fps pacing, then texture streaming.
  **Fifth console run (0.1.6, 4K, 60 fps settings):** mostly 58-60, dips to the low-to-mid 40s. GPU ~13 ms a frame
  (draws 7.8, resolves 2.8, loads 1.5): not the limit. The dips are the command processor in frames of 2000-3900 draws
  (CP 20-50 ms, 13-23 MB uploaded a frame; worst: guest memory 3.5, bindings 3.0, textures 1.8, pipelines 1.5,
  primitives 1.2 ms) and 11 frames building pipelines. **0.1.7** (`out/ps5/GearsOfWar2-PS5-0.1.7.zip`): the test
  gow2.toml adds DS2's `clear_memory_page_state = false` (Linux: uploads 4.3 MB -> 46 KB a frame, guest memory 1.7 ->
  0.3 ms) and `ps5_reuse_texture_bindings = true` (small here); the still-frame check 0 pixels different, play the
  same. **Pipeline storage never grew on the console:** `4D53082D.fbo.vk.xpso` stayed at run 1's 378 pipelines while
  each run made hundreds more. The loader reads the descriptions to the exact end without reaching end-of-file, and
  C needs a positioning call before the writer thread appends; glibc lets it pass, the PS5's C library lost every
  append (the shader storage, read until a read fails, grew). Fixed with a seek to the end in
  `vulkan/pipeline_cache.cpp`; to confirm: the console's xpso grows after a run, and the next run's
  "Created N graphics pipelines from Vulkan storage" is larger. Still open: seeding a pipeline store in the package.
  **Sixth console run (0.1.7):** pipeline storage fix confirmed (378 -> ~1,338 stored). Uploads 200-700 KB a frame
  (were 13-23 MB). First run to reach the hospital's ending cinematic (`SP_Hospital_Cine_Ending`): Tai's armour stretched
  into spikes there and after it, and once all three characters' armour copper (mark 3). Suspected cause: the SDK's
  IssueDraw skipped re-requesting a vertex buffer whose slot had the same address and size last time - for the whole
  session (`vertex_buffer_states_` is never reset), so memory the game reused for another mesh, or rewrote in place,
  kept its old vertices on the GPU. **0.1.8** requests it again each frame (santiquiroz's patch 0059, from a Gears 3
  recompilation's report; Xenia requests every draw); `gpu_vertex_buffer_state_skip = true` is the old behaviour. Linux:
  no cost. Not reproduced on Linux (no quick route to that scene), so unconfirmed. Dips now: outdoors at the end of the
  hospital the GPU is the limit at 4K (87-97 % busy, ~16 ms a frame: draws 6.8, resolves 6.1 of which shadow-atlas
  depth ~2.5, texture loads 2.3: atlas 0.5, scene depth k_24_8_FLOAT 0.5, HDR 0.5); some frames of 3,000+ draws are
  the command processor (~13 ms of draws); others wait on the game's own threads (streaming). The atlas (800x800 D24S8),
  bloom (640x360 k_16_16_FLOAT, 328x184 k_16_16_16_16_FLOAT) share one base (console 107F0000, Linux 0EF90000): each
  resolve invalidates the others' textures, so they are reloaded from memory every frame. Next GPU work: forward
  k_24_8_FLOAT (scene depth) and float colour whole pictures through DS2's slot path (works when the texture was
  invalidated), then skip resolve memory writes that only forwarding reads.
  **Seventh console run (0.1.8, Chapter 2 from a save, 4 min):** 60 fps in nearly every 10 s window, GPU 46-73 % busy
  (~8 ms a frame), 0-11 slow frames per 10 s. The stretch fix was not checked (the hospital ending wasn't replayed). The
  slowest frames: pipelines for areas new to the store (a frame using an async placeholder pipeline is not presented,
  `vulkan_async_skip_incomplete_frames`, so it shows as a hitch; 2 of the 3 worst swaps), the odd GPU spike, and
  frames waiting 17-20 ms on the game's own threads. The store fills as the player goes; seeding it in the package
  would need the .xsh (the game's shader microcode, i.e. game data).
  **0.1.9 (Linux, 4K):** (1) the scene depth (D24FS8 resolved to a k_24_8_FLOAT texture at 0FA58000) goes through
  DS2's whole-picture depth forwarding with new float24 shaders (`shaders/ps5/resolve_forward_depth_float24*.cs.glsl`:
  the dump's PreClampedDepthTo20e4 with the 0-0.5 remap, truncating, then xenos::Float20e4To32; only when
  `depth_float24_round` is false), and after 30 forwarded reads it is no longer written to memory: its resolve + load
  7.9 -> 1.6 ms. (2) The into-larger paths (depth, colour copy, 7e3) also take an OUTDATED texture when the resolve
  covers every texel of it (the atlas and the bloom pictures share one base and outdate each other): atlas loads 2 -> 1
  a frame. 11.7 -> 12.8 fps here; forward checks all 0 different (incl. 0FA58000), still frame 0 pixels outside the
  menu highlight. Not taken: the 322x182 k_16_16_16_16_FLOAT bloom (its source fails FindColorCopySource's layout
  checks; ~0.2 ms on the console); skipping the memory writes of the atlas resolves (its texture still loads once a
  frame from memory, so not safe).
  **Eighth console run (0.1.9, Chapter 1 replayed, 17 min):** Tai's armour right in the ending cinematic (the 0.1.8
  vertex-buffer fix confirmed). The scene depth no longer appears among the GPU items. Dips: (a) indoor frames of
  4,300-4,500 draws where the command processor takes 16-20 ms of draws (~4.6 us a draw: guest memory 3.7, textures
  2.5, bindings 4.4, primitives 1.7, render targets 1.4) plus 4.5 ms of register writes (36k); (b) the hospital ending
  outdoors, GPU ~17 ms a frame (98 % busy). The console's SIGPROF profile of the command processor (symbolized against
  that build's llvm-pie.elf) showed my forwarding lookups scanning every texture on each resolve (~6 %),
  GetLoggerRaw for debug/trace lines that never print (~3 %), RADV descriptor writes (~4 %) and RADV's clock reads
  (~2.6 %). **0.1.10:** an index of textures by base page (`TextureCache::ForEachTextureAtBase`, rebuilt when a texture
  is created or destroyed) for those lookups; `rex::g_log_verbose_possible` lets debug/trace macros skip the logger
  lookup when no category logs at that level (checked: 1,330 debug lines with log_level=debug, none without).
  Forward checks still 0 different. `occlusion_query_enable = false` measured worse at 4K here (more draws, 12.8 -> 12.5).
  **0.1.11:** per-packet timing (two `sceKernelReadTsc` library calls a packet, ~65,000 packets in a busy frame, ~6 % of
  the console's command processor) is now behind `ps5_packet_timing` (default false: the slow-frame lines' "by kind"
  list is then empty); `VulkanTexture::GetView` keeps its last view (~2 %). With 0.1.10's two, ~17 % of the busy
  frames' command-processor samples removed (console profile shares; to be measured there). Checks: forward 0
  different, still frame 0. Left: constant-buffer descriptor writes every draw (UE3 changes constants each draw;
  dynamic offsets would be the fix), ~36k register writes a frame, shader ucode hashed at each IM_LOAD.
  **0.1.12:** type-0 register packets (runs of registers) go through `WriteRegisterRangeFromRing` as in Xenia (shader
  constants copied as a block, dirty bits once per range; other registers still one by one), and the per-register
  debug check is behind `g_log_verbose_possible`. Linux 720p: the command processor waits for the game 9.6 ms a frame
  instead of 8.1 (0.1.10/0.1.11's changes included); still frame 0 pixels, play run no white frames.
  **0.1.13:** `ps5_fast_ticks` (default false, on in the test gow2.toml): `ps5diag::Ticks()` reads the TSC with `rdtsc`
  instead of `sceKernelReadTsc` (same counter; its frequency stays sceKernelGetTscFrequency). The command processor's
  timers read it ~15 times a draw plus 4 per memory request. Not known yet whether user code may run rdtsc on the PS5:
  if 0.1.13 crashes at start, this is the first suspect (turn it off in gow2.toml). Confirm: sane slow-frame numbers.
  **Ninth console run (0.1.13):** `ps5_fast_ticks` works (rdtsc allowed; timings sane). Severe glitches in the hospital's
  second room: fog/light planes drawn hard and pale (already a smaller patch in 0.1.9's mark). Cause (most likely):
  0.1.9's scene depth (112B8000 there) stops being written to memory after 30 forwarded reads; something other than its
  own resolve then invalidated the slot (volatile pages are hashed only when a draw asks, so a CPU write from before
  the resolve is noticed after it), and the k_24_8_FLOAT texture loaded the never-written memory. **0.1.14:**
  `ForwardedRangeWritten` turns such a slot back to "always written to memory" (log: "written by something else ...
  while only forwarding held it"); the slot's own resolve marks its range through `MarkRangeAsResolvedByForward`, which
  doesn't count. Not reproduced on Linux (the walk doesn't reach that room); confirm on the console. Cutscene dips:
  GPU-bound at 4K (90-93 % busy; draws 8.6 ms for 2,358 draws, resolves 3.7, loads 2.7 of which the atlas 0.85).
  **Tenth console run (0.1.14):** the "written by something else" line never fired, so 0.1.14's theory didn't hold.
  Glitches again: brown planes in a hospital room for a few seconds, and a dead Locust's gore as exploded spikes
  (mark at 16:59). Every glitch so far (Tai 0.1.7, pale planes 0.1.9/0.1.13, these) is geometry the game rewrites
  every frame (particles, fog sheets, gore) and started after `clear_memory_page_state = false` (0.1.7): the leading
  suspect is a CPU write the upload tracking misses (volatile pages are unprotected and only hashed when a draw asks;
  the primitive processor's converted-index cache is only invalidated by write faults). Next step: one console run with
  `clear_memory_page_state = true` to confirm or rule it out. Dips: 36-50 fps at the meatshield tutorial = command
  processor on 6,300 draws a frame (2.9 us a draw, down from 4.6 in 0.1.9: the 0.1.10-0.1.13 trims worked); bindings
  5.4 ms is now the largest part (constant/texture descriptor writes per draw - dynamic uniform offsets would be the fix).
  `ps5_motion_blur = false` (app) adds `Xe-GearEngine.ini|SystemSettings|MotionBlur=False` (checked: sharper while
  turning); off in the test gow2.toml.
  **Eleventh console run (0.1.14, drive's gow2.toml with `clear_memory_page_state = true`):** no glitches in play, but
  the real-time ending cinematic (`SP_Hospital_Cine_Ending`, a level streamed in during play, ~1,200 draws a frame)
  broken: world geometry stretched into spikes and black sheets in some camera shots, others right. On Linux the same
  setting draws Chapter 1 play and "Choose path" right (no Linux route reaches a real-time cinematic yet; the opening and
  Chapter 2's speech are videos: 3 draws a frame - tell them apart by draws per frame). The console's shared-memory
  pages are 16 KB (log: "(16 KB pages)"), Linux's 4 KB: the PS5's write tracking takes paths Linux never does. No memexport
  draws on the Linux routes (`ps5 memexport draw` lines, first 64 logged). **0.1.15:** `ps5_upload_check = N` (s) reads
  back the GPU's copy of one frame's requested pages and compares with guest memory: "DIFFERENT while valid" = a CPU
  write the tracking missed; also volatile pages wrong by their hash, and valid pages the guest can write through a
  view that doesn't watch them (A0/C0/E0). Linux: all 0. On (10 s) in the test gow2.toml, `clear_memory_page_state =
  false` again. ELF kept as `out/ps5/llvm-pie-0.1.15.elf`.
  **Twelfth console run (0.1.15):** Tai's armour stretched again (so 0.1.9 being right was luck). The check: every
  10 s 3-13 valid pages the game can write through the 0xE0000000 view but that view doesn't watch. **Cause:** with
  16 KB host pages one covers four of E0's 4 KB guest pages, and `PhysicalHeap::EnableAccessCallbacks` (and the
  unprotect in `TriggerCallbacks`) looked only at the first guest page's access: first one read-only/unused = the whole
  host page unwatched, CPU writes to the other three never seen. **0.1.16:** both use `WatchHostPageAccess` = the
  union of the covered guest pages (`BaseHeap::HostPageUnionAccess`, factored out of `SyncHostPageAccess`);
  `ps5_watch_first_guest_page = true` is the old rule. **Linux 16 KB test mode:** `REX_TEST_HOST_PAGE_SIZE=16384`
  (env) makes the Linux test build's memory system use 16 KB pages and the 4 KB E0 offset in code, like the console
  (`gow2_test_e0_host_offset` in the PCH): it reproduces the console (old rule: 3-14 unwatched pages a check; fix: 0, and
  0 real differences in 22 checks). Page 00A04000 differs on both but only outside the 4 KB pieces draws read (the
  runtime writes it itself; the check now says "only outside what the draws read"). **Test route from the user's save**
  (copied from the drive into the Linux profile; Linux's own in `~/.local/share/gow2-ps5-linux/UserData-backup-linux-*`):
  `AB_FRESH=0`, `--ps5_input_script="12:start 18:a 24:a 34:lb/3000 38:lsup/30000 69:lsup/30000"` = hospital, the
  street fight at the barricade (~2,000-2,250 draws). Script stick names are `lsup` etc. (`lup` is not one). Idea from the
  user, not done: record the console's inputs to replay them here (the script needs analogue stick values first).
  **Thirteenth console run (0.1.16):** the watch fix holds (E0 0/0, 0 real differences in 16 checks), but Tai's arm still
  grows blades and once every character's armour is copper - so not the watch. **Second cause (in the SDK since the
  start):** `vertex_buffers_in_sync_` - a vertex buffer requested once in a frame was not requested again in that frame
  while its fetch constant stayed the same, even after the game rewrote it (Xenia requests every vertex buffer of every
  draw; the end-of-frame upload check can't see it, the page is then correctly invalid). **0.1.17:** an in-sync buffer is
  skipped only if `SharedMemory::IsRangeCurrentForDraw` (all pages valid, volatile ones looked at in this batch of
  commands); `ps5_vertex_buffer_frame_sync = true` is the old rule. Linux, the save's street fight: 236-1,225
  re-requests per 1,800 frames (draws the old rule drew stale), guest memory 1.03 -> 1.07-1.20 ms a frame; counting
  every page with a write-caught counter instead (3.3 ms) or without the batch rule (2.4 ms) cost far more. Log line
  `ps5 vertex buffers: N requested again in the last 1800 frames`.
  **Fourteenth console run (0.1.17):** both fixes active (1,300-4,000 re-requests per 1,800 frames, memory check clean),
  Tai's armour still wrong: a third cause, not stale memory. Tools added (source only, not in a package yet):
  `ps5_input_record` / `ps5_input_replay` (+ `ps5_input_replay_by_time`): drifts within seconds even Linux->Linux
  with `-FIXEDSEED`, so no way to the cinematic; `ps5_mark_trace = N` (a mark also records the next frame as a GPU
  trace, `RequestFrameTrace`) and `--gow2_hold_guest` in the app for `--trace_gpu_replay`: plays back (2,035 draws) but
  black - the Vulkan backend never records memory into traces (only D3D12 does: `InitializeTrace` downloads,
  `UploadRanges` WriteMemoryRead, `TracePlaybackWroteMemory`); not finished. The user then asked for 0.1.10 on the drive
  (as packaged) to test whether the armour is right there, and to get back to the 4K60 goal.
  **Fifteenth console run (0.1.10 package as built then):** Tai's armour RIGHT (the user expected it; I had wrongly
  called 0.1.9 luck and spent five builds on other causes - lesson: when a regression appears, rerun the last good
  package first). Play at 60 fps, 2-30 slow frames per 10 s. So one of 0.1.11-0.1.14's changes broke it. **0.1.18:**
  they are settings now, 0.1.10's behaviour in the test gow2.toml: `ps5_packet_timing = true` (default true again;
  false = 0.1.11's direct path), `ps5_texture_view_cache` (0.1.11, default false), `ps5_register_ranges` (0.1.12,
  default false), `ps5_fast_ticks = false`; upload check off; the 0.1.16/0.1.17 memory fixes on. Next runs turn one
  suspect on at a time. Slow-frame detail lines now only count frames of play (50+ draws), first 300 then 1 in 20.
  **Runs 16-18 (0.1.18, gow2.toml edits only):** both memory fixes on = armour broken; both on the old rule = right;
  0.1.17's re-requests alone on = broken. **Settled: 0.1.17 (`ps5_vertex_buffer_frame_sync = false`) breaks Tai's
  armour**; default back to true (the SDK's rule). (So the GPU must NOT see some rewrites the game makes within a frame:
  likely the command processor runs behind the game, which already writes the next frame's data into buffers this
  frame still draws.) 0.1.16's watch fix not yet tested alone. Play at 60; the ending cinematic 55-56 fps: 188 slow
  frames from the command processor (~2,400 draws, CP 22 ms, draws 13.6 ms), 77 the game's threads, 42 the GPU.
  **Runs 19-22 (gow2.toml edits):** 0.1.16's watch fix alone: armour right (kept). All four 0.1.11-0.1.13 speed-ups on:
  broken, but CP per draw ~11.7 -> ~7.6 us in the cinematic's CP-bound frames. Packet path + fast ticks: right (kept).
  **Register runs (`ps5_register_ranges`, 0.1.12) on: broken - the second culprit** (the code reads as equivalent to
  one-by-one writes; not explained, left off). Run 23: texture-view memory on alone. Screenshots (L3+R3 marks, captures)
  cost hitches of their own: leave marked frames out of performance readings.
  **Run 23 (texture view cache on): armour right - all four kept.** **Run 24 (profiler and captures off, Chapter 2 into
  Chapter 3 "Roadblocks", 15 min):** play 60 almost everywhere; the dips (49-57) are Chapter 3's real-time opening
  cinematic (`SP_Landown_Derrick` streaming in; 1,200-2,800 draws), GPU-bound at 4K (90-94 % busy, ~20-25 ms of GPU a
  frame, the game never waited). Linux route to it: `--gow2_launch="GearGame_P?loadcheckpoint=0?chapter=4
  -nostartupmovies"` + `30:a` (the cinematic plays ~40 s in; chapter numbers are 0-based: 1 = Desperation; the menu's
  Chapter 2 "Rolling Thunder" intro is a video). Linux 4K there: draws ~45, resolves ~47, loads ~19-29, transfers ~9 ms.
  Two full-screen pictures share one address (0E488000 on Linux; the hospital's HDR at 0F328000): the HDR scene
  (7e3 -> k_16_16_16_16_FLOAT, bias -3) and its bits resolved again as k_2_10_10_10 (format 7; the owning target is
  the 7e3 one - the depth of field reads it); each resolve outdated the other's texture, so both loaded every frame.
  `ps5_log_forward_misses` (tests) logs why a sizeable scaled resolve isn't forwarded (1st and 300th time).
  **0.1.19:** (1) 0.1.9's "take an outdated texture the resolve covers" required no packed mip tail; Gears 2's
  pictures always set packed_mips, so it never fired. Now: the base level is packed only if its smaller side is <= 16
  (`texture_util::GetPackedMipLevel`); `ps5_forward_packed_rule_old` = the old rule. Loads 29 -> 19 ms. (2)
  `ps5_forward_10bit`: k_2_10_10_10 resolves into the A2B10G10R10 texture by `resolve_forward_rgb10a2_image.cs.glsl`
  (from a k_2_10_10_10 target - held as 8 bits a channel, A8B8G8R8 - as uint(c*1023+0.5); from a 7e3 target with the
  dump's 7e3 encoding); such textures get STORAGE usage if the device can. Alone: no gain (its shader ~ the load).
  (3) `ps5_forward_skip_memory`: a colour picture written into its texture by a forward-into path, proven for 30
  resolves, isn't dumped/copied to memory (range still marked; `IntoSkipSlot`s). A later load from that memory turns it
  back for good, unless a resolve wrote all those bytes to memory since (`NoteResolveWroteMemory`; the depth-of-field
  texture's first load after its own resolve); and no skip while another texture at that address isn't one such
  picture (the bloom over the shadow atlas: the atlas is loaded from that memory now and then). With
  `ps5_resolve_forward_check` the skip yields when a check is due. Chapter 3 cinematic, Linux 4K: 7.10 -> 8.29 fps
  (+17 %), resolves 47 -> 35 ms; every check 0 different (HDR, 10-bit, atlas, bloom, depth, front buffer; Chapter 1
  too). Defaults now: `ps5_packet_timing = false`, `ps5_texture_view_cache = true`, `ps5_fast_ticks = true`. The test
  gow2.toml: 10-bit and skip on, profiler and captures off (compare with run 24).
  **Run 25 (0.1.19, Chapter 3 restarted, opening cinematic ~40 s recorded):** no glitches. Cinematic's first 40 s per
  10 s window: run 24 52.3 / 55.5 / 52.8 / 48.8 -> run 25 54.6 / 60.1 / 59.1 / 56.1 (59.4 next); GPU 85-92 % busy,
  slow frames nearly all 20-29 ms (the on-screen counter shows high 40s in those stretches): GPU-bound at 4K. Skip
  lines: bloom 10700000 (format 31), HDR 10B88000 (format 32) and its 10-bit twin (format 7) no longer written to memory;
  107F0000 (format 31) reverted once when the atlas (format 22) at that base was read from memory (first frames of
  the Derrick level). The 38 fps windows before it are the Landown loading screen (under 50 draws a frame).
  Next (Linux profile of that cinematic): the shared base of the shadow atlas / bloom / an 8888 1280x720 picture
  (0E0F0000 on Linux, 107F0000 on the console) is ~20 of ~90 ms of resolves + loads + draws: atlas loaded from memory
  2x a frame (5.8 ms), the 8888 picture's resolve (3.7) and load (1.8), 16_16_FLOAT bloom resolves (3.0), atlas
  depth resolves (5.3) - each picture outdates the others there.
  **Run 26 (0.1.19, profiler on, Chapter 3 cinematic in full then play, 6 min):** cinematic 56-60 per 10 s; console GPU
  a frame in it: draws 8.3 ms (1,910 draws), resolves 4.3, loads 2.0, transfers 1.0 (~16 ms): emulation is ~46 %. The
  shared base 107F0000 alone ~4.3 ms: atlas loaded from memory 3.1x a frame (1.0 ms), the 8888 1280x720 picture's
  resolve (0.68) and load (0.51), 16_16_FLOAT bloom resolves 4x (0.59), atlas depth resolves (~1.1), 322x182 bloom.
  Play dips (49-55, 15:18:50-15:19:40, ~3,000-3,600 draws) are the command processor. The HDR at 10B88000 reverted
  once (15:17:51): its 10-bit texture (format 7) was loaded from memory while both were skipped - "this once read what
  was there before", so one frame's depth of field may have read stale data (nothing seen). Order in a cinematic
  frame (Linux dump, 0E0F0000): atlas resolves, 8888 picture, atlas x3, 8888 picture, atlas x10, bloom fp16 x4,
  bloom rgba16f 322x182 x3. Each light's shadow is resolved to the atlas's top rows (800x448..640 of 800x800).
  **`ps5_forward_partial_depth`** (after run 26, Linux): the depth into-larger path also takes an OUTDATED texture its
  resolve covers only partly, when every 4 KB page of the texture is still marked scaled-resolved
  (`TextureCache::IsRangeFullyScaledResolved`; a CPU write unmarks a page): the left-out rows hold another picture's
  bits on the 360 too. The forward check skips such writes (their left-out rows differ from memory on purpose).
  Linux 4K, Chapter 3 cinematic: atlas loads 3.3 -> 0.9 a frame, texture loads 11.3/13.1 -> 6.4/7.6 ms, fps 8.1/7.4
  -> 8.3/7.7. Check: `ps5_test_partial_depth_fill = 1|2` fills the texture with depth 0 / 1 before such a write;
  the cinematic's shadows looked the same either way (the game doesn't read the left-out rows). The Chapter 1 and
  Chapter 3 pause scenes never take this path (their still frames say nothing about it). **0.1.20**: on in the test
  gow2.toml, profiler on (compare with run 26).
  **Run 27 (0.1.20, same route as run 26):** no visual problems reported, "minor dips". Cinematic per 10 s 59.1 / 60.1 /
  57.1 / 58.6 / 60.1 / 60.1 / 59.8 (run 26: 58.6 / 57.8 / 56.1 / 59.3 / 59.7 / 58.9 / 59.9). Console GPU texture
  loads in it 1.8-2.0 -> 0.9-1.1 ms a frame; atlas loads 3.1 -> 0.8 a frame. Left at 107F0000 (~3.5 ms): the 8888
  1280x720 picture's resolve 0.73 + load 0.42, bloom fp16 resolves 0.58, atlas depth resolves ~1.2, atlas load 0.27,
  322x182 bloom load 0.18 + resolve 0.16. HDR 10B88000's 10-bit texture was again read from memory once in play
  (16:00:47; on the 360 that memory holds the HDR picture's bits too, so it is garbage either way).
  **`ps5_forward_8888`** (Linux): k_8_8_8_8 -> k_8_8_8_8 resolves take the colour copy into-path; with the swap bit
  (red/blue) the copy is a blit from the R8G8B8A8 target into the texture's B8G8R8A8 image (`bgra_image`, viewed as
  R8G8B8A8: the same bytes the resolve writes), only when the texture's endianness is the resolve's. It first never
  fired: DS2's whole-picture slot claimed those resolves (forward_source), and the atlas resolves outdated the slot
  before its texture read it. Now an 8888 resolve to an address where a texture of another format also starts
  (`HasOtherFormatAtBase`) takes the copy instead of the slot. Linux 4K cinematic: the 1280x720 picture's loads gone,
  texture loads 7.0/8.2 -> 4.0/5.2 ms; checks of it 0 of 8,294,400 different. Atlas checks can now differ only outside
  the rows/columns a partial resolve covered (some shadow resolves are 320x192 or 704x448). **0.1.21**: on in the test
  gow2.toml with the partial depth rule and the profiler.
  **Run 28 (0.1.21):** cinematic dips minor (57-60 per 10 s). A Chapter 3 play stretch (an alley after the first
  fight; marks f14793, f15429, 16:38:40-16:39:30) drops to 39-49 per 10 s with nothing happening: the command
  processor, not the GPU (GPU busy 59-72 %, ~15 ms). 4,000-4,500 draws a frame (UE3: 673 depth-only pre-pass draws,
  then base and per-light passes); CP 22 ms in slow frames: draws 13 (bindings 3.5-4.0, guest memory 1.1, textures
  1.3, render targets 1.3), the rest ~9 (registers, packets). CP profile (profiler on): register writes ~12 %,
  RADV descriptor writes ~5.5 %, Ticks() 5 % (profiler timers), shader ucode hashing at IM_LOAD ~2 %. Submissions:
  `vkQueueSubmit` ~0.5 ms each on the console (7.2 ms a frame for ~13 submissions of 300 draws, on the submission
  thread); the swap's part on the CP 3.6-4.0 ms. The register range path (0.1.12) reads as equivalent to one-by-one
  writes (checked again); its armour breakage is more likely timing (the CP running faster) than its logic.
  **Run 29 (0.1.21, drive's gow2.toml + `ps5_submit_chunk_draws = 1000`):** alley high 40s (run 28: low 40s); its slow
  frames' CP 22.4 -> 19.8 ms (4,440 -> 4,620 draws), the swap's part 3.6-4.0 -> 0.7-0.8 ms, only ~2 cuts a frame;
  "submitted" stays ~6.5 ms (vkQueueSubmit's cost grows with the commands, not their count). Cinematic 59-60 per 10 s,
  slow frames 6-30 per 10 s (run 28: 10-109). 1000 is the default from 0.1.22.
  **Run 30 (0.1.22, dynamic constants, profiler on):** alley 52.8-57.5 per 10 s (run 29: 45.5-51.0), cinematic
  56-60 (felt mid 50s), no visual problems. Alley bindings 3.5-3.8 -> 2.0-2.3 ms a frame; slow frames still ~22 ms
  of CP with "the rest" (commands, registers, swap) 8-19 ms from frame to frame. CP profile: register writes ~18 %
  (the code is already tight: volume, ~100 ns a write), Ticks() 6.4 %, lock/yield ~3.7 %, hash lookups ~2.3 %.
  **0.1.23:** `rex::ps5diag::counters` each in their own 64-byte line (`PaddedCounter`; the CP and the submission
  thread were adding to neighbours in one line), and the CP's ScopedTicks only while reported
  (`timers_on` = (ps5_perf_log or ps5_profile) and `ps5_cp_timers`, set by the app at start; release logs paid
  for them before). Linux 720p cinematic: CP ~10 % less a draw with timers on (e.g. 5.47 -> 4.60 us). The test
  gow2.toml has the profiler and `ps5_cp_timers` off: release speed, the 10 s lines only.
  **`ps5_dynamic_constants`** (read at start; Linux): the five guest constant buffers are UNIFORM_BUFFER_DYNAMIC with
  a fixed range at offset 0 (floats 4 KB, the others their size) and the upload offset as a dynamic offset
  (`GraphicsUploadBufferPool::Request(..., reserve)` keeps the whole range inside the page); a draw whose constants
  changed but whose buffers are the same pages rebinds this frame's set with new offsets instead of allocating and
  writing one (sets are only reused in the frame that allocated them: current_graphics_descriptor_sets_ is cleared at
  frame start). Linux 720p Chapter 3 cinematic: bindings 1.05 -> 0.64 us a draw; still frame 0 pixels different
  outside the menu highlight. **0.1.22** (`out/ps5/GearsOfWar2-PS5-0.1.22.zip`): on in the test gow2.toml.
- **Third console run (0.1.4): story mode still dark.** The PS5 refuses `fcntl` on a socket too (EACCES). 0.1.5: a
  socket the game wants non-blocking gets `MSG_DONTWAIT` on each receive/send (`XSocket::HostFlags`;
  `ps5_test_refuse_nonblocking=1|2` reproduces the console on Linux). But on Linux even a blocking socket (=2) reaches
  the intro, so this is not the cause. Found: on the console no story level is ever opened (no `SP_HospitalIntro_P`
  open, no failed open; Linux opens it right after the session socket). The game stops before travelling to the level.
  0.1.5 logs every socket call the host refuses (`XSocket: <call> ... refused by the host (errno N)`) and the test
  settings turn on `cogbox_net_trace`, to find the step. Linux route for a NEW campaign (the saves must go first:
  `ab.sh` deletes `UserData/.../00000001/Gears2Checkpoint` and its header before each run): `12:start 18:a 24:a 28:a
  32:a` = Solo Campaign, first empty slot, difficulty; the film plays (Back skips it), then the in-engine "Tip of the
  Spear" scene and "Choose path".
- **Fixed in 0.1.4, the teal cast** (green/teal patches on textures and smoke, the whole hospital tinted; Training's
  yellow sign and road lines teal): `execute_unclipped_draw_vs_on_cpu` was false in the SDK (Xenia's default is true),
  so full-screen clears claimed the whole EDRAM and host render targets copied unrelated ranges into each other. Now
  true by default (santiquiroz/gears-of-war-2-pc-port patch 0033); the picture matches the FSI path. COGBox 0.6.7's SDK
  path has the same bug, so it is **not** a colour reference (its own renderer may be); FSI is.
- **Fixed in 0.1.2, the white flashes** (2026-11-23): newly streamed surfaces (walls, floors, glass) went pure white
  with bloom for ~0.3-1 s. The SDK's Vulkan shader translator took a texture's `exp_adjust` from fetch-constant word 4
  bits 13-18 instead of word 3: word 4 holds the LOD bias, which UE3 steps from ~+3.8 to 0 while it fades a new texture
  in, so each fetch was multiplied by 2^(a piece of the bias), up to 2^31. D3D12 reads word 3. Fix from
  santiquiroz/gears-of-war-2-pc-port (`sdk/patches/0019`, BSD-3-Clause), with its Vulkan resolve write-barrier fix
  (`0020`). Same route before/after: 13-41 white frames to 0. That port (a Gears 2 recompilation with an Android
  Vulkan build, 60+ SDK patches) is worth checking first for any other Vulkan-only bug.
- **The pause menu (checked before the 0.1.4 teal fix; recheck its colours):** the light band is the live scene seen through the menu's torn mask; COGBox
  0.6.7 on this PC (under Proton, both its SDK path and its own renderer) draws it the same (mean difference under
  1/255). COGBox on this PC: `~/cogbox-pc` (release 0.6.7, disc extracted to `game/extracted`, `run.sh <name> <s>
  [args]`; `--input_backend=none` = pause menu, `--gow2_input_script="30:A:0.3, 40:LUP:6, ..."`,
  `--gow2_frame_shots=4 --gow2_frame_dir=...` saves frames). Its SDK path is D3D12 (no way to pick Vulkan there).
- **Console runs (0.1.2):** the package carries `gow2.toml` from `ps5/package/gow2.test.toml` (DS2's ds2.toml pattern):
  frame counter, `ps5_perf_log`, `ps5_capture = 4` (logs/capture_<n>.bmp, last 100) and `ps5_mark_button`: L3+R3
  saves that picture as `logs/mark_<n>_f<frame>.bmp`, writes a `ps5 mark <n>` warning line and dumps the next frame's
  draws (`rex::ps5diag::RequestFrameDump`). `GOW2_TEST_SETTINGS=0` packages without it for a release. Ask the tester
  for the whole logs/ folder and to press L3+R3 at anything wrong.
- Test tools added (SDK patch): `ps5_dump_frame`/`ps5_dump_frames` (+ texture loads, fetch constants, guest bytes,
  pixel c0-c7/c255, `ps5_dump_vertex_constants`), `ps5_skip_pixel_shader` (comma-separated hash prefixes),
  `ps5_log_fetch_changes` (every change of a texture's fetch constant, with the frame), `ps5_log_texture_loads`, a
  warning line for any texture load that fails, app `ps5_capture_ms` (pictures named by frame);
  `dump_shaders=<dir>` gives the shaders' microcode disassembly.
- Build machine: Fedora, clang 23, AMD Radeon 760M on RADV (the PS5's driver), 14 GB RAM (`-j5`).
  Linux test build: `ps5/tools/linux_test.sh build|run` (~3.5 min full build). PS5 toolchain at
  `~/Desktop/ps5-plume-triangle` (d54d9adb).

## The two reference ports

| Repo | What it is | Read it for |
|---|---|---|
| `chonzytron/gta4-ps5` (fork of `elripalda/gta4-ps5`) | GTA IV. ReXGlue 0.9 fork, its own native renderer (`gta4-native`), a hub app, installer payload, FSR 3, trophies. Runs on consoles. | The platform layer as first proven on hardware, a native renderer, frame pacing at 120 Hz, memory limits, the hub, packaging (`pic0.dds`) |
| `chonzytron/Dead-Space-2-PS5`, branch `ccr-9a96951b-5a0smb` (release 1.0) | Dead Space 2. The GTA IV layer carried to upstream ReXGlue 0.10.0 as one patch, using the SDK's **Xenos emulation** (no game-specific renderer). **Ships: native 4K at 60 fps in play on a PS5.** | Porting to upstream ReXGlue; making Xenos emulation fast enough (the console-measured optimisation log); the install/first-start flow; a Linux build of the PS5 program for testing; diagnostics |

Where each started (credit them, and read them for the PC side):

| Repo | Role |
|---|---|
| `bifftannen1/Dead-Space-2-Recomp` | The Dead Space 2 recompilation (TU2, 47,739 functions named and laid out from the game's PDB) the DS2 port is built from |
| `OZORDI/LibertyRecomp`, `vaduur/LibertyRecompAndroid` | The GTA IV recompilation and the fork the GTA IV port starts from |

Upstream and toolchain:

| Repo | Pin used | Role |
|---|---|---|
| `rexglue/rexglue-sdk` | `c94f5ebd` (v0.10.0) for DS2 | Xbox 360 recompilation runtime (derived from Xenia). Already has a Switch (`REX_PLATFORM_NX`) layer and 16 KB host-page support (written for Apple Silicon): both are templates for the PS5. |
| `Nazky/ps5-plume-triangle` | `d54d9adb` | Builds the whole PS5 toolchain with `tools/build.sh`, and provides the link recipe (`tools/radv-link.sh`, `radv_link_recipe`), app CRT (`build/obj/app_crt.o`, `app_cpp_runtime.o`), libSceAgc stubs, the `ps5-native-tool` signer and `runtime/libc.prx` |
| `mihawk-99/PS5_PayloadSDK` | `95c08f27` (pinned by plume-triangle) | clang wrappers, FreeBSD/PS5 headers, libc++ 18, kernel stubs (`sce_stubs/*.c` = the console's exported symbols), `libps5platform` (`ps5platform/shm.h`: `ps5_shm_*`, `ps5_vrange_reserve`) |
| `mihawk-99/PS5_Mesa` | `0b2d6d1a` (pinned by plume-triangle) | RADV for the PS5 as a static `libvulkan_radeon.ps5.a`. Reports Vulkan 1.4, runs geometry shaders as compute, presents through `VK_KHR_display`. |
| `ps5-payload-dev/pacbrew-repo` | v0.40.2 | Prebuilt PS5 libraries (plume-triangle fetches them) |
| `xenia-project/xenia` | | What ReXGlue's kernel, GPU (Xenos) and file system come from: the place to read how the emulated GPU works |

## Files to open first for context

GTA IV (`gta4-ps5`):
- `ps5/BUILD.md`: the build command, the on-console layout, where things are
- `patches/liberty-ps5.patch`: every SDK change, each marked `[ps5 port]` with the reason. Grep it for `[ps5 port]`.
- `ps5/CMakeLists.txt`, `ps5/cmake/ps5-toolchain.cmake`, `ps5/tools/ps5_package.sh` (param.json, `attribute3` for 120 Hz)
- `glue/rexglue-sdk-main/gta4-recomp/src/ps5/gta4_ps5_app.cpp`: the app class (start-up, paths, settings)
- `glue/rexglue-sdk-main/src/graphics/gta4_native/`: the native renderer (GTA IV-specific, but the model for one)
- `glue/rexglue-sdk-main/patches/ffmpeg-ps5.patch`
- `ps5/payload/installer/`: the fast-install payload (port 9021)
- `ps5/package/sce_sys/`: `icon0.png`, `pic0.dds`, `pic1.dds` (the home screen background format)

Dead Space 2 (`Dead-Space-2-PS5`, branch above). **Start here when the target game uses
upstream ReXGlue and the SDK's Xenos emulation.**
- `ps5/NATIVE_RENDERER.md`: **the optimisation log**. Each console run (ds2_014 to ds2_031),
  what limited the frame, what was changed, and the before/after numbers. Read it before
  optimising anything.
- `ps5/BUILD.md`: steps, the on-console layout, the Linux test build, where things are
- `sdk/rexglue_ps5.patch`: the platform layer **and every renderer/command-processor
  optimisation** for ReXGlue 0.10.0, as one patch (85 files). Hot spots:
  - `include/rex/ps5_diag.h`, `src/core/ps5_diag.cpp`: counters, slow-frame lines,
    per-packet timing, the SIGPROF profiler, crash reports
  - `src/graphics/vulkan/command_processor.cpp` (+ `.h`): the submission thread, texture
    binding reuse, GPU phase timing
  - `src/graphics/vulkan/texture_cache.cpp`, `render_target_cache.cpp`,
    `src/graphics/shaders/ps5/resolve_forward_*`: resolve forwarding (colour and depth, one pass)
  - `src/graphics/util/draw_extent_estimator.cpp`: draw extent from the viewport
  - `src/graphics/shared_memory.cpp` (+ `.h`): volatile pages, upload accounting
  - `src/graphics/command_processor.cpp`: idle and WAIT_REG_MEM timing, per-packet timing
  - `src/ui/windowed_app_main_sdl.cpp`: the 10 s `ps5 diag` / `ps5 cp` lines, the hang monitor
- `ps5/CMakeLists.txt`: the SDK built in-tree, the recompiled code as an OBJECT library, CMake's own link skipped
- `ps5/src/ds2_ps5_app.h`: `ReXApp` subclass: setting defaults (`DefaultFlag`), paths, the
  preparing / missing-files / cleanup screens, the FPS counter, the `ps5 settings:` log line
- `ps5/src/ds2_ps5_install.cpp`: finds the player's ISOs/title update/DLC, identifies them by
  XEX execution info, checks SHA-256 pins, stages `default.xex` + `default.xexp`, keeps a
  record so later starts skip it, offers to delete what is no longer needed
- `ps5/src/ds2_ps5_framerate.cpp`: 60 fps from a 30 fps game (two hooks), 30 in cutscenes
- `ps5/src/ds2_ps5_gpu_wait.cpp`: the game's spinning GPU wait made gentle (a big win; see below)
- `ps5/src/ds2_ps5_lowres.cpp`: a game-specific EDRAM fix (MSAA aliasing kept at 1x)
- `ps5/src/ds2_ps5_intro.cpp`: skipping the logo videos without breaking the menus
- `ps5/src/ds2_ps5_d3d_trace.cpp`, `ps5/tools/d3d_frame_summary.py`: record the game's Direct3D calls, turn a frame into its passes
- `ps5/src/ds2_ps5_capture.h`, `ds2_ps5_script.cpp`: screenshots and scripted button presses for A/B tests
- `ps5/src/ds2_ps5_linux.cpp`, `ps5/tools/linux_test.sh`: the PS5 program built and run on Linux
- `ps5/src/ds2_ps5_pch.h`: overriding the generated code's `REX_PHYS_HOST_OFFSET` without touching the generated tree
- `ps5/package/ds2.example.toml`: every user setting, documented for players
- `ps5/tools/`: `fetch_sdk.sh`, `build_ps5.sh`, `ps5_package.sh`, `ps5_syntax_check.sh`,
  `ps5_symbol_check.sh`, `ps5_symbolize.py`, `ps5_emutls.c`, `ps5_stat.c`, `ps5_relro_pad.c`,
  `ps5_skip_link.sh`, `make_background_dds.py`
- `.claude/settings.json`: the permission rule that lets a cloud session run the toolchain build

In the new game's repo: its PC app class (`src/app/*_app.h`), its CMake recipe for the generated code, the generated PCH (`*_pch.h`, which has the memory-access macros), its symbol/name map (hooks are by name), any PC mods worth carrying over (DS2's `framerate` mod became a setting), and anything the PC launcher passes on the command line (those become setting defaults on the PS5).

## How a port fits together

```
recompiled PPC → C++ (generated, as checked in)  ─┐
ReXGlue SDK + game patch + rexglue_ps5.patch      ├─ one static PIE ─ RADV + app CRT ─ signed eboot.bin
app: ReXApp subclass, installer, PS5 glue          ─┘
```

- No shared libraries: the runtime and the GPU backend become static libraries, linked with
  `--whole-archive`. Their kernel exports and settings register themselves from static
  initialisers, and the linker would otherwise drop them without any error. The GPU backend
  is created directly (`rex_gpu_create`) instead of loaded as a plugin.
- CMake compiles. `ps5_package.sh` links: it reads CMake's link line
  (`ninja -t commands <target> | tail -1`), uses each input **once** (CMake repeats static
  libraries, and taking one whole twice duplicates every symbol), and runs plume-triangle's
  recipe. Then it signs with `ps5-native-tool` and writes `sce_sys/param.json`.
- The app folder goes to `/data/homebrew/<TITLE ID>/`, which the app sees as `/app0`.
  The player's files go in `install/`; the app checks them and stages them on first start.
  The package contains no game data.
- **Hooking the game's own functions:** the recompiled code defines each function as a weak
  alias of `__imp__<name>`. Define `<name>` in the app; it wins at link time and calls
  `__imp__<name>` to run the original. With PDB names this is how DS2 changes the frame rate,
  skips videos, counts Direct3D calls and fixes the GPU wait, without touching generated code.

## Workflow for a new game

1. **The PC build first.** The recompiled code must already run correctly on the PC with the
   same SDK commit. The PS5 adds platform risk; it can't be where game bugs get found.
2. **The SDK.** If the game uses upstream ReXGlue, apply DS2's `sdk/rexglue_ps5.patch` on top
   of the game's own patch and fix the conflicts. If it uses a fork, carry the `[ps5 port]`
   hunks over one file at a time. Keep a clean SDK work tree with the base and game patch
   committed (DS2 tags it `ds2-patch-base`), make the PS5 changes there, and regenerate the
   patch from it (DS2: `git add -N . && git -c core.abbrev=8 diff ds2-patch-base >
   ../../sdk/rexglue_ps5.patch`; `--binary` if it holds binaries). Never edit a fetched copy
   without regenerating the patch: the next fetch throws the change away.
3. **The app.** Copy DS2's `ps5/` folder and rename. Port the PC app's defaults (launcher
   flags, language) into `OnPostInitLogging` with `DefaultFlag` (only when the player's
   ds2.toml doesn't set it). Write the installer for this game's files: XEX execution-info
   title id and disc number, SHA-256 pins for every XEX/XEXP the code was recompiled from,
   and the title update and DLC formats (STFS content types).
4. **Generated code stays byte-identical** to the verified PC tree. Platform differences go in
   a PCH that includes the generated one and then redefines macros
   (see `ds2_ps5_pch.h`). Game behaviour changes go in hooks (above).
5. **Check without a console** (below): syntax, then object code, then symbols, then a full
   build with a stand-in toolchain.
6. **The Linux build of the PS5 program** (below): boot it, play it, and keep it working.
   Every later change is tried there first.
7. **Real toolchain build, link and package**, then run it on a console. Expect the first runs
   to need fixes. Read the log in the app folder after each run.
8. **Profile on the console before optimising anything**, then iterate (see the next two sections).

## Working with a console in the loop

The console isn't reachable from the build machine: each test is a build handed to a person,
who copies it over, plays, and copies `logs/` back. Make every run count.

- **One build, one ready `ds2.toml` per run.** Give the tester the exact settings file. New
  behaviour goes behind a setting, **off by default** until a console run confirms it, then
  becomes the default. The release then needs no settings file at all.
- **Identify the program in the log.** The first line names the build's commit, and a
  `ps5 settings:` line prints the values that matter, with "(not in this program)" for a
  setting the program doesn't know. DS2 lost a run (ds2_017) to an old eboot with a new
  ds2.toml before this existed.
- **Copy the package over the old folder, minus `install/`** (DS2: `unzip -o ... -x
  'PPSA94108/install/*'`). The player's game files, saves and caches stay. Copying over never
  deletes, so tell the tester about files a release dropped.
- **Ask the tester where it happened** ("the action scene in the hacking room", "the elevator
  cutscene") and line that up with the log's timestamps and the screenshots (`ps5_capture`).
  A 10 s average hides the drop; the on-screen counter averages half a second.
- **Separate pauses and loading** from play when comparing runs: count draws per frame (a
  pause menu has a steady, low draw count) and look only at windows of real play.
- **Compare like with like:** the same stretch, the same settings, windows with a similar
  draw count. Report the worst windows and slow-frame counts, not just averages.
- **Linux first.** A change that fails on the Linux build never goes to the console.

## The Linux build of the PS5 program

DS2 builds the same app and patched SDK for Linux (`ps5/tools/linux_test.sh build|run`):
`-march=znver2`, the runtime and GPU backend linked in as on the PS5, the same log lines,
diagnostics and profiler. Only the console-only parts differ (window, SDL input/audio;
`ds2_ps5_linux.cpp` stubs `scePad`/`sceUserService`). On an AMD GPU it uses RADV, the PS5's
driver.

- Most bugs and every renderer change were caught or checked there first.
- **A/B checks of pictures:** `ps5_input_script` presses buttons at set times (through the
  menus into a save, a few steps forward), `ps5_capture` saves screenshots with frame numbers.
  Run both settings, compare the pictures.
- **A check mode for risky GPU changes:** DS2's `ps5_resolve_forward_check` reads back a
  forwarded picture and the normally-loaded one every 5 s and counts differing pixels (0 of
  3,686,400 every time). Build one in for anything that replaces an emulated path.
- Its numbers are not the console's: the PC's CPU is much faster, so CPU costs shrink and
  GPU costs dominate. Measured on the PC, the swap replay was 0.8 ms a frame; on the console
  7 ms. Use the PC for correctness and relative cost, the console for what to fix.
- Watch for a leftover `/dev/shm/xenia_memory_*` after a killed run (next run SIGBUS); the
  script cleans it. A first-start screen waiting for input stalls automated runs: give the
  bench a ds2.toml that answers it.

## PS5 platform facts the layer handles

Memory:
- 16 KB pages and 64 KB direct-memory units. The guest's 0xE0000000 physical view needs a
  `+0x1000` host offset (`REX_PHYS_HOST_OFFSET`). The SDK's `PhysicalHostOffset` and the
  generated code's macro **must agree**, or kernel and game see different bytes.
- Guest memory lives in direct memory with mirrored fixed views (`Memory::MapViewsPs5`).
  Reserve the address space with `ps5_vrange_reserve` near 64 GiB (`0x1000000000`).
  Shared memory comes from a small flexible pool, so don't put the guest there. Use
  `shm_open(SHM_ANON)` for anything else that needs it.
- No `MAP_FIXED_NOREPLACE`: inside the guest arena, change protection only and never `mmap`.
  No `/proc`, so `QueryProtect` has to answer from the page table.
- **A protection change is slow** (about 0.1 ms, with the global lock held). Xenos emulation
  write-protects GPU-visible pages and re-protects them after each write fault; for pages the
  game rewrites every frame that dominated the command processor. See "volatile pages" below.
- Files can't be memory-mapped: read them with `pread`. Use O_DIRECT for disc images
  (`direct_io.h`), and `Ps5FileCopy` instead of mapped copies.
- `statvfs` always answers 16 GiB, so a free-space check is meaningless.
- **Each kernel allocation counts.** GTA IV stopped getting memory (even 16 KB) at around
  10,000 texture allocations while the driver still showed half its budget free. It now
  treats more than 6,000 images as memory pressure.

Threads, signals, time:
- `ucontext` doesn't switch stacks: fibers use a hand-written `rex_fiber_switch`. Thread
  stacks come from direct memory.
- No realtime signals: suspend/resume use SIGUSR1/2. The signal context's mcontext is at
  offset +64 (`Ps5MContext`). Bad guest accesses arrive as **SIGBUS** as well as SIGSEGV.
- No thread affinity, priority or robust mutexes. Name threads with `pthread_set_name_np`.
  **The system places every thread, and each core runs two (SMT):** a thread that spins flat
  out slows whatever shares its core.
- The clock is the TSC (`sceKernelReadTsc`). Its tick counts overflow 64-bit
  multiply-divide, so use 128-bit math, including in the `clock_no_scaling` path.
  `steady_clock` is a system call: read the TSC for timing.
- `std::recursive_mutex` is slow: the global critical region is a spin lock
  (`rex::thread::global_lock_t`; DS2 replaced 27 sites).
- Thread wake-ups are slow: spin briefly on the signal count before sleeping
  (GTA IV's `ps5_wait_spin_us`).
- **`thread_local` costs a call into the system on each access** (emulated TLS,
  `-femulated-tls`; 8-17% of DS2's command processor in one profile). Keep it out of hot paths.
- `std::thread` works (DS2's submission thread and profiler use it).

Toolchain and C library:
- Target `x86_64-sie-ps5`, `-march=znver2 -femulated-tls -fno-stack-protector -fno-plt
  -fdenormal-fp-math=ieee -fvisibility-nodllstorageclass=default`. The target has
  **exceptions and RTTI off by default**: add `-frtti -fexceptions`. libc++ 18 needs
  `-fexperimental-library` (jthread/stop_token). Use the default (small) code model.
  Define `FMT_USE_CONSTEVAL=0`.
- Force-include a compat header (`rex/ps5_compat.h`) that maps Linux names to FreeBSD ones.
  Platform macros: `REX_PLATFORM_PS5` **and** `REX_PLATFORM_LINUX` (to share the POSIX paths).
- FreeBSD ships its own `<sha256.h>`, which shadows a library's: include `"crypto/sha256.h"`.
- Missing from the console's libraries although headers declare them: `fwrite_unlocked`
  (set spdlog's `SPDLOG_FWRITE_UNLOCKED` OFF), `wcslcat`/`wcslcpy`/`wcsnlen` (used by SDL),
  `__cxa_thread_atexit` (provided by `ps5_emutls.c`). `stat`/`lstat` are wrapped
  (`ps5_stat.c`, `--wrap=stat --wrap=lstat`).
- If signing fails with "no room for PS5 process parameters", relink with `ps5_relro_pad.c`.
  The package script already retries this automatically.
- A strong symbol in the app against an SDK stub (for example `__imp__XamSwapDisc`): make the
  SDK's weak on the PS5. A DLL build never had this clash.
- Strings in the signed `eboot.bin` are searchable (`LC_ALL=C grep -a`): a quick check that a
  build has a change. A rebuilt eboot can keep exactly the same size (segment padding);
  compare bytes, not sizes.

Graphics, input, audio, system:
- Vulkan: RADV linked statically, entry point `vk_icdGetInstanceProcAddr`. Some functions
  are only found under their KHR names, so fall back to the alias. The surface is
  `VK_KHR_display` (the first display that reports modes).
- **The PS5's RADV reads the system clock for every query `vkGetQueryPoolResults` returns**
  (12% of a thread with one timestamp per draw). Copy query results into a buffer with
  `vkCmdCopyQueryPoolResults` and read that.
- SDL is used with the dummy video and audio drivers only. Turn off X11, Wayland, ALSA,
  HIDAPI, and so on.
- Input through `scePad` (DualSense), audio through `sceAudioOut`. SDL has no PS5 backend
  for either. Tolerate `scePadInit` failing (already initialised).
- Message boxes become PS5 notifications (`sceKernelSendNotificationRequest`). The language
  comes from `sceSystemServiceParamGetInt(1, …)`.
- Paths: the executable is `/app0/eboot.bin` and user data is under `/app0/…`.

Package (`sce_sys/`):
- `param.json` (written by `ps5_package.sh`), `icon0.png` 512x512.
- **The home screen background is `pic0.dds` and `pic1.dds`: 3840x2160, BC7_UNORM, one mip,
  DX10 header.** PNG backgrounds are not picked up. DS2's `ps5/tools/make_background_dds.py`
  scales a picture to cover 16:9 without stretching, crops at a chosen anchor, and writes
  that format (header byte-identical to GTA IV's). Needs `pip install etcpak
  texture2ddecoder pillow` (use a throwaway venv).

## Performance: what mattered on the console

**Measure first, every time.** Both ports' wins came from console profiles and per-frame
logs, not from guesses. Several plausible ideas measured as nothing.

### Diagnostics worth building in (DS2's `ps5_diag`)

- **A 10 s line**: frames presented and the game's own frames (its frame lock), GPU busy %,
  time waited on the GPU, faults, and **slow frames** (over 20 ms and over 40 ms, the longest).
  Averages hide drops; slow-frame counts don't.
- **A line per slow frame** (the first 200, then 1 in 20): how long the command processor
  worked, waited for the game's commands (between them, and inside them for a vblank or a
  value), waited for the GPU; its draws and their parts (guest memory, textures, pipelines,
  bindings, primitives, render targets, state), resolves, pipelines made, bytes uploaded.
  This is what tells a CPU-bound frame from a GPU-bound one from a game-bound one.
- **Per-packet timing by PM4 kind** (DRAW_INDX, XE_SWAP, registers, WAIT_REG_MEM, ...):
  found that the swap packet was a third of the slow frames.
- **GPU phases** from timestamps (draws, EDRAM transfers, resolves, texture loads, uploads,
  the top shaders by time), and a resolve catalogue.
- **A SIGPROF sampler** over registered threads (100 Hz, report every 30 s), with addresses
  relative to an anchor and `ps5_symbolize.py` to name them. Leave out samples of threads
  that were mostly waiting: their stacks land on stale return addresses (a function's first
  address is never a real caller). It costs about 6% of the command processor.
- A **hang monitor** (no frame for 30 s: every thread's stack) and a crash line.
- **One switch for the detailed lines** (`ps5_perf_log`) so a release log stays quiet.
- Read the 99%-busy thread in every profile carefully: in DS2 it was the game spinning, not
  working.

### Frame rate and pacing

- A game locked to 30 can often run at 60 through two hooks: its target frame time
  (DS2: `TimeManager::HandleEvents`, target from 2 vblanks to 1) and its frame lock
  (`Displ_WaitForFrameLock`, capped at 1 vblank). The PC build's frame-rate mod shows which.
- **Check the game clock keeps real time** at the new rate (DS2's did, with remainder carry),
  then look for what is **counted in frames**: DS2's cutscene events came early at 60.
  DS2 runs cutscenes at the game's own 30 (detected from the player's scripted-sequence
  flag) and keeps **played** sequences at 60: the hacking minigame (from its own events)
  and any sequence where the controller does something new. Make sure no gameplay is held
  to 30 by accident.
- **Adaptive vsync:** a frame a little late is shown at once instead of waiting a refresh.
  Without it a 17 ms frame shows after 33.
- "Slower above 60" reports: check whether the counter or the game's clock is wrong before
  changing pacing (in DS2 the clock was right; the drops were frame-counted parts).
- Keep cutscenes and loading screens at their original 30 where the game needs it.
- GTA IV: frame pacing at 120 Hz output with a 60 fps limit needs care in the presenter
  (`attribute3` in param.json declares 120 Hz).

### Xenos-emulation ports (DS2): from 20 fps to a locked 60 at native 4K

In order of what they bought, all in `sdk/rexglue_ps5.patch` or `ps5/src/`, each measured on
the console (details and numbers in DS2's `ps5/NATIVE_RENDERER.md`):

1. **EDRAM transfers are the first thing to look at.** A heavy room at 4K had 44 ms of GPU
   work, 25.8 ms of it copies between host render targets: the game switched its depth buffer
   between 1x and 4x MSAA over the same EDRAM ~25 times each way a frame (free on the 360, a
   full copy here). Find the game code that does it (DS2: the deferred lighting's stencil
   culling, `Displ_StartLowRes_XenonSpecific`; and Direct3D's `ClearRect`) and keep it at 1x
   (`ps5_lowres_culling`, `ps5_lowres_clears`): 103 transfers a frame to 12, 21 fps to 41.
   The fragment-shader-interlock path (no transfers) drew three times slower.
2. **Resolve forwarding** (`ps5_resolve_forward = 2`): a whole-picture resolve also goes into
   a host image; the texture that reads that picture is filled by an image copy instead of
   untiling guest memory, and after 30 such reads the resolve stops writing guest memory.
   Then **one pass**: colour blits straight into the texture (`MUTABLE_FORMAT`, BGRA view
   swaps red/blue), depth through a compute shader writing an R32_SFLOAT storage view,
   quantised exactly as the EDRAM dump and k_24_8 load do. Byte-identical by the check mode.
3. **Draw extent from the viewport** (`ps5_draw_extent_from_viewport`): full-screen quads
   with no scissor and a vertex setup the CPU can't run were taken to reach 2048 rows, so the
   depth buffer claimed the colour target's EDRAM. Bounding them by the viewport removed the
   last regular transfers. With 2 and 3: GPU work 21.6 ms to 13.5 ms a frame.
4. **`clear_memory_page_state = false`** (the SDK default was true): uploads 13-17 MB a frame
   to 1.6-3 MB in action scenes. The game's writes are still caught by write watches.
5. **Volatile pages:** a page that keeps write-faulting stops being protected and is hashed
   (XXH3) when a draw asks for it; re-protected after 128 unchanged looks. Avoids the slow
   protection round trip. (Hashing it only once per command batch measured as no gain.)
6. **The submission thread** (`ps5_submit_thread`, default on): Xenia records each draw in a
   deferred command buffer and replays it into a Vulkan command buffer at submission. On the
   PS5 that replay is ~2 µs a draw, 6-10 ms at the swap in a 3,000-draw frame. A second thread
   now replays and submits, and big frames are cut into submissions of 300 draws so it
   overlaps (150 cost more than it saved). The command processor decides everything about a
   submission (command buffer, fence, semaphores, timestamps) and only waits on a fence once
   the thread has submitted it; the swap's own submission stays synchronous because the
   presenter shows the image right after. Swap time 6-10 ms to 0.2-0.6 ms.
7. **Gentle GPU waits** (`ps5_gentle_gpu_waits`, default on): Direct3D's `CBlocker::Check`
   polls a fence flat out and used a whole core all the time, even at 60. `_mm_pause` x16
   before each check and a yield every 16th: in the busiest stretches slow frames 124 to 11
   and the command processor's draws 8.7 to 7.5 ms a frame (its core is shared no more). Look
   for the same loop in any XDK game (`D3D::CDevice::BlockOnFence`).
8. **Command-processor trims**, each a few percent: reuse texture descriptor sets when a draw
   has the last draw's textures (`ps5_reuse_texture_bindings`, bindings 1.6 to 1.3 ms); look
   up register info per register write only when debug logging is on (register packets 1.7
   to 1.0 ms); keep one vector in `RequestRanges` instead of allocating per request; don't
   read the clock in a per-draw path for a log line (it cost more than the hashing it sat
   next to); profiler timestamps via `vkCmdCopyQueryPoolResults`.
9. Ruled out by measurement: the WAIT_REG_MEM waits (0 in slow frames), the profiler as the
   cause, the game clock. What is left in DS2: rare ~240 ms hitches at area changes, inside
   the game's own threads (likely streaming).

General rules from both ports:
- Avoid `thread_local` on hot paths (above). Use fixed tables with atomics.
- Don't look up settings by name while frames are built: the cvar registry locks and copies
  strings. Cache the values (GTA IV re-reads at most every 250 ms per call site; DS2 reads
  them once into a `static const`).
- Spin briefly before sleeping on hand-offs between threads; but never spin without
  `_mm_pause` (SMT).
- Move work off the critical thread: GTA IV captures commands on a core of its own through a
  lock-free byte ring; DS2 moved the Vulkan replay off the command processor.
- Cache per-draw lookups, since consecutive draws use the same targets and textures.
- Evict unused textures early (40 s) and count the number of images, not only bytes, as
  memory pressure.
- Run expensive passes every other frame where it can't be seen (GTA IV's reflections).
- Seed a shader/pipeline cache in the package (`ps5/package/shader_cache_seed`; DS2 sets
  `p4_vk_pipeline_cache_path`). Compiling pipelines on the console causes stutter.
- Use exact floating point (`-ffp-model=strict`, `-fno-strict-aliasing`), as the PC builds
  do. x86 FMA contraction changes results: GTA IV fixed fused multiply-subtract cases one by one.
- ThinLTO across the whole program is an option (DS2: `-DDS2_PS5_THINLTO=ON`). Get a plain
  build working first.
- FSR 3 (FidelityFX) runs on the PS5's RADV. Building it needs Wine for AMD's shader
  compiler, plus spirv-tools and glslang.
- Xenos-emulation defaults (DS2): sparse shared memory off, `readback_resolve=none`,
  `clock_no_scaling=true`, `ps5_display_height=2160`, `resolution_scale=3` (native 4K).
- **When emulation can't get there**, a native renderer for the game's Direct3D (GTA IV's
  `gta4-native`) is the next step. In DS2 about 12 of 21 ms of GPU work was EDRAM emulation
  before the fixes above; record the game's Direct3D calls first (`ds2_ps5_d3d_trace.cpp`)
  to see what it really does.

## The player's side: install, first start, release

- **First start** (DS2): a screen checks the files in `install/` against the pins and
  prepares the game (stages XEX/XEXP, unpacks DLC), says how far it is, and resumes if
  closed. A record (`game/prepared.txt`) lets later starts go straight to the game while the
  install folder holds the same files. At the end it offers to delete what is no longer
  needed (title update and DLC packages; never the disc images the game reads from):
  Cross deletes, Circle keeps (`ps5_install_cleanup = "ask" | "keep" | "delete"`).
- **Disc swapping** is automatic when both disc images are there (the PC's disc-swap code,
  compiled with the PS5 PCH).
- **Skipping logo videos** (`ps5_skip_intro_videos`): finish the movie at the movie-player
  layer (`IsFinished` returns true) rather than ending it from outside or faking a button
  press, which left the title screen deaf or misplaced. Then check every screen after it:
  DS2's title overlay (`FE66`) had to stay hidden after the prompt.
- **Release:** a version file, a short README (install, first start, controls, a settings
  table, what the port adds, credits with the original recompilation first, honest test
  status), the background, defaults that need no settings file, detailed logs and
  screenshots switchable off (`ps5_perf_log`, `ps5_capture = 0`). Zip the app folder only;
  check the zip holds no ISO, XEX/XEXP, title update, DLC, saves or logs before handing it on.

## Checks that need no console (DS2's tools)

```
ps5/tools/fetch_sdk.sh
ps5/tools/ps5_syntax_check.sh <ps5 headers> [jobs]                       # every SDK + app source, syntax only
PS5_CHECK_OBJ_DIR=<objs> ps5/tools/ps5_syntax_check.sh <ps5 headers>      # to PS5 object code
ps5/tools/ps5_symbol_check.sh <objs> <PS5_PayloadSDK source tree>         # duplicates, missing, libc coverage
```

- `<ps5 headers>` can be assembled from the payload SDK's **source tree** (no build needed):
  `include/freebsd`, `include/khronos`, `include/ps5`, `platform/include/ps5platform`,
  copied under `target/include`. Pair them with the host's clang 18 and libc++ 18 headers.
- A whole-build dry run uses a stand-in toolchain file: host clang with
  `CMAKE_*_COMPILER_TARGET=x86_64-sie-ps5`, `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`,
  the sysroot from those headers, and no linker. Then run `ninja <target>` and a symbol
  check over every library on the link line.
- These checks found real link errors before any console run: SHA-256 compiled into both the
  runtime and the app, the runtime library repeated on the link line, and spdlog using
  `fwrite_unlocked`.
- They prove the code compiles for the PS5 and that its parts fit together. They do not
  prove that it links with the real toolchain or that it runs. Say so plainly when
  reporting.

## On the console: logs and crashes

- The log goes in the app folder (flushed every 2 s), with a crash hook and a diagnostics
  monitor (`ps5_diag.h`: counters, thread samples, the guest arena range, the time of the
  crash).
- Crash and stack lines print addresses relative to an anchor (`Anchor()` = `&NowMs`).
  Run `ps5_symbolize.py` against the unstripped `ps5-link/build/llvm-pie.elf` **of the same
  build** to turn them into functions (keep each build's ELF if old profiles may need
  reading). Compile the recompiled code with `-gline-tables-only` so guest functions
  resolve. The Linux build's binary can be symbolized the same way.

## Working in Claude Code cloud sessions

- The auto-mode classifier refuses to run the toolchain's build scripts (plume-triangle's
  `tools/build.sh` and what it clones) as "code from outside". Don't try to work around it.
  Commit a narrow allow rule for one in-repo entry script instead:

  ```json
  { "permissions": { "allow": ["Bash(ps5/tools/build_ps5.sh)", "Bash(ps5/tools/build_ps5.sh *)"] } }
  ```

  Have that script pin plume-triangle to a commit. The rule is read only by a session that
  has this repository alone, started from a branch that contains the file.
- The toolchain build downloads from github.com and zlib.net, and the environment's
  network policy must allow both.
- Run long builds in the background and write logs to files. The full DS2 build is 852 steps;
  after it, an incremental `build_ps5.sh compile package zip` takes about a minute.

## Rules

- Never commit or package game data (XEX, ISOs, assets). Check the player's own files against
  SHA-256 pins and stage them on the console. Never copy the player's files into the repo,
  and never upload them.
- The PS5 layer derives from GPL-3.0 code (the GTA IV port, `libps5platform`), so PS5 files are
  GPL-3.0. ReXGlue keeps its own licence. Credit the game's recompilation project first, then
  elripalda, mihawk-99, Nazky, ps5-payload-dev and Xenia.
- State test status honestly in the README: "compiles", "links", "boots" and "playable" are
  different claims. So are "60 in play" and "60 everywhere".
- New behaviour ships off by default until a console run confirms it.
