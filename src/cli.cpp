#include "wsh.hpp"
#include <algorithm>
#include <stdexcept>
#include <windows.h>
namespace wsh {
Options parse(const std::vector<std::wstring> &args) {
  Options o;
  for (size_t i = 0; i < args.size(); ++i) {
    const auto &a = args[i];
    if (a == L"--all")
      o.all = true;
    else if (a == L"--json")
      o.json = true;
    else if (a == L"--help")
      o.help = true;
    else if (a == L"--version")
      o.version = true;
    else if (a == L"--disk") {
      if (o.disk)
        throw std::invalid_argument("Duplicate --disk");
      ++i;
      if (i == args.size() || args[i].empty())
        throw std::invalid_argument("--disk requires one unsigned disk number");
      unsigned long long n = 0;
      for (wchar_t c : args[i]) {
        if (c < L'0' || c > L'9')
          throw std::invalid_argument("Invalid disk number");
        n = n * 10 + static_cast<unsigned>(c - L'0');
        if (n > MAXDWORD)
          throw std::invalid_argument("Disk number out of range");
      }
      o.disk = static_cast<unsigned long>(n);
    } else
      throw std::invalid_argument("Unknown option");
  }
  return o;
}
} // namespace wsh
