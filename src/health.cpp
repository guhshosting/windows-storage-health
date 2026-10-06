#include "storage_internal.hpp"
#include <ntddstor.h>
namespace wsh::detail {
void health(HANDLE h, Disk &d) {
  DWORD bytes = 0;
  STORAGE_PREDICT_FAILURE prediction{};
  const bool returned = query(h, IOCTL_STORAGE_PREDICT_FAILURE, nullptr, 0,
                              &prediction, sizeof(prediction), bytes);
  const DWORD failure = returned ? ERROR_INVALID_DATA : GetLastError();
  if (returned && bytes >= sizeof(prediction)) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string raw =
        "PredictFailure=" + std::to_string(prediction.PredictFailure) +
        "; VendorSpecific=";
    for (auto b : prediction.VendorSpecific) {
      raw += hex[static_cast<unsigned>(b) >> 4U];
      raw += hex[static_cast<unsigned>(b) & 15U];
    }
    d.health.push_back({"IOCTL_STORAGE_PREDICT_FAILURE",
                        prediction.PredictFailure ? "unhealthy" : "healthy",
                        raw,
                        {}});
  } else
    d.health.push_back({"IOCTL_STORAGE_PREDICT_FAILURE", "unavailable",
                        "Failure prediction not returned; no healthy inference",
                        failure});
}
} // namespace wsh::detail
