#include "wsh.hpp"
#include <iomanip>
#include <sstream>
namespace wsh {
namespace {
std::string quote(const std::string &s) {
  std::ostringstream o;
  o << '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      o << "\\\"";
      break;
    case '\\':
      o << "\\\\";
      break;
    case '\n':
      o << "\\n";
      break;
    case '\r':
      o << "\\r";
      break;
    case '\t':
      o << "\\t";
      break;
    default:
      if (c < 32)
        o << "\\u" << std::hex << std::setw(4) << std::setfill('0')
          << static_cast<unsigned>(c) << std::dec;
      else
        o << static_cast<char>(c);
    }
  }
  o << '"';
  return o.str();
}
template <class T> std::string value(const std::optional<T> &v) {
  return v ? std::to_string(*v) : "null";
}
std::string boolean(const std::optional<bool> &v) {
  return v ? (*v ? "true" : "false") : "null";
}
std::string strings(const std::vector<std::string> &a) {
  std::string s = "[";
  for (size_t i = 0; i < a.size(); ++i) {
    if (i)
      s += ',';
    s += quote(a[i]);
  }
  return s + "]";
}
} // namespace
std::string json(const Report &r) {
  std::ostringstream o;
  o << "{\"schema_version\":\"1.0\",\"application_version\":\"1.0.0\","
       "\"physical_disks\":[";
  for (size_t i = 0; i < r.disks.size(); ++i) {
    if (i)
      o << ',';
    const auto &d = r.disks[i];
    o << "{\"disk_number\":" << d.number
      << ",\"device_instance_id\":" << quote(d.instance)
      << ",\"model\":" << quote(d.model) << ",\"serial\":" << quote(d.serial)
      << ",\"bus_type\":" << quote(d.bus)
      << ",\"media_context\":" << quote(d.media)
      << ",\"removable\":" << boolean(d.removable)
      << ",\"capacity_bytes\":" << value(d.capacity)
      << ",\"trim_enabled\":" << boolean(d.trim) << '}';
  }
  o << "],\"volumes\":[";
  for (size_t i = 0; i < r.volumes.size(); ++i) {
    if (i)
      o << ',';
    const auto &v = r.volumes[i];
    o << "{\"id\":" << quote(v.id) << ",\"label\":" << quote(v.label)
      << ",\"filesystem\":" << quote(v.filesystem)
      << ",\"mount_points\":" << strings(v.mounts)
      << ",\"capacity_bytes\":" << value(v.capacity)
      << ",\"free_bytes\":" << value(v.free)
      << ",\"caller_available_bytes\":" << value(v.available)
      << ",\"filesystem_dirty\":" << boolean(v.dirty)
      << ",\"warnings\":" << strings(v.warnings) << '}';
  }
  o << "],\"partitions\":[";
  bool first = true;
  for (const auto &d : r.disks)
    for (const auto &p : d.partitions) {
      if (!first)
        o << ',';
      first = false;
      o << "{\"disk_number\":" << d.number
        << ",\"partition_number\":" << p.number
        << ",\"style\":" << quote(p.style)
        << ",\"identity\":" << quote(p.identity)
        << ",\"offset_bytes\":" << p.offset << ",\"length_bytes\":" << p.length
        << '}';
    }
  o << "],\"relationships\":[";
  first = true;
  for (const auto &v : r.volumes)
    for (const auto &e : v.extents) {
      if (!first)
        o << ',';
      first = false;
      o << "{\"volume_id\":" << quote(v.id) << ",\"disk_number\":" << e.disk
        << ",\"offset_bytes\":" << e.offset << ",\"length_bytes\":" << e.length
        << ",\"partition_number\":";
      std::optional<unsigned long> part;
      for (const auto &d : r.disks)
        if (d.number == e.disk)
          for (const auto &p : d.partitions)
            if (p.offset == e.offset && p.length == e.length)
              part = p.number;
      o << value(part) << '}';
    }
  o << "],\"health_evidence\":[";
  first = true;
  for (const auto &d : r.disks)
    for (const auto &e : d.health) {
      if (!first)
        o << ',';
      first = false;
      o << "{\"disk_number\":" << d.number << ",\"source\":" << quote(e.source)
        << ",\"state\":" << quote(e.state) << ",\"raw\":" << quote(e.raw)
        << ",\"win32_error\":" << value(e.error) << '}';
    }
  o << "],\"event_evidence\":[],\"findings\":" << strings(r.findings)
    << ",\"warnings\":" << strings(r.warnings)
    << ",\"limitations\":" << strings(r.limitations) << "}\n";
  return o.str();
}
} // namespace wsh
