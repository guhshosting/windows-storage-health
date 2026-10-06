#include "wsh.hpp"
#include <algorithm>
#include <stdexcept>
#include <windows.h>
namespace wsh {
void select_disk(Report &r, unsigned long n) {
  if (std::none_of(r.disks.begin(), r.disks.end(),
                   [n](const Disk &d) { return d.number == n; }))
    throw std::out_of_range("Requested disk not found");
  r.disks.erase(std::remove_if(r.disks.begin(), r.disks.end(),
                               [n](const Disk &d) { return d.number != n; }),
                r.disks.end());
  r.volumes.erase(std::remove_if(r.volumes.begin(), r.volumes.end(),
                                 [n](const Volume &v) {
                                   return std::none_of(v.extents.begin(),
                                                       v.extents.end(),
                                                       [n](const Extent &e) {
                                                         return e.disk == n;
                                                       });
                                 }),
                  r.volumes.end());
}
} // namespace wsh
