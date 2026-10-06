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
Handle open(const std::wstring &path, DWORD access) {
  if (access != 0 && access != GENERIC_READ) {
    SetLastError(ERROR_ACCESS_DENIED);
    return Handle(INVALID_HANDLE_VALUE);
  }
  return Handle(
      CreateFileW(path.c_str(), access,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr));
}
bool valid(const Handle &h) {
  return h.get() != INVALID_HANDLE_VALUE && h.get() != nullptr;
}
std::string guid(const GUID &g) {
  char s[40]{};
  sprintf_s(s, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", g.Data1,
            g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
            g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
  return s;
}
bool query(HANDLE h, DWORD code, void *in, DWORD ni, void *out, DWORD no,
           DWORD &bytes) {
  // Explicit read-only allowlist. No passthrough or mutating control codes.
  switch (code) {
  case IOCTL_STORAGE_QUERY_PROPERTY:
  case IOCTL_STORAGE_GET_DEVICE_NUMBER:
  case IOCTL_STORAGE_PREDICT_FAILURE:
  case IOCTL_DISK_GET_LENGTH_INFO:
  case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX:
  case IOCTL_DISK_GET_DRIVE_LAYOUT_EX:
  case IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS:
  case FSCTL_IS_VOLUME_DIRTY:
    break;
  default:
    SetLastError(ERROR_INVALID_FUNCTION);
    return false;
  }
  return DeviceIoControl(h, code, in, ni, out, no, &bytes, nullptr) != FALSE;
}
std::string error(const char *source, DWORD e) {
  return std::string(source) + ": Win32 error " + std::to_string(e);
}
} // namespace wsh::detail
