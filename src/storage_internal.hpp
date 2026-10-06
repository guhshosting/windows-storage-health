#pragma once
#include "wsh.hpp"
#include <memory>
#include <windows.h>
#include <winioctl.h>
namespace wsh::detail {
struct Close {
  void operator()(void *h) const noexcept {
    if (h && h != INVALID_HANDLE_VALUE)
      CloseHandle(h);
  }
};
using Handle = std::unique_ptr<void, Close>;
Handle open(const std::wstring &path, DWORD access = 0);
bool valid(const Handle &h);
bool query(HANDLE h, DWORD code, void *in, DWORD ni, void *out, DWORD no,
           DWORD &bytes);
std::string error(const char *source, DWORD e);
std::string guid(const GUID &g);
void dirty_state(const std::wstring &device, Volume &v,
                 Handle (*open_volume)(const std::wstring &, DWORD) = open,
                 bool (*control)(HANDLE, DWORD, void *, DWORD, void *, DWORD,
                                 DWORD &) = query);
void properties(HANDLE h, Disk &d);
void health(HANDLE h, Disk &d);
std::vector<Disk> enumerate_disks(std::vector<std::string> &warnings);
std::vector<Volume> enumerate_volumes(std::vector<std::string> &warnings);
} // namespace wsh::detail
