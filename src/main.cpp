#include "wsh.hpp"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <windows.h>
namespace {
void output(const std::string &s) {
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(h, &mode)) {
    if (s.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
      throw std::runtime_error("Output too large");
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    if (!n || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                                   static_cast<int>(s.size()), w.data(), n))
      throw std::runtime_error("Output Unicode conversion failed");
    size_t pos = 0;
    while (pos < w.size()) {
      auto count =
          static_cast<DWORD>((std::min)(size_t{16384}, w.size() - pos));
      if (pos + count < w.size() && w[pos + count - 1] >= 0xD800 &&
          w[pos + count - 1] <= 0xDBFF)
        --count;
      DWORD written = 0;
      if (!WriteConsoleW(h, w.data() + pos, count, &written, nullptr) ||
          !written)
        throw std::runtime_error("Output failed");
      pos += written;
    }
  } else {
    std::cout << s;
    if (!std::cout)
      throw std::runtime_error("Output failed");
  }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i)
      args.emplace_back(argv[i]);
    auto o = wsh::parse(args);
    if (o.help) {
      output("windows-storage-health [--all] [--disk <number>] "
             "[--json]\nRead-only local Windows storage audit. --all expands "
             "volume details.\n--help  --version\nExit codes: 0 completed, 2 "
             "usage, 3 disk not found, 4 enumeration failure, 5 internal "
             "failure.\n");
      return 0;
    }
    if (o.version) {
      output("windows-storage-health 1.0.0\n");
      return 0;
    }
    auto r = wsh::acquire();
    if (o.disk)
      wsh::select_disk(r, *o.disk);
    wsh::analyze(r);
    output(o.json ? wsh::json(r) : wsh::human(r, o.all));
    return 0;
  } catch (const std::invalid_argument &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 2;
  } catch (const std::out_of_range &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 3;
  } catch (const std::system_error &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 4;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 5;
  }
}
