#include "wsh.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <windows.h>
namespace wsh {
std::string utf8(const std::wstring &s) {
  if (s.empty())
    return {};
  if (s.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
    throw std::runtime_error("Unicode input too long");
  const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(),
                                    static_cast<int>(s.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (!n)
    throw std::runtime_error("Invalid Unicode");
  std::string out(static_cast<size_t>(n), '\0');
  if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(),
                           static_cast<int>(s.size()), out.data(), n, nullptr,
                           nullptr))
    throw std::runtime_error("Unicode conversion failed");
  return out;
}
} // namespace wsh
