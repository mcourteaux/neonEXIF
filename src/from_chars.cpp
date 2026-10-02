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

// clang-format off
#if defined(NEXIF_NO_FROM_CHARS_FLOAT)
# define HAS_FROM_CHARS_FLOAT 0
#elif defined(_LIBCPP_VERSION)  // libc++: follows the deployment target on Apple.
# if defined(_LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT)
#   define HAS_FROM_CHARS_FLOAT _LIBCPP_AVAILABILITY_HAS_FROM_CHARS_FLOATING_POINT
# else
#   define HAS_FROM_CHARS_FLOAT (_LIBCPP_VERSION >= 200000)
# endif
#elif defined(__GLIBCXX__)  // GNU libstdc++: floats since GCC 11.
# define HAS_FROM_CHARS_FLOAT (__GNUC__ >= 11)
#elif defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
# define HAS_FROM_CHARS_FLOAT 1
#else
# define HAS_FROM_CHARS_FLOAT 0
#endif
// clang-format on


#if !HAS_FROM_CHARS_FLOAT

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace nexif {
namespace detail {
// std::from_chars accepts no leading whitespace or '+', strtof does.
inline bool is_whitespace(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

locale_t c_locale()
{
  static locale_t loc = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
  return loc;
}
}  // namespace detail

from_chars_result from_chars(const char *s, const char *e, float &r)
{
  if (s == e || detail::is_whitespace(*s) || *s == '+')
    return {s, std::errc::invalid_argument};

  char buf[128];  // Larger for floats (scientific notation)
  std::size_t len = std::min<std::size_t>(e - s, sizeof(buf) - 1);
  std::memcpy(buf, s, len);
  buf[len] = '\0';

  char *endptr;
  errno = 0;
  float v = strtof_l(buf, &endptr, detail::c_locale());
  if (endptr == buf)
    return {s, std::errc::invalid_argument};
  if (errno == ERANGE)
    return {s + (endptr - buf), std::errc::result_out_of_range};
  r = v;
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
