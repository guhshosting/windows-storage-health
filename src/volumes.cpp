#include "storage_internal.hpp"
#include "wsh.hpp"
#include <algorithm>
#include <cstring>
#include <devguid.h>
#include <memory>
#include <ntddstor.h>
#include <setupapi.h>
#include <system_error>
#include <windows.h>
#include <winioctl.h>
namespace wsh::detail {
void dirty_state(const std::wstring &device, Volume &v,
                 Handle (*open_volume)(const std::wstring &, DWORD),
                 bool (*control)(HANDLE, DWORD, void *, DWORD, void *, DWORD,
                                 DWORD &)) {
  v.dirty.reset();
  // A query-only device open can bypass the mounted filesystem. The FSCTL
  // requires a user volume open, not a device or root-directory handle.
  auto read = open_volume(device, GENERIC_READ);
  if (!valid(read)) {
    v.warnings.push_back(error("Filesystem dirty state unavailable (volume "
                               "open, GENERIC_READ)", GetLastError()));
    return;
  }
  DWORD dirty = 0, bytes = 0;
  if (!control(read.get(), FSCTL_IS_VOLUME_DIRTY, nullptr, 0, &dirty,
               sizeof(dirty), bytes)) {
    const DWORD e = GetLastError();
    v.warnings.push_back(error(
        e == ERROR_INVALID_FUNCTION || e == ERROR_NOT_SUPPORTED
            ? "Filesystem dirty state unavailable (FSCTL_IS_VOLUME_DIRTY "
              "unsupported by filesystem/volume stack)"
            : "Filesystem dirty state unavailable (FSCTL_IS_VOLUME_DIRTY)", e));
  } else if (bytes < sizeof(dirty)) {
    v.warnings.push_back("Filesystem dirty state unavailable "
                         "(FSCTL_IS_VOLUME_DIRTY truncated response: " +
                         std::to_string(bytes) + " bytes; expected 4)");
  } else {
    v.dirty = (dirty & VOLUME_IS_DIRTY) != 0;
  }
}
struct VolumeSearch {
  HANDLE h;
  ~VolumeSearch() {
    if (h != INVALID_HANDLE_VALUE)
      FindVolumeClose(h);
  }
};
std::vector<Volume> enumerate_volumes(std::vector<std::string> &warnings) {
  std::vector<wchar_t> name(32768);
  VolumeSearch search{
      FindFirstVolumeW(name.data(), static_cast<DWORD>(name.size()))};
  if (search.h == INVALID_HANDLE_VALUE) {
    DWORD e = GetLastError();
    if (e == ERROR_NO_MORE_FILES)
      return {};
    throw std::system_error(static_cast<int>(e), std::system_category(),
                            "Volume enumeration");
  }
  std::vector<Volume> volumes;
  do {
    const std::wstring path = name.data();
    UINT type = GetDriveTypeW(path.c_str());
    if (type == DRIVE_REMOTE) {
      warnings.push_back("Remote volume skipped");
      continue;
    }
    if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE &&
        type != DRIVE_RAMDISK) {
      warnings.push_back("Non-local-disk volume skipped: " + utf8(path));
      continue;
    }
    Volume v;
    v.id = utf8(path);
    DWORD needed = 0;
    std::vector<wchar_t> mounts(32768);
    if (GetVolumePathNamesForVolumeNameW(path.c_str(), mounts.data(),
                                         static_cast<DWORD>(mounts.size()),
                                         &needed)) {
      size_t pos = 0;
      while (pos < mounts.size() && mounts[pos]) {
        std::wstring m = mounts.data() + pos;
        pos += m.size() + 1;
        v.mounts.push_back(utf8(m));
      }
    } else
      v.warnings.push_back(error("Mount points", GetLastError()));
    wchar_t label[261]{}, fs[261]{};
    DWORD serial = 0, maxcomp = 0, flags = 0;
    if (GetVolumeInformationW(path.c_str(), label, 261, &serial, &maxcomp,
                              &flags, fs, 261)) {
      v.label = utf8(label);
      v.filesystem = utf8(fs);
    } else
      v.warnings.push_back(error("Filesystem information", GetLastError()));
    ULARGE_INTEGER available{}, total{}, free{};
    if (GetDiskFreeSpaceExW(path.c_str(), &available, &total, &free)) {
      v.capacity = total.QuadPart;
      v.free = free.QuadPart;
      v.available = available.QuadPart;
    } else
      v.warnings.push_back(error("Free space", GetLastError()));
    std::wstring device = path;
    if (!device.empty() && device.back() == L'\\')
      device.pop_back();
    auto h = open(device);
    if (valid(h)) {
      std::vector<unsigned char> buf(size_t{1024} * 1024);
      DWORD bytes = 0;
      if (query(h.get(), IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0,
                buf.data(), static_cast<DWORD>(buf.size()), bytes) &&
          bytes >= offsetof(VOLUME_DISK_EXTENTS, Extents)) {
        DWORD count = 0;
        std::memcpy(&count, buf.data(), sizeof(count));
        if (count <= (bytes - offsetof(VOLUME_DISK_EXTENTS, Extents)) /
                         sizeof(DISK_EXTENT)) {
          for (DWORD i = 0; i < count; ++i) {
            DISK_EXTENT e{};
            std::memcpy(&e,
                        buf.data() + offsetof(VOLUME_DISK_EXTENTS, Extents) +
                            i * sizeof(e),
                        sizeof(e));
            if (e.StartingOffset.QuadPart >= 0 && e.ExtentLength.QuadPart >= 0)
              v.extents.push_back(
                  {e.DiskNumber,
                   static_cast<std::uint64_t>(e.StartingOffset.QuadPart),
                   static_cast<std::uint64_t>(e.ExtentLength.QuadPart)});
          }
        } else
          v.warnings.push_back("Malformed disk extent response");
      } else
        v.warnings.push_back(error("Volume disk extents", GetLastError()));
    } else
      v.warnings.push_back(error("Volume query handle", GetLastError()));
    dirty_state(device, v);
    volumes.push_back(std::move(v));
  } while (
      FindNextVolumeW(search.h, name.data(), static_cast<DWORD>(name.size())));
  if (GetLastError() != ERROR_NO_MORE_FILES)
    throw std::system_error(static_cast<int>(GetLastError()),
                            std::system_category(),
                            "Volume enumeration continuation");
  return volumes;
}
} // namespace wsh::detail
