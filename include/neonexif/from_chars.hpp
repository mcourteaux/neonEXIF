#pragma once

#include <charconv>

namespace nexif {
using std::from_chars_result;

from_chars_result from_chars(const char* first, const char* last, int &value, int base = 10);
from_chars_result from_chars(const char* first, const char* last, unsigned &value, int base = 10);
from_chars_result from_chars(const char* first, const char* last, long &value, int base = 10);
from_chars_result from_chars(const char* first, const char* last, long long &value, int base = 10);

from_chars_result from_chars(const char *s, const char *e, float &r);

}
