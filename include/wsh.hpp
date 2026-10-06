#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace wsh {
struct Evidence {
  std::string source, state, raw;
  std::optional<unsigned long> error;
};
struct Partition {
  unsigned long number = 0;
  std::string style, identity;
  std::uint64_t offset = 0, length = 0;
};
struct Disk {
  unsigned long number = 0;
  std::string instance, model, serial, bus = "unknown", media = "unavailable";
  std::optional<bool> removable, trim;
  std::optional<std::uint64_t> capacity;
  std::vector<Evidence> health;
  std::vector<Partition> partitions;
};
struct Extent {
  unsigned long disk;
  std::uint64_t offset, length;
};
struct Volume {
  std::string id, label, filesystem;
  std::vector<std::string> mounts;
  std::optional<std::uint64_t> capacity, free, available;
  std::optional<bool> dirty;
  std::vector<Extent> extents;
  std::vector<std::string> warnings;
};
struct Report {
  std::vector<Disk> disks;
  std::vector<Volume> volumes;
  std::vector<std::string> warnings, findings, limitations;
};
struct Options {
  bool all = false, json = false, help = false, version = false;
  std::optional<unsigned long> disk;
};
Options parse(const std::vector<std::wstring> &args);
std::string utf8(const std::wstring &value);
Report acquire();
void analyze(Report &report);
void select_disk(Report &report, unsigned long number);
std::string json(const Report &report);
std::string human(const Report &report, bool all);
} // namespace wsh
