// Gears of War 2 for PS5 - see gow2_ps5_install.h. (After the Dead Space 2 PS5
// port's ds2_ps5_install.cpp: one disc instead of two, and the title update
// unpacked whole, as COGBox does.)
#include "gow2_ps5_install.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <system_error>

#include <fmt/format.h>

#include <rex/filesystem/devices/disc_image_device.h>
#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/filesystem/devices/stfs_container_entry.h>
#include <rex/filesystem/direct_io.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/logging.h>
#include <rex/system/util/xex2_info.h>
#include <rex/system/xcontent.h>

// (by its folder: FreeBSD, and so the PS5 SDK, has a <sha256.h> of its own)
#include "crypto/sha256.h"

#include "gow2_ps5_io.h"

namespace gow2ps5 {

namespace fs = std::filesystem;
using rex::X_STATUS;  // (the SDK's status macros name it unqualified)

namespace {

constexpr uint32_t kTitleId = 0x4D53082D;  // Gears of War 2 (the US disc; COGBox's x360fs.h)

// The exact files generated/tu6 was recompiled from: COGBox's tested copy (cogbox/src/games.cpp
// pins them by SHA-1: b6860bf3... and 8689bf52...).
constexpr const char* kXexSha256 = "e98c25b4b9d173ac7ff69c84e7f1b4240c6be77c5fd7f672041a95221f68247c";  // 0.0.0.5
constexpr const char* kXexpSha256 = "f219c7dc96a4f06b4fcb75441fa554e177a95c05a3196042c03b9da01583337a";  // TU6

std::string Sha256(const std::vector<uint8_t>& data) {
  sha256::SHA256 hash;
  hash.add(data.data(), data.size());
  return hash.getHash();
}

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

bool ReadWholeFile(const fs::path& path, std::vector<uint8_t>& out, size_t limit) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > limit) return false;
  out.resize(size_t(size));
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  const size_t got = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
  std::fclose(f);
  return got == out.size();
}

// Written under a temporary name and renamed, so a file is either whole or absent.
bool WriteWholeFile(const fs::path& path, const uint8_t* data, size_t size) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  const fs::path temporary = path.string() + ".part";
  std::FILE* f = std::fopen(temporary.c_str(), "wb");
  if (!f) return false;
  const size_t put = size ? std::fwrite(data, 1, size, f) : 0;
  const bool ok = std::fclose(f) == 0 && put == size;
  if (!ok) {
    fs::remove(temporary, ec);
    return false;
  }
  fs::rename(temporary, path, ec);
  return !ec;
}

struct CloseFile {
  void operator()(rex::filesystem::File* f) const {
    if (f) f->Destroy();
  }
};

// A whole file out of a mounted image or package.
bool ReadEntry(rex::filesystem::Entry* entry, std::vector<uint8_t>& out, size_t limit) {
  if (!entry || entry->size() > limit) return false;
  rex::filesystem::File* raw = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kGenericRead, &raw) != X_STATUS_SUCCESS || !raw) return false;
  std::unique_ptr<rex::filesystem::File, CloseFile> file(raw);
  out.resize(entry->size());
  size_t done = 0;
  while (done < out.size()) {
    size_t got = 0;
    const size_t want = std::min<size_t>(out.size() - done, size_t(4) << 20);
    if (file->ReadSync({out.data() + done, want}, done, &got) != X_STATUS_SUCCESS || !got) return false;
    done += got;
  }
  return true;
}

// Whether a default.xex is Gears of War 2's (by its execution info: title id, one disc).
bool IsGears2Xex(const std::vector<uint8_t>& xex) {
  if (xex.size() < sizeof(rex::xex2_header)) return false;
  rex::xex2_header header;
  std::memcpy(&header, xex.data(), sizeof(header));
  if (header.magic != 0x58455832) return false;  // 'XEX2'
  const size_t first = offsetof(rex::xex2_header, headers);
  const uint32_t count = header.header_count;
  if (count > (xex.size() - first) / sizeof(rex::xex2_opt_header)) return false;
  for (uint32_t i = 0; i < count; ++i) {
    rex::xex2_opt_header opt;
    std::memcpy(&opt, xex.data() + first + i * sizeof(opt), sizeof(opt));
    if (opt.key != rex::XEX_HEADER_EXECUTION_INFO) continue;
    const uint32_t offset = opt.offset;
    if (offset > xex.size() || sizeof(rex::xex2_opt_execution_info) > xex.size() - offset) return false;
    rex::xex2_opt_execution_info info;
    std::memcpy(&info, xex.data() + offset, sizeof(info));
    return info.title_id == kTitleId;
  }
  return false;
}

bool HasMagic(const fs::path& path, const char* a, const char* b = nullptr, const char* c = nullptr) {
  char magic[4] = {};
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  const bool read = std::fread(magic, 1, 4, f) == 4;
  std::fclose(f);
  if (!read) return false;
  for (const char* m : {a, b, c}) {
    if (m && std::memcmp(magic, m, 4) == 0) return true;
  }
  return false;
}

// Every file in a mounted package, depth first, with its path inside it.
void WalkFiles(rex::filesystem::Entry* entry, const fs::path& relative,
               std::vector<std::pair<rex::filesystem::Entry*, fs::path>>& out) {
  for (const auto& child : entry->children()) {
    const fs::path path = relative / child->name();
    if (child->attributes() & rex::filesystem::kFileAttributeDirectory) {
      WalkFiles(child.get(), path, out);
    } else {
      out.emplace_back(child.get(), path);
    }
  }
}

// Where a file's bytes are in its package, when the package is one file and
// the entry lists all of them (empty otherwise: the file is then read through
// the SDK's own reader).
std::vector<io::Extent> ExtentsOf(rex::filesystem::Entry* entry) {
  const auto& blocks = static_cast<rex::filesystem::StfsContainerEntry*>(entry)->block_list();
  std::vector<io::Extent> extents;
  extents.reserve(blocks.size());
  uint64_t bytes = 0;
  for (const auto& block : blocks) {
    if (block.file != 0) return {};
    extents.push_back({block.offset, block.length});
    bytes += block.length;
  }
  if (bytes != entry->size()) return {};
  return extents;
}

// The slow way, for a file ExtentsOf can't describe: 4 MB at a time through
// the SDK's reader and the file cache.
bool CopyThroughCache(rex::filesystem::Entry* entry, const fs::path& out_path,
                      const std::function<void(uint64_t)>& step) {
  rex::filesystem::File* raw = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kGenericRead, &raw) != X_STATUS_SUCCESS || !raw) return false;
  std::unique_ptr<rex::filesystem::File, CloseFile> file(raw);
  std::FILE* out = std::fopen(out_path.c_str(), "wb");
  if (!out) return false;
  std::vector<uint8_t> chunk(size_t(4) << 20);
  size_t done = 0;
  bool ok = true;
  while (ok && done < entry->size()) {
    size_t got = 0;
    const size_t want = std::min(chunk.size(), entry->size() - done);
    ok = file->ReadSync({chunk.data(), want}, done, &got) == X_STATUS_SUCCESS && got &&
         std::fwrite(chunk.data(), 1, got, out) == got;
    done += got;
    if (ok) step(got);
  }
  return (std::fclose(out) == 0) && ok && done == entry->size();
}

// A package (title update or DLC) unpacked whole into `destination`.
//
// Read and written past the file cache (gow2_ps5_io.h). Each file is written
// under a temporary name and renamed when it is whole, so a file that is there
// with its size is done: an unpacking that was interrupted goes on at the next
// start instead of starting again. A mark file says the whole package is there.
bool UnpackPackage(const fs::path& package, const fs::path& destination, const std::string& what,
                   std::vector<std::string>& notes, Progress* progress, fs::path& marker_out) {
  const fs::path marker = destination / ".gow2ps5_unpacked";
  marker_out = marker;
  std::error_code ec;
  if (fs::exists(marker, ec)) return true;
  const std::string name = package.filename().string();
  if (progress) progress->SetStep("Reading " + what);
  rex::filesystem::StfsContainerDevice device("\\Device\\Gow2Ps5Package", package);
  if (!device.Initialize()) {
    notes.push_back(what + " " + name + ": the package can't be read");
    return false;
  }
  rex::filesystem::Entry* root = device.ResolvePath("");
  if (!root) return false;
  std::vector<std::pair<rex::filesystem::Entry*, fs::path>> files;
  WalkFiles(root, {}, files);
  uint64_t total = 0;
  for (const auto& [entry, relative] : files) total += entry->size();
  if (progress) {
    progress->done_bytes = 0;
    progress->total_bytes = total;
    progress->SetStep("Unpacking " + what);
  }
  REXLOG_INFO("gow2 files: {} {}: unpacking {} files, {} MB, to {}", what, name, files.size(), total >> 20,
              destination.string());
  rex::FlushLogging();
  if (progress && progress->announce && total > (uint64_t(64) << 20)) {
    progress->announce(fmt::format("Gears of War 2: unpacking {} ({} MB). This is done the first time only.", what,
                                   total >> 20));
  }

  rex::filesystem::direct::Reader reader;
  const bool have_reader = reader.Open(package.c_str());
  io::ChunkBuffer buffer;
  const auto started = std::chrono::steady_clock::now();
  auto logged = started;
  uint64_t copied = 0, kept = 0, unpacked = 0;
  const auto step = [&](uint64_t bytes) {
    if (progress) progress->done_bytes += bytes;
    unpacked += bytes;
    const auto now = std::chrono::steady_clock::now();
    if (now - logged < std::chrono::seconds(10)) return;
    logged = now;
    const double seconds = std::chrono::duration<double>(now - started).count();
    REXLOG_INFO("gow2 files: {} {}: {} of {} MB after {:.0f} s ({:.1f} MB/s{}), screen drawn {} times", what, name,
                (kept + unpacked) >> 20, total >> 20, seconds, double(unpacked >> 10) / 1024.0 / seconds,
                reader.direct() ? ", past the file cache" : "", progress ? progress->draws.load() : 0);
    rex::FlushLogging();
  };
  for (const auto& [entry, relative] : files) {
    const fs::path out_path = destination / relative;
    const uint64_t size = entry->size();
    if (fs::is_regular_file(out_path, ec) && fs::file_size(out_path, ec) == size && !ec) {
      kept += size;  // (whole, from a start that was interrupted)
      if (progress) progress->done_bytes += size;
      continue;
    }
    fs::create_directories(out_path.parent_path(), ec);
    const fs::path temporary = out_path.string() + ".part";
    const std::vector<io::Extent> extents = have_reader ? ExtentsOf(entry) : std::vector<io::Extent>();
    const uint64_t before = progress ? progress->done_bytes.load() : 0, unpacked_before = unpacked;
    bool ok = (!extents.empty() || size == 0) && have_reader &&
              io::CopyExtents(reader, extents, temporary.c_str(), rex::filesystem::direct::kEnabled, buffer, step);
    if (!ok) {
      if (progress) progress->done_bytes = before;
      unpacked = unpacked_before;
      ok = CopyThroughCache(entry, temporary, step);
    }
    if (ok) {
      fs::rename(temporary, out_path, ec);
      ok = !ec;
    }
    if (!ok) {
      fs::remove(temporary, ec);
      notes.push_back(what + " " + name + ": " + relative.string() + " didn't copy whole (is the drive full?)");
      return false;
    }
    copied += size;
  }
  if (std::FILE* f = std::fopen(marker.c_str(), "w")) {
    std::fprintf(f, "%zu files, %llu bytes\n", files.size(), static_cast<unsigned long long>(total));
    std::fclose(f);
  }
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  notes.push_back(fmt::format("{} {} unpacked ({} files, {} MB: {} MB in {:.0f} s{}, {} MB from an earlier start)",
                              what, name, files.size(), total >> 20, copied >> 20, seconds,
                              reader.direct() ? ", past the file cache" : "", kept >> 20));
  return true;
}

// What the player gave us: the disc image and packages (title update, DLC), in
// install/ (subfolders included) and next to the app.
std::vector<fs::path> FindCandidates(const fs::path& app_root) {
  std::error_code ec;
  const fs::path install = app_root / "install";
  std::vector<fs::path> candidates;
  for (fs::recursive_directory_iterator it(install, fs::directory_options::skip_permission_denied, ec), end;
       !ec && it != end; it.increment(ec)) {
    if (it.depth() > 3) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file(ec) && it->path().filename().string().rfind(".gow2ps5", 0) != 0) {
      candidates.push_back(it->path());
    }
  }
  for (fs::directory_iterator it(app_root, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && Lower(it->path().extension().string()) == ".iso") {
      candidates.push_back(it->path());
    }
  }
  std::sort(candidates.begin(), candidates.end());
  return candidates;
}

// The candidates as the record has them: path, size, last written.
struct ListedFile {
  std::string path;
  uint64_t size = 0;
  int64_t time = 0;
  bool operator==(const ListedFile&) const = default;
};
std::vector<ListedFile> ListCandidates(const fs::path& app_root) {
  std::vector<ListedFile> listed;
  for (const fs::path& path : FindCandidates(app_root)) {
    std::error_code ec;
    ListedFile file;
    file.path = path.string();
    file.size = fs::file_size(path, ec);
    if (ec) continue;
    file.time = int64_t(fs::last_write_time(path, ec).time_since_epoch().count());
    if (ec) continue;
    listed.push_back(std::move(file));
  }
  return listed;
}

fs::path RecordPath(const fs::path& app_root) { return app_root / "game" / "prepared.txt"; }
fs::path UpdateDir(const fs::path& app_root) { return app_root / "game" / "update"; }

// An unpacked title update is usable when its default.xexp is the pinned one and it was unpacked whole.
bool UpdateDirReady(const fs::path& dir) {
  std::error_code ec;
  std::vector<uint8_t> xexp;
  return fs::exists(dir / ".gow2ps5_unpacked", ec) && ReadWholeFile(dir / "default.xexp", xexp, size_t(16) << 20) &&
         Sha256(xexp) == kXexpSha256;
}

}  // namespace

GameFiles PrepareGameFiles(const fs::path& app_root, const fs::path& content_root, Progress* progress) {
  GameFiles result;
  if (progress) progress->SetStep("Looking for your game files");
  std::error_code ec;
  const fs::path install = app_root / "install";
  fs::create_directories(install, ec);
  const std::vector<fs::path> candidates = FindCandidates(app_root);

  fs::path update_package;                      // the title update that holds the pinned default.xexp
  std::vector<fs::path> other_updates;          // title updates for another build of the disc
  std::vector<fs::path> dlc_packages;
  bool other_disc = false;
  for (const fs::path& path : candidates) {
    const std::string extension = Lower(path.extension().string());
    if (extension == ".iso") {
      if (progress) progress->SetStep("Checking " + path.filename().string());
      rex::filesystem::DiscImageDevice disc("\\Device\\Gow2Ps5Inspect", path);
      if (!disc.Initialize()) {
        result.notes.push_back(path.filename().string() + ": not an Xbox 360 disc image this can read");
        continue;
      }
      std::vector<uint8_t> image_xex;
      if (!ReadEntry(disc.ResolvePath("default.xex"), image_xex, size_t(64) << 20)) {
        result.notes.push_back(path.filename().string() + ": no default.xex in it");
        continue;
      }
      if (!IsGears2Xex(image_xex)) {
        result.notes.push_back(path.filename().string() + ": not a Gears of War 2 disc");
        continue;
      }
      if (Sha256(image_xex) != kXexSha256) {
        other_disc = true;
        result.notes.push_back(path.filename().string() +
                               ": Gears of War 2, but another release (this build is made from the US disc, 0.0.0.5)");
        continue;
      }
      if (result.iso.empty()) {
        result.iso = path;
        result.notes.push_back("Disc: " + path.filename().string());
      }
    } else if (HasMagic(path, "LIVE", "PIRS", "CON ")) {
      auto header = rex::filesystem::StfsContainerDevice::ReadPackageHeader(path);
      if (!header || header->metadata.execution_info.title_id != kTitleId) continue;
      const uint32_t type = uint32_t(rex::system::XContentType(header->metadata.content_type));
      if (type == uint32_t(rex::system::XContentType::kInstaller)) {
        if (!update_package.empty()) continue;
        if (progress) progress->SetStep("Reading the title update " + path.filename().string());
        rex::filesystem::StfsContainerDevice update("\\Device\\Gow2Ps5Update", path);
        std::vector<uint8_t> xexp;
        if (!update.Initialize() || !ReadEntry(update.ResolvePath("default.xexp"), xexp, size_t(16) << 20)) {
          result.notes.push_back(path.filename().string() + ": the title update can't be read");
          continue;
        }
        if (Sha256(xexp) == kXexpSha256) {
          update_package = path;
          result.notes.push_back("Title Update 6: " + path.filename().string());
        } else {
          other_updates.push_back(path);
          result.notes.push_back(path.filename().string() +
                                 ": a title update, but not the Title Update 6 this build is made for");
        }
      } else if (type == uint32_t(rex::system::XContentType::kMarketplaceContent)) {
        dlc_packages.push_back(path);
      }
    }
  }

  // Title Update 6 unpacked to game/update. What is there already (an earlier start) is used as it is when it is
  // whole and its patch has the right hash, so the package needn't stay in install/.
  const fs::path update_dir = UpdateDir(app_root);
  bool update_ready = UpdateDirReady(update_dir);
  if (!update_ready && !update_package.empty()) {
    fs::path marker;
    update_ready = UnpackPackage(update_package, update_dir, "Title Update 6", result.notes, progress, marker) &&
                   UpdateDirReady(update_dir);
  }
  if (update_ready) {
    result.update_dir = update_dir;
    result.markers.push_back(update_dir / ".gow2ps5_unpacked");
    std::error_code size_ec;
    if (!update_package.empty() && update_package.parent_path().string().rfind(install.string(), 0) == 0) {
      const uint64_t size = fs::file_size(update_package, size_ec);
      if (!size_ec) result.no_longer_needed.emplace_back(update_package, size);
    }
  }

  // DLC where the runtime's content manager looks for it:
  // <content root>/0000000000000000/<title id>/<content type>/<package file name>/
  bool dlc_whole = true;
  for (const fs::path& package : dlc_packages) {
    const fs::path destination = content_root / "0000000000000000" / fmt::format("{:08X}", kTitleId) /
                                 fmt::format("{:08X}", uint32_t(rex::system::XContentType::kMarketplaceContent)) /
                                 package.filename();
    fs::path marker;
    if (UnpackPackage(package, destination, "the add-on content", result.notes, progress, marker)) {
      result.markers.push_back(marker);
      std::error_code size_ec;
      const uint64_t size = fs::file_size(package, size_ec);
      if (!size_ec && package.parent_path().string().rfind(install.string(), 0) == 0) {
        result.no_longer_needed.emplace_back(package, size);
      }
    } else {
      dlc_whole = false;
    }
  }
  result.complete = result.ready() && dlc_whole;
  if (progress) {
    progress->done_bytes = 0;
    progress->total_bytes = 0;
    progress->SetStep("Starting the game");
  }

  if (!result.ready()) {
    if (result.iso.empty()) {
      result.problem =
          std::string(other_disc ? "The Gears of War 2 disc image found is another release than this build is made "
                                   "for.\n\n"
                                 : "No Gears of War 2 disc image was found.\n\n") +
          "Copy your own Gears of War 2 Xbox 360 disc image (.iso) - the US disc, version 0.0.0.5 - into this app's "
          "install folder:\n\n"
          "    " + install.string() + "\n\n"
          "Then start the app again.";
    } else if (update_package.empty()) {
      result.problem =
          std::string(other_updates.empty() ? "The disc image was found, but Title Update 6 is missing.\n\n"
                                            : "The disc image was found, but the title update there is not the "
                                              "Title Update 6 for this disc.\n\n") +
          "Copy the Gears of War 2 Title Update 6 package for the US disc (from the console's "
          "Content\\0000000000000000\\4D53082D\\000B0000\\ folder, as it is, no extension) into this app's install "
          "folder:\n\n"
          "    " + install.string() + "\n\n"
          "Then start the app again.";
    } else {
      result.problem =
          "The game's files were found but couldn't be prepared (is the drive full or read-only?).\n\n"
          "The log in this app's logs folder has the details.";
    }
  }
  return result;
}

bool LoadPreparedRecord(const fs::path& app_root, GameFiles& out) {
  std::FILE* f = std::fopen(RecordPath(app_root).c_str(), "r");
  if (!f) return false;
  GameFiles files;
  std::vector<ListedFile> listed;
  bool ok = false;
  char line[4096];
  while (std::fgets(line, sizeof(line), f)) {
    std::string text(line);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    std::vector<std::string> fields;
    size_t start = 0;
    for (size_t tab; (tab = text.find('\t', start)) != std::string::npos; start = tab + 1) {
      fields.push_back(text.substr(start, tab - start));
    }
    fields.push_back(text.substr(start));
    if (fields[0] == "gow2ps5-prepared" && fields.size() == 2 && fields[1] == "1") {
      ok = true;
    } else if (fields[0] == "disc" && fields.size() == 2) {
      files.iso = fields[1];
    } else if (fields[0] == "update" && fields.size() == 2) {
      files.update_dir = fields[1];
    } else if (fields[0] == "unpacked" && fields.size() == 2) {
      std::error_code ec;
      if (!fs::exists(fields[1], ec)) ok = false;
      files.markers.push_back(fields[1]);
    } else if (fields[0] == "file" && fields.size() == 4) {
      ListedFile file;
      file.size = std::strtoull(fields[1].c_str(), nullptr, 10);
      file.time = std::strtoll(fields[2].c_str(), nullptr, 10);
      file.path = fields[3];
      listed.push_back(std::move(file));
    }
  }
  std::fclose(f);
  // install/ as it was when recorded: the same files, sizes and times.
  if (!ok || listed != ListCandidates(app_root)) return false;
  std::error_code ec;
  if (files.iso.empty() || !fs::exists(files.iso, ec) || files.update_dir.empty() ||
      !fs::exists(files.update_dir / "default.xexp", ec)) {
    return false;
  }
  files.complete = true;
  files.notes.push_back("as prepared before (" + RecordPath(app_root).string() +
                        "): the install folder is as it was then");
  out = std::move(files);
  return true;
}

void SavePreparedRecord(const fs::path& app_root, const GameFiles& files) {
  const fs::path path = RecordPath(app_root);
  std::string text = "gow2ps5-prepared\t1\n";
  text += fmt::format("disc\t{}\n", files.iso.string());
  text += fmt::format("update\t{}\n", files.update_dir.string());
  for (const fs::path& marker : files.markers) text += fmt::format("unpacked\t{}\n", marker.string());
  for (const ListedFile& file : ListCandidates(app_root)) {
    text += fmt::format("file\t{}\t{}\t{}\n", file.size, file.time, file.path);
  }
  if (!WriteWholeFile(path, reinterpret_cast<const uint8_t*>(text.data()), text.size())) {
    REXLOG_WARN("gow2 files: {} couldn't be written: the next start looks through everything again", path.string());
  }
}

uint64_t DeleteNoLongerNeeded(const std::vector<std::pair<fs::path, uint64_t>>& files) {
  uint64_t freed = 0;
  for (const auto& [path, size] : files) {
    std::error_code ec;
    if (fs::remove(path, ec) && !ec) {
      freed += size;
      REXLOG_INFO("gow2 files: deleted {} ({} MB), no longer needed", path.string(), size >> 20);
    } else {
      REXLOG_WARN("gow2 files: {} couldn't be deleted ({})", path.string(), ec.message());
    }
  }
  return freed;
}

}  // namespace gow2ps5
