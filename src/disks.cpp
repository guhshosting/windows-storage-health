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
struct DeviceSet {
  HDEVINFO h;
  ~DeviceSet() {
    if (h != INVALID_HANDLE_VALUE)
      SetupDiDestroyDeviceInfoList(h);
  }
};
std::vector<Disk> enumerate_disks(std::vector<std::string> &warnings) {
  DeviceSet set{SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr,
                                     DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
  if (set.h == INVALID_HANDLE_VALUE)
    throw std::system_error(static_cast<int>(GetLastError()),
                            std::system_category(), "Disk enumeration");
  std::vector<Disk> disks;
  for (DWORD i = 0;; ++i) {
    SP_DEVICE_INTERFACE_DATA iface{};
    iface.cbSize = sizeof(iface);
    if (!SetupDiEnumDeviceInterfaces(set.h, nullptr, &GUID_DEVINTERFACE_DISK, i,
                                     &iface)) {
      DWORD e = GetLastError();
      if (e == ERROR_NO_MORE_ITEMS)
        break;
      throw std::system_error(static_cast<int>(e), std::system_category(),
                              "Disk interfaces");
    }
    DWORD needed = 0;
    SetupDiGetDeviceInterfaceDetailW(set.h, &iface, nullptr, 0, &needed,
                                     nullptr);
    if (needed < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) ||
        needed > size_t{1024} * 1024) {
      warnings.push_back("Invalid disk interface size");
      continue;
    }
    std::vector<unsigned char> buf(needed);
    auto detail =
        reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buf.data());
    detail->cbSize = sizeof(*detail);
    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    if (!SetupDiGetDeviceInterfaceDetailW(set.h, &iface, detail, needed,
                                          nullptr, &info)) {
      warnings.push_back(error("Disk interface detail", GetLastError()));
      continue;
    }
    auto h = open(detail->DevicePath);
    if (!valid(h)) {
      warnings.push_back(error("Disk query handle", GetLastError()));
      continue;
    }
    STORAGE_DEVICE_NUMBER number{};
    DWORD bytes = 0;
    if (!query(h.get(), IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &number,
               sizeof(number), bytes) ||
        bytes < sizeof(number)) {
      warnings.push_back(error("Disk number", GetLastError()));
      continue;
    }
    if (number.DeviceType != FILE_DEVICE_DISK)
      continue;
    if (std::any_of(disks.begin(), disks.end(), [&](const Disk &d) {
          return d.number == number.DeviceNumber;
        }))
      continue;
    Disk d;
    d.number = number.DeviceNumber;
    wchar_t instance[4096]{};
    if (SetupDiGetDeviceInstanceIdW(set.h, &info, instance, 4096, nullptr))
      d.instance = utf8(instance);
    properties(h.get(), d);
    auto layoutHandle = open(detail->DevicePath);
    std::vector<unsigned char> layout(size_t{1024} * 1024);
    if (valid(layoutHandle) &&
        query(layoutHandle.get(), IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0,
              layout.data(), static_cast<DWORD>(layout.size()), bytes) &&
        bytes >= offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) {
      DRIVE_LAYOUT_INFORMATION_EX header{};
      std::memcpy(&header, layout.data(),
                  offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry));
      if (header.PartitionCount <=
          (bytes - offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)) /
              sizeof(PARTITION_INFORMATION_EX)) {
        for (DWORD p = 0; p < header.PartitionCount; ++p) {
          PARTITION_INFORMATION_EX part{};
          std::memcpy(
              &part,
              layout.data() +
                  offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry) +
                  p * sizeof(part),
              sizeof(part));
          if (!part.PartitionNumber || part.PartitionLength.QuadPart <= 0 ||
              part.StartingOffset.QuadPart < 0)
            continue;
          std::string style = part.PartitionStyle == PARTITION_STYLE_GPT ? "GPT"
                              : part.PartitionStyle == PARTITION_STYLE_MBR
                                  ? "MBR"
                                  : "RAW";
          std::string identity =
              part.PartitionStyle == PARTITION_STYLE_GPT
                  ? guid(part.Gpt.PartitionId)
                  : "MBR signature=" + std::to_string(header.Mbr.Signature) +
                        "; offset=" +
                        std::to_string(part.StartingOffset.QuadPart);
          d.partitions.push_back(
              {part.PartitionNumber, style, identity,
               static_cast<std::uint64_t>(part.StartingOffset.QuadPart),
               static_cast<std::uint64_t>(part.PartitionLength.QuadPart)});
        }
      } else
        warnings.push_back("Malformed partition layout on disk " +
                           std::to_string(d.number));
    } else
      warnings.push_back("Partition layout disk " + std::to_string(d.number) +
                         ": Win32 error " + std::to_string(GetLastError()));
    // Length IOCTL requires read access on some systems; retry with only
    // GENERIC_READ.
    if (!d.capacity) {
      auto read = open(detail->DevicePath, GENERIC_READ);
      GET_LENGTH_INFORMATION length{};
      if (valid(read) &&
          query(read.get(), IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length,
                sizeof(length), bytes) &&
          bytes >= sizeof(length) && length.Length.QuadPart >= 0)
        d.capacity = static_cast<std::uint64_t>(length.Length.QuadPart);
    }
    disks.push_back(std::move(d));
  }
  std::sort(disks.begin(), disks.end(),
            [](const Disk &a, const Disk &b) { return a.number < b.number; });
  return disks;
}
} // namespace wsh::detail
