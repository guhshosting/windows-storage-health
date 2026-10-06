#include "wsh.hpp"
#include "../src/storage_internal.hpp"
#include <cstdio>
#include <iostream>
#include <stdexcept>
void check(bool b) {
  if (!b)
    throw std::runtime_error("Test assertion failed");
}
namespace {
DWORD open_error = 0, query_error = 0, returned_bytes = 4, returned_flags = 0;
bool queried = false;
constexpr wchar_t volume_device[] = L"\\\\?\\Volume{00000000-0000-0000-0000-000000000001}";
wsh::detail::Handle test_open(const std::wstring &path, DWORD access) {
  check(path == volume_device && path.back() != L'\\');
  check(access == GENERIC_READ);
  if (open_error != 0) {
    SetLastError(open_error);
    return wsh::detail::Handle(INVALID_HANDLE_VALUE);
  }
  // Real disposable kernel handle for RAII; the injected control never uses it.
  return wsh::detail::Handle(CreateEventW(nullptr, FALSE, FALSE, nullptr));
}
bool test_control(HANDLE h, DWORD code, void *in, DWORD ni, void *out, DWORD no,
                  DWORD &bytes) {
  check(h != nullptr && code == FSCTL_IS_VOLUME_DIRTY && in == nullptr &&
        ni == 0 && no == sizeof(DWORD));
  queried = true;
  *static_cast<DWORD *>(out) = returned_flags;
  bytes = returned_bytes;
  SetLastError(query_error);
  return query_error == 0;
}
void test_dirty_state() {
  for (DWORD flags : {DWORD{0}, DWORD{VOLUME_IS_DIRTY},
                      DWORD{VOLUME_SESSION_OPEN}, DWORD{VOLUME_IS_DIRTY | 8}}) {
    wsh::Volume v;
    returned_flags = flags;
    wsh::detail::dirty_state(volume_device, v, test_open, test_control);
    check(v.dirty && *v.dirty == ((flags & VOLUME_IS_DIRTY) != 0) &&
          v.warnings.empty());
  }
  for (DWORD e : {DWORD{ERROR_INVALID_FUNCTION}, DWORD{ERROR_NOT_SUPPORTED},
                  DWORD{ERROR_ACCESS_DENIED}, DWORD{ERROR_NOT_READY}}) {
    wsh::Volume v;
    v.dirty = false;
    query_error = e;
    wsh::detail::dirty_state(volume_device, v, test_open, test_control);
    check(!v.dirty && v.warnings.size() == 1 &&
          v.warnings.front().find("Win32 error " + std::to_string(e)) !=
              std::string::npos);
    check((v.warnings.front().find("unsupported") != std::string::npos) ==
          (e == ERROR_INVALID_FUNCTION || e == ERROR_NOT_SUPPORTED));
  }
  query_error = 0;
  for (DWORD bytes : {DWORD{0}, DWORD{1}, DWORD{3}}) {
    wsh::Volume v;
    returned_bytes = bytes;
    SetLastError(ERROR_INVALID_FUNCTION);
    wsh::detail::dirty_state(volume_device, v, test_open, test_control);
    check(!v.dirty && v.warnings.front().find("truncated") != std::string::npos &&
          v.warnings.front().find("Win32 error") == std::string::npos);
  }
  returned_bytes = 4;
  open_error = ERROR_ACCESS_DENIED;
  queried = false;
  wsh::Volume v;
  wsh::detail::dirty_state(volume_device, v, test_open, test_control);
  check(!v.dirty && !queried && v.warnings.front().find("volume open") !=
                                   std::string::npos);
  open_error = 0;
}
} // namespace
int main(int argc, char **argv) {
  try {
    test_dirty_state();
    auto opts = wsh::parse({L"--disk", L"4294967295", L"--json", L"--all"});
    check(opts.disk && *opts.disk == 4294967295UL && opts.json && opts.all);
    for (const auto &args : std::vector<std::vector<std::wstring>>{
             {L"--disk"},
             {L"--disk", L"-1"},
             {L"--disk", L"4294967296"},
             {L"--x"},
             {L"--disk", L"0", L"--disk", L"1"}}) {
      bool caught = false;
      try {
        wsh::parse(args);
      } catch (const std::invalid_argument &) {
        caught = true;
      }
      check(caught);
    }
    wsh::Report r;
    for (unsigned long i = 0; i < 5; ++i) {
      wsh::Disk d;
      d.number = i;
      d.bus = i == 3 ? "virtual" : "unknown (99)";
      d.health.push_back(
          {"synthetic",
           std::vector<std::string>{"healthy", "warning", "unhealthy",
                                    "unavailable", "unknown"}[i],
           "raw",
           {}});
      r.disks.push_back(d);
    }
    wsh::Volume v;
    v.id = "volume";
    v.label = wsh::utf8(L"資料 café 🧪");
    v.capacity = 100;
    v.free = 9;
    v.extents = {{0, 0, 50}, {1, 50, 50}};
    v.dirty = true;
    r.volumes.push_back(v);
    wsh::Volume u;
    u.id = "unmounted";
    u.label = "quote\" slash\\ newline\n";
    r.volumes.push_back(u);
    wsh::analyze(r);
    check(r.findings.size() == 4);
    auto j = wsh::json(r);
    check(j.find(v.label) != std::string::npos);
    check(j.find("\\\"") != std::string::npos);
    check(j.find("\"filesystem_dirty\":null") != std::string::npos);
    check(j.find("\"state\":\"unavailable\"") != std::string::npos);
    check(wsh::human(r, true).find("capacity warning") != std::string::npos);
    if (argc == 2 && std::string(argv[1]) == "--fixture-json") {
      std::cout << j;
      return 0;
    }
    wsh::select_disk(r, 0);
    check(r.disks.size() == 1 && r.volumes.size() == 1 &&
          r.volumes.front().extents.size() == 2);
    bool absent = false;
    try {
      wsh::select_disk(r, 99);
    } catch (const std::out_of_range &) {
      absent = true;
    }
    check(absent);
    std::cout << "Synthetic CLI, health, capacity, relationships, Unicode and "
                 "JSON tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
