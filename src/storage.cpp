#include "storage_internal.hpp"
namespace wsh {
Report acquire() {
  Report r;
  r.disks = detail::enumerate_disks(r.warnings);
  r.volumes = detail::enumerate_volumes(r.warnings);
  // Intentional adjacent literals keep long explanatory strings readable.
  // NOLINTBEGIN(bugprone-suspicious-missing-comma)
  r.limitations = {
      "Virtual machines/VPS commonly expose generic virtual disks without "
      "physical health telemetry. Unavailable telemetry does not mean healthy.",
      "Failure prediction is Windows-reported evidence at query time, not a "
      "guarantee against failure; vendor bytes are preserved without "
      "interpretation.",
      "No vendor SMART passthrough, life prediction, event-log acquisition, or "
      "Storage Spaces/RAID member health assessment is implemented.",
      "Media classification uses seek-penalty context only; no definitive "
      "SSD/HDD inference.",
      "Volume extents describe disk ranges. Partition matches use identical "
      "ranges only; dynamic and spanned volumes may have no partition match.",
      "Access-denied and unsupported per-device queries remain visible. "
      "Network drives are excluded. Capacity thresholds describe space only."};
  // NOLINTEND(bugprone-suspicious-missing-comma)
  return r;
}
} // namespace wsh
