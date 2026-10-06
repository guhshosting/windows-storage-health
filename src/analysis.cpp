#include "wsh.hpp"
#include <algorithm>
#include <stdexcept>
#include <windows.h>
namespace wsh {
void analyze(Report &r) {
  r.findings.clear();
  for (const auto &d : r.disks)
    for (const auto &e : d.health)
      if (e.state == "unhealthy" || e.state == "warning")
        r.findings.push_back("Disk " + std::to_string(d.number) + ": " +
                             e.state + " reported by " + e.source);
  for (const auto &v : r.volumes) {
    if (v.capacity && v.free && *v.capacity &&
        static_cast<long double>(*v.free) /
                static_cast<long double>(*v.capacity) <
            0.10L)
      r.findings.push_back(v.id +
                           ": capacity warning: less than 10% free "
                           "(informational threshold, not physical health)");
    if (v.dirty && *v.dirty)
      r.findings.push_back(v.id +
                           ": filesystem dirty flag set; this is filesystem "
                           "state, not a physical-health diagnosis");
  }
}
} // namespace wsh
