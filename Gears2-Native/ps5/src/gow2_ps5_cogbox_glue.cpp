// Gears of War 2 for PS5 - the parts of COGBox's layer 0 that the game layer
// calls but this program doesn't have.
//
// The PS5 build runs the SDK's Xenos GPU emulation (COGBox's --gow2_gpu=sdk),
// so COGBox's own renderer (renderer/shadow_renderer.cpp, gpu_system.cpp), its
// in-game menu (ingame_menu.cpp), crash report (crash_report.cpp; the PS5 layer
// has its own crash line) and window icon (window_icon.cpp) are not built.
// The game layer's hooks (layer3_gow2/game_patches.cpp and friends, compiled as
// they are) ask them a few questions; these are the answers COGBox's own code
// gives when its renderer and menu are not running.
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>

#include <xenos/dev_tools.h>  // XenosPrebuildShaderRef

namespace rex {
class Runtime;
}
struct ImFontAtlas;

// crash_report.cpp's folder setting: frame_log.cpp writes its CSV next to it.
REXCVAR_DEFINE_STRING(gow2_crash_dir, "", "Gears2", "Folder for frame-log files (COGBox's crash folder setting)");

// gpu_system.cpp: our graphics system is not in place of the SDK's.
bool XenosOurGpuSystem() { return false; }

// renderer/shadow_renderer.cpp: no renderer of ours to tell about the game's
// shaders, to poll, or to wait for.
void XenosGuestShaderCreated(bool, uint32_t, uint32_t) {}
bool XenosRendererPresents() { return false; }
bool XenosRendererStrictQueries() { return false; }
bool XenosPollRendererIdle() { return false; }
bool XenosRendererRecipePackOn() { return false; }
bool XenosRendererRecipeIdle() { return false; }
void XenosRendererRelease() {}

// renderer/shader_cache.cpp: the shader prebuild (layer2_ue3/shader_prebuild.cpp reads the game's packages for
// shaders to compile early) has no renderer of ours to hand them to: it is never wanted, and reads nothing.
bool XenosPrebuildWanted() { return false; }
bool XenosPrebuildPixelWanted(uint64_t) { return false; }
bool XenosPrebuildVertexShader(const void*, uint32_t, bool) { return false; }
void XenosPrebuildPixelShaders(const XenosPrebuildShaderRef*, size_t) {}
void XenosPrebuildFileShaders(uint64_t, const uint64_t*, size_t) {}
void XenosPrebuildGameOpened(uint64_t, bool, const char*) {}
void XenosPrebuildAtStartDone(uint64_t) {}
uint64_t XenosPrebuildSelectiveKey() { return 0; }
uint64_t XenosPrebuildVariantHits(uint32_t) { return 0; }
bool XenosRendererInPlay() { return false; }

// gpu_system.cpp: the processors this program may use (no affinity masks on the PS5).
uint32_t XenosUsableProcessors() {
  const unsigned n = std::thread::hardware_concurrency();
  return n ? n : 8;
}

// crash_report.cpp: a note for a crash report goes in the log here (the PS5 layer writes its own crash line).
void XenosCrashNote(const std::string& note) { REXLOG_WARN("crash note: {}", note); }

// ingame_menu.cpp: the in-game menu never opens; no frame-rate cap set in it.
bool XenosInGameMenuIsOpen() noexcept { return false; }
bool XenosLiveMaxFps(int32_t*) { return false; }
void XenosConfigureMenuFonts(ImFontAtlas*) {}

// window_icon.cpp: no window icon on a console.
namespace xenos::window_icon {
void TakeFromRuntime(rex::Runtime*) {}
}  // namespace xenos::window_icon
