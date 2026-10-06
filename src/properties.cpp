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
std::string bus(STORAGE_BUS_TYPE b) {
  switch (b) {
  case BusTypeScsi:
    return "SCSI";
  case BusTypeAta:
    return "ATA";
  case BusTypeSata:
    return "SATA";
  case BusTypeNvme:
    return "NVMe";
  case BusTypeUsb:
    return "USB";
  case BusTypeSas:
    return "SAS";
  case BusTypeRAID:
    return "RAID";
  case BusTypeVirtual:
    return "virtual";
  case BusTypeFileBackedVirtual:
    return "file-backed virtual";
  case BusTypeSpaces:
    return "Storage Spaces";
  case BusTypeSd:
    return "SD";
  case BusTypeMmc:
    return "MMC";
  default:
    return "unknown (" + std::to_string(static_cast<unsigned>(b)) + ")";
  }
}
std::string descriptor_string(const std::vector<unsigned char> &b, DWORD off) {
  if (!off || off >= b.size())
    return {};
  auto begin = b.begin() + off;
  auto end = std::find(begin, b.end(), 0);
  if (end == b.end())
    return {};
  // STORAGE_DEVICE_DESCRIPTOR strings are ASCII. Replace non-ASCII rather than
  // emit invalid UTF-8.
  std::string s;
  for (auto it = begin; it != end; ++it)
    s += *it < 128 ? static_cast<char>(*it) : '?';
  while (!s.empty() && s.back() == ' ')
    s.pop_back();
  return s;
}
template <class T>
bool property(HANDLE h, STORAGE_PROPERTY_ID id, T &out, DWORD &err) {
  STORAGE_PROPERTY_QUERY q{};
  q.PropertyId = id;
  q.QueryType = PropertyStandardQuery;
  DWORD bytes = 0;
  if (!query(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &out, sizeof(out),
             bytes)) {
    err = GetLastError();
    return false;
  }
  if (bytes < sizeof(out)) {
    err = ERROR_INVALID_DATA;
    return false;
  }
  return true;
}
void properties(HANDLE h, Disk &d) {
  STORAGE_PROPERTY_QUERY q{};
  q.PropertyId = StorageDeviceProperty;
  q.QueryType = PropertyStandardQuery;
  STORAGE_DESCRIPTOR_HEADER head{};
  DWORD bytes = 0;
  if (query(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &head, sizeof(head),
            bytes) &&
      bytes >= sizeof(head) && head.Size >= sizeof(STORAGE_DEVICE_DESCRIPTOR) &&
      head.Size <= size_t{1024} * 1024) {
    std::vector<unsigned char> buf(head.Size);
    if (query(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf.data(),
              static_cast<DWORD>(buf.size()), bytes) &&
        bytes >= sizeof(STORAGE_DEVICE_DESCRIPTOR) && bytes <= buf.size()) {
      STORAGE_DEVICE_DESCRIPTOR desc{};
      std::memcpy(&desc, buf.data(), sizeof(desc));
      buf.resize(bytes);
      d.model = descriptor_string(buf, desc.VendorIdOffset) + " " +
                descriptor_string(buf, desc.ProductIdOffset);
      d.serial = descriptor_string(buf, desc.SerialNumberOffset);
      d.bus = bus(desc.BusType);
      d.removable = desc.RemovableMedia != FALSE;
    } else
      d.health.push_back({"StorageDeviceProperty", "unavailable",
                          "Device descriptor unavailable", GetLastError()});
  } else
    d.health.push_back({"StorageDeviceProperty", "unavailable",
                        "Device descriptor unavailable", GetLastError()});
  GET_LENGTH_INFORMATION length{};
  if (query(h, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length),
            bytes) &&
      bytes >= sizeof(length) && length.Length.QuadPart >= 0)
    d.capacity = static_cast<std::uint64_t>(length.Length.QuadPart);
  if (!d.capacity) {
    std::vector<unsigned char> geometry(4096);
    if (query(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, geometry.data(),
              static_cast<DWORD>(geometry.size()), bytes) &&
        bytes >= offsetof(DISK_GEOMETRY_EX, Data)) {
      DISK_GEOMETRY_EX g{};
      std::memcpy(&g, geometry.data(), offsetof(DISK_GEOMETRY_EX, Data));
      if (g.DiskSize.QuadPart >= 0)
        d.capacity = static_cast<std::uint64_t>(g.DiskSize.QuadPart);
    }
  }
  DWORD err = 0;
  DEVICE_TRIM_DESCRIPTOR trim{};
  if (property(h, StorageDeviceTrimProperty, trim, err))
    d.trim = trim.TrimEnabled != FALSE;
  DEVICE_SEEK_PENALTY_DESCRIPTOR seek{};
  if (property(h, StorageDeviceSeekPenaltyProperty, seek, err))
    d.media =
        seek.IncursSeekPenalty
            ? "seek penalty reported (rotational-like)"
            : "no seek penalty reported (not definitive media identification)";
  health(h, d);
}
} // namespace wsh::detail
