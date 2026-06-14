#pragma once

#include <version>
#if __cpp_lib_to_chars >= 201611L
#include <charconv>
namespace nexif {
using std::from_chars;
using std::from_chars_result;
}  // namespace nexif
#else
#include <system_error>
#include <algorithm>
#include <cstring>
#include <cstdlib>
namespace nexif {
struct from_chars_result {
  const char *ptr;
  std::errc ec;
};

namespace detail {
// Helper: std::from_chars does NOT skip whitespace.
// strtol/strtof do. We must check manually.
inline bool is_whitespace(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
}  // namespace detail

inline from_chars_result from_chars(const char *s, const char *e, int &r)
{
  if (s == e || detail::is_whitespace(*s))
    return {s, std::errc::invalid_argument};

  char *endptr;
  // strtol requires a null-terminated string, so we must buffer
  char buf[64];
  std::size_t len = std::min<std::size_t>(e - s, sizeof(buf) - 1);
  std::memcpy(buf, s, len);
  buf[len] = '\0';

  long val = std::strtol(buf, &endptr, 10);

  if (endptr == buf)
    return {s, std::errc::invalid_argument};
  r = static_cast<int>(val);
  return {s + (endptr - buf), std::errc{}};
}

inline from_chars_result from_chars(const char *s, const char *e, float &r)
{
  if (s == e || detail::is_whitespace(*s))
    return {s, std::errc::invalid_argument};

  char buf[128];  // Larger for floats (scientific notation)
  std::size_t len = std::min<std::size_t>(e - s, sizeof(buf) - 1);
  std::memcpy(buf, s, len);
  buf[len] = '\0';

  char *endptr;
  r = strtof_l(buf, &endptr, LC_C_LOCALE);

  if (endptr == buf)
    return {s, std::errc::invalid_argument};
  return {s + (endptr - buf), std::errc{}};
}
}  // namespace nexif
#endif
