// Gears of War 2 for PS5 - a controller that presses buttons by itself
// (ps5_input_script), for tests.
//
// A renderer change is checked by playing to the same place with each setting
// and comparing the pictures (ps5_capture). Without someone holding the
// controller, this does the pressing: through the menus, into a save, a few
// steps forward. It goes into what the game reads for the first player
// (XamInputGetState, through the SDK's input system), on top of a real
// controller if there is one, and the game is told a controller is there.
//
// ps5_input_script = "<seconds>:<buttons>[/<ms held>] ...", times counted from
// the game's first look at the controller, presses held 150 ms unless told:
//   ps5_input_script = "20:start 24:a 28:a 32:a 60:lsup/3000"
// Buttons: a b x y start back up down left right lb rb lt rt ls rs, and
// lsup lsdown lsleft lsright rsup rsdown rsleft rsright (a stick pushed all
// the way). Several at once with +: "40:lt+rt/1000". Each press is logged.
//
// ps5_input_record (the console): every change of what the game reads from the
// first controller goes to logs/inputs_<n>.txt, one line each:
//   <ms since the game's first look> <buttons hex> <lt> <rt> <lx> <ly> <rx> <ry> f<frame> p<look>
// (played back by the look number: loading times don't shift it)
// ps5_input_replay = "<file>" (the Linux test build): the game reads that file's
// controller instead, from its first look on (a real controller and the script
// still add to it). The game won't play out exactly the same (enemies, loading
// times), so a long replay drifts; it gets to the same places.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/input/input_system.h>
#include <rex/ps5_diag.h>

REXCVAR_DEFINE_BOOL(ps5_mark_button, false, "PS5",
                    "L3+R3 (both sticks in) marks a moment for troubleshooting: its picture in the logs folder, a "
                    "\"ps5 mark\" line and the next frame's draws in the log");
REXCVAR_DEFINE_INT32(ps5_mark_trace, 0, "PS5",
                     "The first N marks (L3+R3) also record the next frame as a GPU trace in the logs folder "
                     "(<title>_<frame>.xtr), to play it back on the Linux test build (trace_gpu_replay)");
REXCVAR_DEFINE_BOOL(ps5_input_record, false, "PS5",
                    "Write every change of the first controller's state to logs/inputs_<n>.txt, to play it back "
                    "(ps5_input_replay)");
REXCVAR_DEFINE_STRING(ps5_input_replay, "", "PS5",
                      "Play back a controller recorded with ps5_input_record (a logs/inputs_<n>.txt file)");
REXCVAR_DEFINE_BOOL(ps5_input_replay_by_time, false, "PS5",
                    "Play a recording back by its milliseconds, not by the game's looks at the controller");
REXCVAR_DEFINE_STRING(ps5_input_script, "", "PS5",
                      "Buttons pressed by themselves, for tests: \"<seconds>:<buttons>[/<ms held>] ...\" "
                      "(ps5/src/gow2_ps5_script.cpp)");

using rex::X_RESULT;  // (the SDK's status macros name it unqualified)

bool Gow2Ps5MarkPicture(uint32_t n);  // gow2_ps5_capture.h

namespace {

struct Press {
  uint64_t at_ms = 0, until_ms = 0;
  uint16_t buttons = 0;
  uint8_t left_trigger = 0, right_trigger = 0;
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
  std::string text;
  bool logged = false;
};

bool AddButton(Press& press, const std::string& name) {
  struct Button {
    const char* name;
    uint16_t bit;
  };
  static constexpr Button kButtons[] = {
      {"up", 0x0001},    {"down", 0x0002}, {"left", 0x0004}, {"right", 0x0008}, {"start", 0x0010},
      {"back", 0x0020},  {"ls", 0x0040},   {"rs", 0x0080},   {"lb", 0x0100},    {"rb", 0x0200},
      {"a", 0x1000},     {"b", 0x2000},    {"x", 0x4000},    {"y", 0x8000},
  };
  for (const Button& b : kButtons) {
    if (name == b.name) {
      press.buttons |= b.bit;
      return true;
    }
  }
  constexpr int16_t kFull = 32767;
  if (name == "lt") press.left_trigger = 255;
  else if (name == "rt") press.right_trigger = 255;
  else if (name == "lsup") press.ly = kFull;
  else if (name == "lsdown") press.ly = -kFull;
  else if (name == "lsleft") press.lx = -kFull;
  else if (name == "lsright") press.lx = kFull;
  else if (name == "rsup") press.ry = kFull;
  else if (name == "rsdown") press.ry = -kFull;
  else if (name == "rsleft") press.rx = -kFull;
  else if (name == "rsright") press.rx = kFull;
  else return false;
  return true;
}

std::vector<Press> Parse(const std::string& script) {
  std::vector<Press> presses;
  std::string item;
  std::istringstream in(script);
  while (in >> item) {
    if (!item.empty() && item.back() == ',') item.pop_back();
    const size_t colon = item.find(':');
    if (colon == std::string::npos) {
      REXLOG_WARN("ps5 input script: '{}' has no time, left out", item);
      continue;
    }
    Press press;
    press.text = item;
    std::string what = item.substr(colon + 1);
    uint64_t held_ms = 150;
    if (const size_t slash = what.find('/'); slash != std::string::npos) {
      held_ms = std::strtoull(what.c_str() + slash + 1, nullptr, 10);
      what.resize(slash);
    }
    press.at_ms = uint64_t(std::strtod(item.c_str(), nullptr) * 1000.0);
    press.until_ms = press.at_ms + held_ms;
    bool ok = true;
    size_t start = 0;
    while (start <= what.size()) {
      const size_t plus = what.find('+', start);
      const std::string name = what.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
      if (!AddButton(press, name)) {
        REXLOG_WARN("ps5 input script: no button '{}' in '{}', left out", name, item);
        ok = false;
      }
      if (plus == std::string::npos) break;
      start = plus + 1;
    }
    if (ok) presses.push_back(press);
  }
  return presses;
}

struct Script {
  std::vector<Press> presses;
  uint64_t start_ms = 0;
  uint32_t packet = 0;
  uint16_t last_buttons = 0xFFFF;
};

Script* GetScript() {
  static Script* script = [] {
    const std::string text = REXCVAR_GET(ps5_input_script);
    if (text.empty()) return static_cast<Script*>(nullptr);
    auto* s = new Script;
    s->presses = Parse(text);
    REXLOG_INFO("ps5 input script: {} presses: {}", s->presses.size(), text);
    return s;
  }();
  return script;
}

int16_t Stick(int16_t real, int16_t scripted) { return scripted ? scripted : real; }



// [ps5 port] What the game reads for a controller (XamInputGetState, through the SDK's InputSystem::GetState) passes
// through here: the scripted presses go on top of a real controller, and with none the first player has this one.
// (The Dead Space 2 port hooked the game's own XInputGetStateEx by its PDB name; Gears of War 2's recompilation has
// no names, and the SDK's input system is the same for every game.)
// The mark button: L3+R3 held together (they reach the game as nothing while both are in).
void MarkButton(rex::input::X_INPUT_STATE* state) {
  constexpr uint16_t kBoth = 0x0040 | 0x0080;  // (left and right thumb)
  static bool held = false;
  static uint32_t marks = 0;
  const bool both = (state->gamepad.buttons & kBoth) == kBoth;
  if (both && !held) {
    const uint32_t n = ++marks;
    const uint64_t frame = rex::ps5diag::Get(rex::ps5diag::kSwaps);
    const bool picture = Gow2Ps5MarkPicture(n);
    rex::ps5diag::RequestFrameDump(1);
    static const int32_t traces = REXCVAR_GET(ps5_mark_trace);
    if (int32_t(n) <= traces) rex::ps5diag::RequestFrameTrace();
    REXLOG_WARN("ps5 mark {}: L3+R3 at frame {}{}; the next frame's draws follow (\"ps5 dump\")", n, frame,
                picture ? "" : " (no picture: the game isn't drawing yet)");
    rex::FlushLogging();
  }
  held = both;
  if (both) state->gamepad.buttons = uint16_t(uint16_t(state->gamepad.buttons) & uint16_t(~kBoth));
}

void ApplyScript(uint32_t user_index, rex::input::X_INPUT_STATE* state, X_RESULT* result) {
  Script* script = GetScript();
  if (!script || user_index != 0) return;
  const uint64_t now = rex::ps5diag::NowMs();
  if (!script->start_ms) script->start_ms = now;
  const uint64_t t = now - script->start_ms;
  const bool connected = *result == X_ERROR_SUCCESS;
  if (!state) {  // a connection check: there is one now
    *result = X_ERROR_SUCCESS;
    return;
  }
  if (!connected) std::memset(state, 0, sizeof(*state));
  uint16_t buttons = state->gamepad.buttons;
  uint8_t lt = state->gamepad.left_trigger, rt = state->gamepad.right_trigger;
  int16_t lx = state->gamepad.thumb_lx, ly = state->gamepad.thumb_ly;
  int16_t rx = state->gamepad.thumb_rx, ry = state->gamepad.thumb_ry;
  for (Press& p : script->presses) {
    if (t < p.at_ms || t >= p.until_ms) continue;
    if (!p.logged) {
      p.logged = true;
      REXLOG_INFO("ps5 input script: {:.1f} s: {} (frame {})", double(t) / 1000.0, p.text,
                  rex::ps5diag::Get(rex::ps5diag::kSwaps));
    }
    buttons |= p.buttons;
    lt = std::max(lt, p.left_trigger);
    rt = std::max(rt, p.right_trigger);
    lx = Stick(lx, p.lx);
    ly = Stick(ly, p.ly);
    rx = Stick(rx, p.rx);
    ry = Stick(ry, p.ry);
  }
  state->gamepad.buttons = buttons;
  state->gamepad.left_trigger = lt;
  state->gamepad.right_trigger = rt;
  state->gamepad.thumb_lx = lx;
  state->gamepad.thumb_ly = ly;
  state->gamepad.thumb_rx = rx;
  state->gamepad.thumb_ry = ry;
  if (!connected) {
    // (no controller: this one; its packet number moves when its state does)
    if (buttons != script->last_buttons || lx || ly || rx || ry || lt || rt) ++script->packet;
    state->packet_number = script->packet;
    *result = X_ERROR_SUCCESS;
  }
  script->last_buttons = buttons;
}

std::filesystem::path g_logs_folder;

// The game's looks at the first controller since its first (its own clock: one
// or a few a game frame, none while a level loads, so loading times don't shift
// a replay timed by it).
uint64_t g_polls = 0;

struct InputLine {
  uint64_t ms = 0;
  uint64_t poll = 0;
  uint16_t buttons = 0;
  uint8_t lt = 0, rt = 0;
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

// The recorded controller, played back.
struct Replay {
  std::vector<InputLine> lines;
  size_t next = 0;
  uint64_t start_ms = 0;
  InputLine current;
  uint32_t packet = 0;
  bool by_poll = true;  // (files without poll numbers: by time)
};

Replay* GetReplay() {
  static Replay* replay = [] {
    const std::string path = REXCVAR_GET(ps5_input_replay);
    if (path.empty()) return static_cast<Replay*>(nullptr);
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
      REXLOG_WARN("ps5 input replay: can't open {}", path);
      return static_cast<Replay*>(nullptr);
    }
    auto* r = new Replay;
    char text[256];
    while (std::fgets(text, sizeof(text), f)) {
      InputLine l;
      unsigned long long ms;
      unsigned buttons, lt, rt;
      int lx, ly, rx, ry;
      unsigned long long frame = 0, poll = 0;
      const int n = std::sscanf(text, "%llu %x %u %u %d %d %d %d f%llu p%llu", &ms, &buttons, &lt, &rt, &lx,
                                &ly, &rx, &ry, &frame, &poll);
      if (n >= 8) {
        l.ms = ms;
        l.poll = n >= 10 ? poll : 0;
        if (n < 10) r->by_poll = false;
        l.buttons = uint16_t(buttons);
        l.lt = uint8_t(lt);
        l.rt = uint8_t(rt);
        l.lx = int16_t(lx);
        l.ly = int16_t(ly);
        l.rx = int16_t(rx);
        l.ry = int16_t(ry);
        r->lines.push_back(l);
      }
    }
    std::fclose(f);
    REXLOG_INFO("ps5 input replay: {} changes, {:.0f} s, from {}", r->lines.size(),
                r->lines.empty() ? 0.0 : double(r->lines.back().ms) / 1000.0, path);
    return r;
  }();
  return replay;
}

void ApplyReplay(uint32_t user_index, rex::input::X_INPUT_STATE* state, X_RESULT* result) {
  Replay* r = GetReplay();
  if (!r || user_index != 0) return;
  const uint64_t now = rex::ps5diag::NowMs();
  if (!r->start_ms) r->start_ms = now;
  const uint64_t t = now - r->start_ms;
  if (!state) {
    *result = X_ERROR_SUCCESS;
    return;
  }
  static const bool by_time = REXCVAR_GET(ps5_input_replay_by_time);
  const bool poll_clock = r->by_poll && !by_time;
  auto due = [&](const InputLine& l) { return poll_clock ? l.poll <= g_polls : l.ms <= t; };
  const bool moved = r->next < r->lines.size() && due(r->lines[r->next]);
  while (r->next < r->lines.size() && due(r->lines[r->next])) r->current = r->lines[r->next++];
  if (moved) ++r->packet;
  const InputLine& c = r->current;
  const bool connected = *result == X_ERROR_SUCCESS;
  if (!connected) std::memset(state, 0, sizeof(*state));
  state->gamepad.buttons = uint16_t(state->gamepad.buttons | c.buttons);
  state->gamepad.left_trigger = std::max(state->gamepad.left_trigger, c.lt);
  state->gamepad.right_trigger = std::max(state->gamepad.right_trigger, c.rt);
  state->gamepad.thumb_lx = Stick(state->gamepad.thumb_lx, c.lx);
  state->gamepad.thumb_ly = Stick(state->gamepad.thumb_ly, c.ly);
  state->gamepad.thumb_rx = Stick(state->gamepad.thumb_rx, c.rx);
  state->gamepad.thumb_ry = Stick(state->gamepad.thumb_ry, c.ry);
  if (!connected) {
    state->packet_number = r->packet;
    *result = X_ERROR_SUCCESS;
  }
}

// What the game reads, written down when it changes.
void Record(uint32_t user_index, const rex::input::X_INPUT_STATE* state, X_RESULT result) {
  static const bool on = REXCVAR_GET(ps5_input_record);
  if (!on || user_index != 0 || !state) return;
  static FILE* f = nullptr;
  static bool tried = false;
  static uint64_t start_ms = 0, flushed_ms = 0;
  static InputLine last;
  static bool any = false;
  if (!tried) {
    tried = true;
    std::error_code ec;
    std::filesystem::create_directories(g_logs_folder, ec);
    for (uint32_t n = 1; n < 1000; ++n) {
      char name[32];
      std::snprintf(name, sizeof(name), "inputs_%03u.txt", n);
      const std::filesystem::path path = g_logs_folder / name;
      if (std::filesystem::exists(path, ec)) continue;
      f = std::fopen(path.string().c_str(), "w");
      REXLOG_INFO("ps5 input record: {}{}", path.string(), f ? "" : " (can't write it)");
      break;
    }
  }
  if (!f) return;
  const uint64_t now = rex::ps5diag::NowMs();
  if (!start_ms) start_ms = now;
  InputLine l;
  if (result == X_ERROR_SUCCESS) {
    l.buttons = uint16_t(state->gamepad.buttons);
    l.lt = state->gamepad.left_trigger;
    l.rt = state->gamepad.right_trigger;
    l.lx = state->gamepad.thumb_lx;
    l.ly = state->gamepad.thumb_ly;
    l.rx = state->gamepad.thumb_rx;
    l.ry = state->gamepad.thumb_ry;
  }
  const bool same = l.buttons == last.buttons && l.lt == last.lt && l.rt == last.rt && l.lx == last.lx &&
                    l.ly == last.ly && l.rx == last.rx && l.ry == last.ry;
  if (!any || !same) {
    any = true;
    last = l;
    std::fprintf(f, "%llu %04x %u %u %d %d %d %d f%llu p%llu\n", (unsigned long long)(now - start_ms),
                 l.buttons, l.lt, l.rt, l.lx, l.ly, l.rx, l.ry,
                 (unsigned long long)rex::ps5diag::Get(rex::ps5diag::kSwaps), (unsigned long long)g_polls);
  }
  if (now - flushed_ms >= 2000) {
    flushed_ms = now;
    std::fflush(f);
  }
}

// The replay, the script's presses, then the mark button (a real or a scripted
// L3+R3), and what the game then reads is recorded.
void Filter(uint32_t user_index, rex::input::X_INPUT_STATE* state, X_RESULT* result) {
  if (user_index == 0 && state) ++g_polls;
  ApplyReplay(user_index, state, result);
  ApplyScript(user_index, state, result);
  static const bool mark_button = REXCVAR_GET(ps5_mark_button);
  if (mark_button && user_index == 0 && state && *result == X_ERROR_SUCCESS) MarkButton(state);
  Record(user_index, state, *result);
}

}  // namespace

// Called by the app once the runtime exists (gow2_ps5_app.h, OnPostSetup); nothing happens without a script.
void Gow2Ps5InstallInputScript(const std::filesystem::path& logs_folder) {
  g_logs_folder = logs_folder;
  if (GetScript() || GetReplay() || REXCVAR_GET(ps5_mark_button) || REXCVAR_GET(ps5_input_record))
    rex::input::SetInputStateFilter(&Filter);
  if (REXCVAR_GET(ps5_mark_button)) REXLOG_INFO("ps5 mark button: L3+R3 marks a moment (picture, log line, draws)");
}
