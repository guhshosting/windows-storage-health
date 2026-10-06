#include "wsh.hpp"
#include <iomanip>
#include <sstream>
namespace wsh {
namespace {
std::string boolean(const std::optional<bool> &v) {
  return v ? (*v ? "true" : "false") : "null";
}
std::string size(const std::optional<std::uint64_t> &v) {
  if (!v)
    return "unavailable";
  std::ostringstream o;
  o << std::fixed << std::setprecision(2)
    << static_cast<long double>(*v) / 1073741824.0L << " GiB";
  return o.str();
}
std::string safe(const std::string &s) {
  std::string o;
  for (unsigned char c : s)
    o += c < 32 || c == 127 ? '?' : static_cast<char>(c);
  return o;
}
} // namespace
std::string human(const Report &r, bool all) {
  std::ostringstream o;
  o << "windows-storage-health 1.0.0\nStorage summary: " << r.disks.size()
    << " disk(s), " << r.volumes.size() << " local volume(s)\n";
  for (const auto &d : r.disks) {
    o << "Disk " << d.number << ": " << safe(d.model) << " | " << d.bus << " | "
      << size(d.capacity) << '\n';
    for (const auto &e : d.health) {
      o << "  " << e.source << ": " << e.state;
      if (e.error)
        o << " (Win32 " << *e.error << ')';
      o << '\n';
      if (all)
        o << "    " << safe(e.raw) << '\n';
    }
    if (all)
      for (const auto &p : d.partitions)
        o << "  Partition " << p.number << " (" << p.style << "): " << p.offset
          << " + " << p.length << " bytes; " << p.identity << '\n';
    if (all)
      o << "  Instance: " << safe(d.instance)
        << "\n  Serial: " << safe(d.serial) << "\n  Media context: " << d.media
        << "\n  Removable: " << boolean(d.removable)
        << "; TRIM capability: " << boolean(d.trim) << '\n';
  }
  o << "Volumes/capacity:\n";
  for (const auto &v : r.volumes) {
    o << "  " << (v.mounts.empty() ? safe(v.id) : safe(v.mounts.front())) << " "
      << safe(v.label) << " [" << safe(v.filesystem) << "] " << size(v.free)
      << " free / " << size(v.capacity) << "; disks:";
    if (v.extents.empty())
      o << " unavailable";
    for (const auto &e : v.extents)
      o << ' ' << e.disk;
    o << '\n';
    if (all) {
      o << "    Identity: " << safe(v.id) << "; dirty: " << boolean(v.dirty)
        << "; caller available: " << size(v.available) << '\n';
      for (const auto &m : v.mounts)
        o << "    Mount: " << safe(m) << '\n';
    }
    for (const auto &w : v.warnings)
      o << "    Warning: " << w << '\n';
  }
  o << "Findings:\n";
  if (r.findings.empty())
    o << "  No findings from available evidence; missing telemetry remains "
         "unknown.\n";
  for (const auto &f : r.findings)
    o << "  " << safe(f) << '\n';
  for (const auto &w : r.warnings)
    o << "Warning: " << w << '\n';
  o << "Limitations:\n";
  for (const auto &l : r.limitations)
    o << "  " << l << '\n';
  return o.str();
}
} // namespace wsh
