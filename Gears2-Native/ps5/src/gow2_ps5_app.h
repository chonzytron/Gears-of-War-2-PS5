// Gears of War 2 for PS5 - the app.
//
// COGBox's PC app (src/layer3_gow2/gow2_app.h) with what a console needs
// instead of a launcher: the game's files are found and prepared at start-up
// (gow2_ps5_install.h), everything lives in the app's own folder (/app0, which
// is /data/homebrew/<title id>), and a screen says what to do when the files
// aren't there. The game runs on the SDK's Xenos GPU emulation (COGBox's
// --gow2_gpu=sdk); COGBox's game layer (its hooks, UE3 config edits, unlocks,
// cheats) is the PC's, compiled as it is. (After the Dead Space 2 PS5 port's
// ds2_ps5_app.h.)
#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/disc_image_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/ps5_diag.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system.h>
#include <rex/thread.h>
#include <rex/ui/imgui_dialog.h>

#include <imgui.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <pthread.h>
#include <unistd.h>

#include <fmt/format.h>

#include <ue3/config_bundle.h>
#include <ue3/launch.h>
#include <xdk7645/stage1.h>

#include "gow2_build_stamp.h"
#include "gow2_ps5_capture.h"
#include "gow2_ps5_install.h"
#include "main_menu_exit.h"

// COGBox's game layer (src/layer3_gow2), as its gow2_app.h declares it.
void Gow2ApplyDataPatchesIfRequested(rex::Runtime* runtime);
void Gow2InstallLocLogIfRequested();
void Gow2InstallUnlocks();
void Gow2InstallCheats();
void Gow2InstallFirstPerson();
void Gow2AddTestIniEdits();
void Gow2Ps5InstallInputScript(const std::filesystem::path& logs_folder);  // gow2_ps5_script.cpp
namespace rex::system {
class XThread;
}
void XenosHoldGuestIfRequested(rex::system::XThread* main_thread);  // COGBox layer0_xenos/hold_guest.cpp

// The game's language: the Xbox 360's, taken from the PS5's unless set here.
REXCVAR_DEFINE_STRING(ps5_language, "auto", "PS5",
                      "Game language: auto (the PS5's), english, french, german, spanish, italian, polish, russian, "
                      "korean or chinese");

// The screen shown while the game's files are prepared.
REXCVAR_DEFINE_BOOL(ps5_prepare_screen, true, "PS5", "Show a progress screen while the game's files are prepared");

// The first start (or one after the install folder changed): the packages in
// install/ the game no longer needs once unpacked (Title Update 6, DLC) are
// offered for deleting, on the screen.
REXCVAR_DEFINE_STRING(ps5_install_cleanup, "ask", "PS5",
                      "Files in install/ no longer needed once prepared: ask (on the first start), keep "
                      "(never ask), or delete (without asking)");

// A frames-a-second counter in the top right corner of the picture.
REXCVAR_DECLARE(bool, ps5_cp_timers);  // ps5_diag.cpp
REXCVAR_DECLARE(std::string, ue3_ini_edits);  // COGBox layer 2 (config_install.cpp)
REXCVAR_DEFINE_BOOL(ps5_motion_blur, true, "PS5",
                    "The game's motion blur (the blur of fast camera movement); false turns it off through the "
                    "engine's own setting, [SystemSettings] MotionBlur in Xe-GearEngine.ini");
REXCVAR_DEFINE_BOOL(ps5_show_fps, false, "PS5", "Show the frames a second in the top right corner");

// For finding what limits the frame rate: where every thread spends its time,
// in the log every 30 s (rex/ps5_diag.h, StartProfiler). Costs a few percent.
REXCVAR_DEFINE_BOOL(ps5_profile, false, "PS5",
                    "Log where each thread spends its time, every 30 s (for performance work)");

// The Epic Games and Microsoft Game Studios logos at start-up, skipped with the engine's own -nostartupmovies
// (UE3's command line, through COGBox's gow2_launch); the game's other films still play.
REXCVAR_DEFINE_BOOL(ps5_skip_intro_videos, false, "PS5", "Skip the logo videos at start-up");
REXCVAR_DECLARE(std::string, gow2_launch);  // layer2_ue3/launch_command_line.cpp

extern "C" int sceSystemServiceParamGetInt(int param_id, int* value);
extern "C" int sceKernelSendNotificationRequest(int, void*, size_t, int);
extern "C" int sceUserServiceInitialize(void* params);
extern "C" int sceUserServiceGetForegroundUser(int32_t* user_id);
extern "C" int scePadInit(void);
extern "C" int scePadOpen(int32_t user_id, int32_t type, int32_t index, const void* param);
extern "C" int scePadGetHandle(int32_t user_id, int32_t type, int32_t index);
extern "C" int scePadReadState(int32_t handle, void* data);

namespace gow2ps5 {

inline void Notify(const std::string& text) {
  struct {
    char unused[45];
    char message[3075];
  } request;
  std::memset(&request, 0, sizeof(request));
  std::strncpy(request.message, text.c_str(), sizeof(request.message) - 1);
  sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

// The Xbox 360 language id (XLanguage) for the ps5_language setting, from the
// PS5's system language when "auto". Gears of War 2's US disc has English,
// French, German, Spanish, Italian, Chinese (traditional), Korean, Polish,
// Russian, Czech and Hungarian; the Xbox 360 has no id for the last two, so
// those, and every other PS5 language, are English.
inline uint32_t XboxLanguage(const std::string& setting) {
  constexpr uint32_t kEnglish = 1, kGerman = 3, kFrench = 4, kSpanish = 5, kItalian = 6, kKorean = 7,
                     kTChinese = 8, kPolish = 11, kRussian = 12;
  if (setting == "english") return kEnglish;
  if (setting == "french") return kFrench;
  if (setting == "german") return kGerman;
  if (setting == "spanish") return kSpanish;
  if (setting == "italian") return kItalian;
  if (setting == "polish") return kPolish;
  if (setting == "russian") return kRussian;
  if (setting == "korean") return kKorean;
  if (setting == "chinese") return kTChinese;
  int ps5 = 1;
  if (sceSystemServiceParamGetInt(1 /* SCE_SYSTEM_SERVICE_PARAM_ID_LANG */, &ps5) != 0) ps5 = 1;
  switch (ps5) {
    case 2:   // French
    case 22:  // French (Canada)
      return kFrench;
    case 3:   // Spanish
    case 20:  // Spanish (Latin America)
      return kSpanish;
    case 4:
      return kGerman;
    case 5:
      return kItalian;
    case 8:
      return kRussian;
    case 9:
      return kKorean;
    case 10:  // Chinese (traditional)
    case 11:  // Chinese (simplified): the disc's Chinese is the traditional one
      return kTChinese;
    case 16:
      return kPolish;
    default:
      return kEnglish;
  }
}

// The first player's controller buttons (ScePadData's), for the screens before
// the game: the game's own controller code isn't running yet. 0 without one
// (and on the Linux test build, which has none of these).
constexpr uint32_t kPadCircle = 0x00002000u, kPadCross = 0x00004000u;
inline uint32_t ReadPadButtons() {
  static const int32_t handle = [] {
    sceUserServiceInitialize(nullptr);
    scePadInit();
    int32_t user = -1;
    if (sceUserServiceGetForegroundUser(&user) != 0 || user < 0) return int32_t(-1);
    int32_t opened = scePadOpen(user, 0, 0, nullptr);
    if (opened < 0) opened = scePadGetHandle(user, 0, 0);  // (already open: its handle)
    return opened;
  }();
  if (handle < 0) return 0;
  struct {
    uint32_t buttons;
    uint8_t rest[252];  // (room for the whole ScePadData)
  } data{};
  return scePadReadState(handle, &data) == 0 ? data.buttons : 0;
}

// Text over the whole screen, scaled to read from a sofa.
inline void BeginFullScreen(const char* id, ImGuiIO& io, float* scale, float* margin) {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin(id, nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoNav);
  *scale = io.DisplaySize.y > 0 ? io.DisplaySize.y / 540.0f : 2.0f;
  *margin = io.DisplaySize.x * 0.08f;
}

// A screen of text that stays up: what is missing and where to put it. There
// is no game to run behind it; the PS button closes the app.
class MessageScreen final : public rex::ui::ImGuiDialog {
 public:
  MessageScreen(rex::ui::ImGuiDrawer* drawer, std::string title, std::string body)
      : rex::ui::ImGuiDialog(drawer), title_(std::move(title)), body_(std::move(body)) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    float scale, margin;
    BeginFullScreen("##gow2ps5-message", io, &scale, &margin);
    ImGui::SetWindowFontScale(scale * 1.4f);
    ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.12f));
    ImGui::TextUnformatted(title_.c_str());
    ImGui::SetWindowFontScale(scale);
    ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.24f));
    ImGui::PushTextWrapPos(io.DisplaySize.x - margin);
    ImGui::TextUnformatted(body_.c_str());
    ImGui::PopTextWrapPos();
    ImGui::End();
  }

 private:
  std::string title_, body_;
};

// The pictures the game presented in the last half second, as frames a
// second, in the top right corner (ps5_show_fps).
class FpsCounter final : public rex::ui::ImGuiDialog {
 public:
  explicit FpsCounter(rex::ui::ImGuiDrawer* drawer)
      : rex::ui::ImGuiDialog(drawer),
        frames_(rex::ps5diag::Get(rex::ps5diag::kSwaps)),
        since_ms_(rex::ps5diag::NowMs()) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    const uint64_t now_ms = rex::ps5diag::NowMs();
    if (now_ms - since_ms_ >= 500) {
      const uint64_t frames = rex::ps5diag::Get(rex::ps5diag::kSwaps);
      fps_ = double(frames - frames_) * 1000.0 / double(now_ms - since_ms_);
      frames_ = frames;
      since_ms_ = now_ms;
    }
    // (sized for the TV as the other screens are, and in from the corner: TVs crop the edge)
    const float scale = io.DisplaySize.y > 0 ? io.DisplaySize.y / 540.0f : 2.0f;
    const float margin = io.DisplaySize.y * 0.03f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - margin, margin), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.4f);
    ImGui::Begin("##gow2ps5-fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(scale * 0.8f);
    ImGui::Text("%.0f FPS", fps_);
    ImGui::End();
  }

 private:
  uint64_t frames_, since_ms_;
  double fps_ = 0.0;
};

// After the first preparation: the packages in install/ the game no longer
// needs, and a choice, Cross to delete them or Circle to keep them (Enter or
// Escape on a PC). Asked once: the answer is kept with the preparation's record.
class CleanupScreen final : public rex::ui::ImGuiDialog {
 public:
  CleanupScreen(rex::ui::ImGuiDrawer* drawer,
                const std::vector<std::pair<std::filesystem::path, uint64_t>>& files,
                std::function<void(bool)> answer)
      : rex::ui::ImGuiDialog(drawer), answer_(std::move(answer)) {
    uint64_t total = 0;
    for (const auto& [path, size] : files) {
      list_ += fmt::format("    {}   ({} MB)\n", path.filename().string(), size >> 20);
      total += size;
    }
    total_mb_ = total >> 20;
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    // (a press counts from the screen on: one held from before doesn't)
    const uint32_t pad = ReadPadButtons();
    const uint32_t pressed = first_ ? 0 : pad & ~previous_pad_;
    previous_pad_ = pad;
    first_ = false;
    float scale, margin;
    BeginFullScreen("##gow2ps5-cleanup", io, &scale, &margin);
    ImGui::SetWindowFontScale(scale * 1.4f);
    ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.12f));
    ImGui::TextUnformatted("Gears of War 2 is ready");
    ImGui::SetWindowFontScale(scale);
    ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.24f));
    ImGui::PushTextWrapPos(io.DisplaySize.x - margin);
    ImGui::TextUnformatted(fmt::format(
        "These files in the install folder have been unpacked, and the game no longer needs them:\n\n{}\n"
        "Deleting them frees {} MB. The disc image stays: the game reads from it. (If you ever need "
        "these files again, copy them back in.)\n\n"
        "Cross: delete them        Circle: keep them",
        list_, total_mb_).c_str());
    ImGui::PopTextWrapPos();
    ImGui::End();
    if (answered_) return;
    const bool remove = (pressed & kPadCross) || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    const bool keep = (pressed & kPadCircle) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (remove || keep) {
      answered_ = true;
      answer_(remove);
      Close();
    }
  }

 private:
  std::function<void(bool)> answer_;
  std::string list_;
  uint64_t total_mb_ = 0;
  uint32_t previous_pad_ = 0;
  bool first_ = true, answered_ = false;
};

// What is shown while the game's files are looked through and prepared.
class PrepareScreen final : public rex::ui::ImGuiDialog {
 public:
  PrepareScreen(rex::ui::ImGuiDrawer* drawer, std::shared_ptr<Progress> progress)
      : rex::ui::ImGuiDialog(drawer), progress_(std::move(progress)), started_(std::chrono::steady_clock::now()) {}

  // (the base class's Close is for the dialog itself to call)
  void Finish() { Close(); }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (progress_->draws++ == 0) REXLOG_INFO("gow2 files: the screen is being drawn");
    float scale, margin;
    BeginFullScreen("##gow2ps5-prepare", io, &scale, &margin);
    const float width = io.DisplaySize.x - 2 * margin;
    ImGui::SetWindowFontScale(scale * 1.4f);
    ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.12f));
    ImGui::TextUnformatted("Getting Gears of War 2 ready");
    ImGui::SetWindowFontScale(scale);
    ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.24f));
    ImGui::PushTextWrapPos(io.DisplaySize.x - margin);
    const int seconds = int(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started_).count());
    // (a few dots that move, so a long step doesn't look stuck)
    ImGui::TextUnformatted((progress_->step() + std::string(size_t(1 + seconds % 3), '.')).c_str());
    const uint64_t total = progress_->total_bytes.load(), done = std::min(progress_->done_bytes.load(), total);
    if (total) {
      ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.36f));
      ImGui::ProgressBar(float(double(done) / double(total)), ImVec2(width, 28.0f * scale),
                         fmt::format("{} of {} MB", done >> 20, total >> 20).c_str());
      ImGui::SetCursorPos(ImVec2(margin, io.DisplaySize.y * 0.48f));
      ImGui::TextUnformatted(
          "This is done once. If the app is closed before it is finished, the next start goes on from where this "
          "one stopped.");
    }
    ImGui::PopTextWrapPos();
    ImGui::End();
  }

 private:
  std::shared_ptr<Progress> progress_;  // (shared: the screen is drawn once more after Finish)
  std::chrono::steady_clock::time_point started_;
};

}  // namespace gow2ps5

class Gow2Ps5App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Gow2Ps5App>(new Gow2Ps5App(ctx, "gow2", PPCImageConfig));
  }

 protected:
  // After gow2.toml is loaded and before the GPU backend is created. On the PS5
  // the backend is part of the program, so its flags exist already and take
  // their defaults here. Only what the platform needs and what COGBox's
  // launcher always passes (cogbox/src/gears_settings.cpp, build_args):
  // everything else is the SDK's and COGBox's own default until a console run
  // says otherwise (the toml can set any of it for a test).
  void OnPostInitLogging() override {
    // COGBox's launcher: --gpu_plugin=xenos (and its exe's --gow2_gpu=ours, which the PS5 doesn't have: the SDK's
    // Xenos emulation draws here), --host_present_from_non_ui_thread=false.
    DefaultFlag("gpu_plugin", "xenos");
    DefaultFlag("trace_gpu_prefix", (app_root() / "logs").string().c_str());  // (GPU traces: ps5_mark_trace)
    DefaultFlag("host_present_from_non_ui_thread", "false");
    // COGBox's PC menu opens on Back + Start, so its SDK patch 43 holds both back from the game for a moment; the
    // PS5 has no such menu, and the game gets them as the Xbox 360 gave them.
    DefaultFlag("menu_combo_guard", "false");
    // No keyboard or mouse to play with.
    DefaultFlag("mnk_mode", "false");
    // V-Synced presentation: the swapchain otherwise prefers IMMEDIATE, which tears.
    DefaultFlag("vulkan_allow_present_mode_immediate", "false");
    // One ordinary buffer for the console's 512 MB instead of a sparse one: the PS5's driver is a port (as in DS2).
    DefaultFlag("vulkan_sparse_shared_memory", "false");
    // Release 1.0: what the console runs confirmed (runs 5-31; see CLAUDE.md), so a player needs no gow2.toml.
    // Native 4K at 60 frames a second.
    DefaultFlag("resolution_scale", "3");
    DefaultFlag("vsync", "false");            // the game's every-2nd-vblank wait no longer holds it to 30
    DefaultFlag("gow2_max_fps", "60");        // the engine's own limiter
    DefaultFlag("clock_no_scaling", "true");  // (as Dead Space 2)
    // The renderer and command-processor work, each checked picture for picture on the Linux test build.
    DefaultFlag("ps5_full_res_clears", "true");
    DefaultFlag("ps5_resolve_forward", "2");
    DefaultFlag("ps5_draw_extent_from_viewport", "true");
    DefaultFlag("ps5_forward_into_larger", "true");
    DefaultFlag("ps5_skip_dead_transfers", "true");
    DefaultFlag("clear_memory_page_state", "false");
    DefaultFlag("ps5_reuse_texture_bindings", "true");
    DefaultFlag("ps5_forward_10bit", "true");
    DefaultFlag("ps5_forward_skip_memory", "true");
    DefaultFlag("ps5_forward_partial_depth", "true");
    DefaultFlag("ps5_forward_8888", "true");
    DefaultFlag("ps5_dynamic_constants", "true");
    // Every console run since 0.1.14 had motion blur off; the 60 was measured that way (gow2.toml can turn it on).
    DefaultFlag("ps5_motion_blur", "false");
    DefaultFlag("ps5_skip_intro_videos", "true");
    // A quiet log: the 10 s and slow-frame lines only when gow2.toml asks for them.
    DefaultFlag("ps5_perf_log", "false");
    DefaultFlag("user_language", std::to_string(gow2ps5::XboxLanguage(REXCVAR_GET(ps5_language))).c_str());
    REXLOG_INFO("Gears of War 2 for PS5 {} (build {}): language {} ({}), config {}", GOW2_PS5_VERSION,
                GOW2_PS5_BUILD, rex::cvar::GetFlagByName("user_language"), REXCVAR_GET(ps5_language),
                (app_root() / "gow2.toml").string());
    // The settings that matter for a console run, as this program has them (a gow2.toml line an older program
    // doesn't know is not in its list, and is ignored).
    std::string settings;
    for (const char* name : {"resolution_scale", "readback_resolve", "clock_no_scaling", "clear_memory_page_state",
                             "ps5_draw_extent_from_viewport", "ps5_reuse_texture_bindings",
                             "ps5_submit_thread", "gow2_untiled", "gow2_patch_gpu_wait", "gow2_patch_query_wait",
                             "gow2_max_fps", "gow2_patch_texture_pool_mb", "ps5_skip_intro_videos",
                             "ps5_display_height", "ps5_perf_log", "ps5_capture", "ps5_mark_button", "ps5_profile",
                             "cogbox_net_trace", "execute_unclipped_draw_vs_on_cpu", "ps5_full_res_clears",
                             "ps5_motion_blur", "ps5_resolve_forward", "ps5_forward_into_larger",
                             "ps5_skip_dead_transfers", "vsync", "ps5_fast_ticks", "ps5_packet_timing", "ps5_register_ranges", "ps5_texture_view_cache", "ps5_upload_check", "ps5_vertex_buffer_frame_sync", "ps5_watch_first_guest_page", "ps5_forward_10bit", "ps5_forward_skip_memory", "ps5_forward_packed_rule_old", "ps5_forward_partial_depth", "ps5_forward_8888", "ps5_dynamic_constants", "ps5_submit_chunk_draws", "ps5_cp_timers"}) {
      std::string value = rex::cvar::GetFlagByName(name);
      settings += fmt::format("{}{} = {}", settings.empty() ? "" : ", ", name,
                              value.empty() ? "(not in this program)" : value);
    }
    REXLOG_INFO("ps5 settings: {}", settings);
    // The command processor's timers only while something reports them (rex::ps5diag::timers_on).
    rex::ps5diag::timers_on =
        (rex::ps5diag::PerfLog() || REXCVAR_GET(ps5_profile)) && REXCVAR_GET(ps5_cp_timers);
  }

  // The runtime (and its input system) exists; the game hasn't started. COGBox's game layer, as its
  // Gow2App::OnPostSetup calls it, without the parts for its own renderer, window, mouse and PC tools.
  // --gow2_hold_guest (COGBox): the game never starts, for --trace_gpu_replay (a GPU trace of one frame, recorded
  // on the console with ps5_mark_trace, played back here).
  void OnPostLaunchModule(rex::system::XThread* thread) override { XenosHoldGuestIfRequested(thread); }

  void OnPostSetup() override {
    Gow2Ps5InstallInputScript(app_root() / "logs");
    Gow2AddTestIniEdits();
    if (!REXCVAR_GET(ps5_motion_blur)) {
      // (the engine's own switch, as COGBox's Motion blur setting sets it)
      const std::string edits = REXCVAR_GET(ue3_ini_edits);
      rex::cvar::SetFlagByName("ue3_ini_edits", "Xe-GearEngine.ini|SystemSettings|MotionBlur=False" +
                                                     (edits.empty() ? std::string() : ";" + edits));
      REXLOG_INFO("ps5_motion_blur: off (Xe-GearEngine.ini [SystemSettings] MotionBlur=False)");
    }
    Ue3InstallConfigEditsIfRequested(runtime());
    Gow2InstallLocLogIfRequested();
    Gow2InstallMainMenuExitIfRequested();
    Gow2InstallUnlocks();
    Gow2InstallCheats();
    Gow2InstallFirstPerson();
    Gow2InstallResourceCheckIfRequested(runtime());
  }

  void OnPostLoadXexImage() override { Gow2ApplyDataPatchesIfRequested(runtime()); }

  // The window and the overlay drawer exist; the runtime doesn't yet. The
  // game's files are looked for and prepared here, on a thread of their own
  // with a screen that says how far that is. The game starts when it is done;
  // without the files the app stays on a screen that says what to copy where.
  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
                                                 std::function<void(rex::PathConfig)> resume) override {
    // Prepared before, and the install folder as it was then: straight on.
    if (gow2ps5::LoadPreparedRecord(app_root(), files_)) {
      return FilesPrepared(defaults);
    }
    if (!imgui_drawer() || !REXCVAR_GET(ps5_prepare_screen)) {
      return PrepareWithoutScreen(defaults);
    }
    prepare_ = std::make_unique<Prepare>();
    prepare_->app = this;
    prepare_->defaults = defaults;
    prepare_->resume = std::move(resume);
    prepare_->progress->announce = [](const std::string& text) { gow2ps5::Notify(text); };
    prepare_->screen = new gow2ps5::PrepareScreen(imgui_drawer(), prepare_->progress);
    // (a stack of its own size: a new thread's default is the console's to choose)
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, size_t(4) << 20);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    const int failed = pthread_create(&thread, &attributes, &PrepareThread, prepare_.get());
    pthread_attr_destroy(&attributes);
    if (failed) {
      REXLOG_WARN("gow2 files: no thread to prepare them on ({}): prepared with nothing on the screen", failed);
      prepare_->screen->Finish();
      prepare_.reset();
      return PrepareWithoutScreen(defaults);
    }
    return std::nullopt;  // (resumed from PrepareDone)
  }

  // After Runtime::Setup (the VFS exists: game: and update: are the unpacked
  // title update folder) and before LoadXexImage: the disc image as game: and
  // d:, read where it is. The game loads game:\default.xex from the disc, and
  // the runtime takes update:\default.xexp with it (COGBox SDK patch 31).
  void OnLoadXexImage(std::string& xex_image) override {
    if (!files_.ready()) Fail("No Gears of War 2 disc image is ready");
    auto* fs = runtime()->file_system();
    constexpr const char* kDisc = "\\Device\\Cdrom0";
    auto disc = std::make_unique<rex::filesystem::DiscImageDevice>(kDisc, files_.iso);
    if (!disc->Initialize()) Fail("The disc image can't be read: " + files_.iso.string());
    if (!fs->RegisterDevice(std::move(disc))) Fail("The disc image can't be mounted");
    {
      auto lock = rex::thread::global_critical_region::AcquireDirect();
      for (const char* alias : {"game:", "d:"}) {
        fs->UnregisterSymbolicLink(alias);
        if (!fs->RegisterSymbolicLink(alias, kDisc)) Fail(std::string("Can't point ") + alias + " at the disc");
      }
    }
    REXLOG_INFO("gow2: disc {} as game: and d:, Title Update 6 from {} as update:; loading {}", files_.iso.string(),
                files_.update_dir.string(), xex_image);
  }

  // Before the overlay drawer goes: a dialog takes itself off it when destroyed.
  void OnShutdown() override { fps_counter_.reset(); }

  void OnPreLaunchModule() override {
    if (REXCVAR_GET(ps5_skip_intro_videos)) {
      const std::string launch = REXCVAR_GET(gow2_launch);
      if (launch.find("-nostartupmovies") == std::string::npos) {
        rex::cvar::SetFlagByName("gow2_launch", launch.empty() ? "-nostartupmovies" : launch + " -nostartupmovies");
      }
    }
    Ue3SetLaunchCommandLineIfRequested();
    // (here and not earlier: over the game, not over the screens before it)
    if (REXCVAR_GET(ps5_profile)) rex::ps5diag::StartProfiler(30);
    if (runtime() && runtime()->graphics_system()) {
      gow2ps5::PictureCapture::Start(runtime()->graphics_system()->presenter(), app_root() / "logs");
    }
    if (REXCVAR_GET(ps5_show_fps) && imgui_drawer() && !fps_counter_) {
      fps_counter_ = std::make_unique<gow2ps5::FpsCounter>(imgui_drawer());
    }
  }

 private:
  struct Prepare {
    Gow2Ps5App* app = nullptr;
    rex::PathConfig defaults;
    std::function<void(rex::PathConfig)> resume;
    std::shared_ptr<gow2ps5::Progress> progress = std::make_shared<gow2ps5::Progress>();
    gow2ps5::PrepareScreen* screen = nullptr;  // (deletes itself once finished)
    gow2ps5::GameFiles files;
  };

  gow2ps5::GameFiles files_;
  std::unique_ptr<Prepare> prepare_;
  std::unique_ptr<gow2ps5::FpsCounter> fps_counter_;

  static void* PrepareThread(void* argument) {
    auto* prepare = static_cast<Prepare*>(argument);
    rex::ps5diag::RegisterThread("gow2 files");
    Gow2Ps5App* app = prepare->app;
    prepare->files =
        gow2ps5::PrepareGameFiles(app_root(), prepare->defaults.user_data_root, prepare->progress.get());
    rex::ps5diag::UnregisterThread();
    app->app_context().CallInUIThread([app] { app->PrepareDone(); });
    return nullptr;
  }

  std::optional<rex::PathConfig> PrepareWithoutScreen(const rex::PathConfig& defaults) {
    gow2ps5::Progress progress;
    progress.announce = [](const std::string& text) { gow2ps5::Notify(text); };
    files_ = gow2ps5::PrepareGameFiles(app_root(), defaults.user_data_root, &progress);
    // (no screen to ask on: only ps5_install_cleanup = delete deletes)
    RecordPreparation(REXCVAR_GET(ps5_install_cleanup) == "delete");
    return FilesPrepared(defaults);
  }

  // A preparation that went through: the files no longer needed deleted if
  // asked to, then the record that lets the next start skip all this.
  void RecordPreparation(bool delete_no_longer_needed) {
    if (!files_.complete) return;
    if (delete_no_longer_needed && !files_.no_longer_needed.empty()) {
      const uint64_t freed = gow2ps5::DeleteNoLongerNeeded(files_.no_longer_needed);
      gow2ps5::Notify(fmt::format("Gears of War 2: {} MB freed in the install folder", freed >> 20));
    }
    gow2ps5::SavePreparedRecord(app_root(), files_);
  }

  // On the UI thread, when the files are prepared (or can't be).
  void PrepareDone() {
    std::unique_ptr<Prepare> prepare = std::move(prepare_);
    if (!prepare) return;
    files_ = std::move(prepare->files);
    REXLOG_INFO("gow2 files: prepared; the screen was drawn {} times", prepare->progress->draws.load());
    prepare->screen->Finish();
    const std::string cleanup = REXCVAR_GET(ps5_install_cleanup);
    if (files_.complete && !files_.no_longer_needed.empty() && cleanup == "ask" && imgui_drawer()) {
      REXLOG_INFO("gow2 files: {} files no longer needed: asking whether to delete them",
                  files_.no_longer_needed.size());
      rex::PathConfig defaults = prepare->defaults;
      std::function<void(rex::PathConfig)> resume = prepare->resume;
      new gow2ps5::CleanupScreen(imgui_drawer(), files_.no_longer_needed, [this, defaults, resume](bool remove) {
        REXLOG_INFO("gow2 files: the player chose to {} them", remove ? "delete" : "keep");
        // (on with the start outside the screen's drawing)
        app_context().CallInUIThread([this, defaults, resume, remove] {
          RecordPreparation(remove);
          if (auto paths = FilesPrepared(defaults)) resume(std::move(*paths));
        });
      });
      return;
    }
    RecordPreparation(cleanup == "delete");
    if (auto paths = FilesPrepared(prepare->defaults)) prepare->resume(std::move(*paths));
  }

  // The paths to start the game with; without the game's files, the screen
  // that says what is missing (and nothing: the app stays on it).
  std::optional<rex::PathConfig> FilesPrepared(const rex::PathConfig& defaults) {
    for (const std::string& note : files_.notes) REXLOG_INFO("gow2 files: {}", note);
    rex::FlushLogging();
    if (!files_.ready()) {
      REXLOG_ERROR("gow2 files: {}", files_.problem);
      rex::FlushLogging();
      gow2ps5::Notify("Gears of War 2: game files are missing - see the screen");
      if (imgui_drawer()) {
        new gow2ps5::MessageScreen(imgui_drawer(), "Gears of War 2 needs your game files", files_.problem);
        return std::nullopt;  // (never resumed: the screen stays until the app is closed)
      }
      Fail(files_.problem);
    }
    rex::PathConfig paths = defaults;
    // game: starts as the title update folder (a host folder the runtime can mount before the game is loaded);
    // OnLoadXexImage points game: and d: at the disc image.
    paths.game_data_root = files_.update_dir;
    paths.update_data_root = files_.update_dir;
    return paths;
  }

  static std::filesystem::path app_root() { return rex::filesystem::GetExecutableFolder(); }

  static void DefaultFlag(const char* name, const char* value) {
    if (!rex::cvar::GetFlagInfo(name) || rex::cvar::GetFlagSource(name) != rex::cvar::Source::kDefault) return;
    rex::cvar::SetFlagByName(name, value);
  }

  [[noreturn]] static void Fail(const std::string& message) {
    REXLOG_ERROR("{}", message);
    rex::FlushLogging();
    gow2ps5::Notify("Gears of War 2: " + message);
    sleep(3);  // (let the notification show)
    std::_Exit(1);
  }
};
