#include "neonexif/from_chars.hpp"

#include <version>
#include <charconv>

namespace nexif {
using std::from_chars_result;

from_chars_result from_chars(const char* first, const char* last, int &value, int base) {
  return std::from_chars(first, last, value, base);
}
from_chars_result from_chars(const char* first, const char* last, unsigned &value, int base) {
  return std::from_chars(first, last, value, base);
}
from_chars_result from_chars(const char* first, const char* last, long &value, int base) {
  return std::from_chars(first, last, value, base);
}
}  // namespace nexif

#if defined(__cpp_lib_to_chars)
# define HAS_FROM_CHARS_FLOAT 1
#else
// clang-format off
#if defined(_LIBCPP_VERSION)  // libc++
# if _LIBCPP_VERSION >= 20000
#   define HAS_FROM_CHARS_FLOAT 1
# else
#   define HAS_FROM_CHARS_FLOAT 0
# endif

#elif defined(__GLIBCXX__)  // GNU libstdc++
# if __GNUC__ >= 11
   // Floating-point from_chars was introduced in GCC 11
#   define HAS_GCC_FROM_CHARS_FLOAT 1
# else
   // GCC 8, 9, and 10 only support integers, lacking floats
#   define HAS_GCC_FROM_CHARS_FLOAT 0
# endif

#else // Generic
# if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
#   define HAS_FROM_CHARS_FLOAT 1
# else
#   define HAS_FROM_CHARS_FLOAT 0
# endif
#endif
// clang-format on

#if defined(NEXIF_NO_FROM_CHARS_FLOAT)
# undef HAS_FROM_CHARS_FLOAT
# define HAS_FROM_CHARS_FLOAT 0
#endif
#endif


#if !HAS_FROM_CHARS_FLOAT


#include <system_error>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace nexif {
namespace detail {
// Helper: std::from_chars does NOT skip whitespace.
// strtol/strtof do. We must check manually.
inline bool is_whitespace(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
}  // namespace detail

from_chars_result from_chars(const char *s, const char *e, float &r)
{
  if (s == e || detail::is_whitespace(*s))
    return {s, std::errc::invalid_argument};

  char buf[128];  // Larger for floats (scientific notation)
  std::size_t len = std::min<std::size_t>(e - s, sizeof(buf) - 1);
  std::memcpy(buf, s, len);
  buf[len] = '\0';

  char *endptr;
  //r = strtof_l(buf, &endptr, "C");

  if (endptr == buf)
    return {s, std::errc::invalid_argument};
  return {s + (endptr - buf), std::errc{}};
}
}  // namespace nexif
#else

namespace nexif {
from_chars_result from_chars(const char *s, const char *e, float &r) {
  return std::from_chars(s, e, r);
}
}
#endif

