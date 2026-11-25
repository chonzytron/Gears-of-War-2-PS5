// Gears of War 2 for PS5 - getting the game ready from the player's own files.
//
// On the PC, COGBox unpacks the disc image and Title Update 6 next to itself and
// starts the game with --game_data_root (the disc's files) and
// --update_data_root (the title update's). A console has no launcher, so the
// app does it at start-up, from whatever the player put in its install folder:
// the disc image is read where it is (mounted as game: / d:), the title update
// package (2.8 MB: default.xexp, its .ini and localisation files and script
// patches) is unpacked to <app>/game/update (mounted as update:, where the
// runtime finds default.xexp), and DLC packages are unpacked where the
// runtime's content manager looks. Nothing is downloaded and no game data is
// part of the app. (After the Dead Space 2 PS5 port's ds2_ps5_install.h.)
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace gow2ps5 {

struct GameFiles {
  // The Gears of War 2 disc image (the US disc the code was recompiled from).
  std::filesystem::path iso;
  // <app>/game/update: Title Update 6's files, default.xexp checked against the
  // hash the recompiled code was made from. Empty when it isn't ready.
  std::filesystem::path update_dir;
  // What was found and done, for the log.
  std::vector<std::string> notes;
  // Why the game can't start (empty when it can), in words for the screen.
  std::string problem;
  // Every package found was prepared whole (the title update and each DLC
  // package unpacked to the end): only then is the preparation recorded.
  bool complete = false;
  // Files in install/ whose contents are prepared and that the game doesn't
  // read (the title update and DLC packages once unpacked), with their sizes:
  // the player may delete them. Never the disc image (the game reads it).
  std::vector<std::pair<std::filesystem::path, uint64_t>> no_longer_needed;
  // The unpacked packages' "unpacked whole" marks (for the record).
  std::vector<std::filesystem::path> markers;

  bool ready() const { return !iso.empty() && !update_dir.empty(); }
};

// How far PrepareGameFiles is, for the screen shown meanwhile (it runs on a
// thread of its own: unpacking a DLC package takes a while).
struct Progress {
  std::atomic<uint64_t> done_bytes{0}, total_bytes{0};
  // Something long is starting: said to the player some other way than the
  // screen as well (a PS5 notification), should the screen not be showing.
  std::function<void(const std::string&)> announce;
  // How many times the screen was drawn (for the log).
  std::atomic<uint64_t> draws{0};

  void SetStep(std::string step) {
    std::lock_guard lock(mutex_);
    step_ = std::move(step);
  }
  std::string step() {
    std::lock_guard lock(mutex_);
    return step_;
  }

 private:
  std::mutex mutex_;
  std::string step_;
};

// Looks through <app>/install (and <app> itself) for the disc image, the title
// update package and DLC packages, unpacks what is missing under <app>/game
// and <content_root>. A package unpacked whole stays so; one that was
// interrupted is gone on with at the next start.
GameFiles PrepareGameFiles(const std::filesystem::path& app_root, const std::filesystem::path& content_root,
                           Progress* progress = nullptr);

// A preparation that went through is recorded (<app>/game/prepared.txt). The
// next start takes the game's files from the record, without looking through
// the install folder or showing a screen, as long as install/ holds the same
// files and what was unpacked is there; anything else makes it prepare again.
bool LoadPreparedRecord(const std::filesystem::path& app_root, GameFiles& out);
void SavePreparedRecord(const std::filesystem::path& app_root, const GameFiles& files);

// Deletes the files listed (from no_longer_needed), returns the bytes freed.
uint64_t DeleteNoLongerNeeded(const std::vector<std::pair<std::filesystem::path, uint64_t>>& files);

}  // namespace gow2ps5
