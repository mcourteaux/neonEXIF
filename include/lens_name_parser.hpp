#pragma once

#include <string_view>
#include <regex>

namespace nexif {

struct ParsedLensName {
  int min_focal;
  int max_focal;
  float min_fnum_at_min_focal;
  float min_fnum_at_max_focal;
};

struct LensNameParser {
#define RGX_FLOAT "[0-9]+(\\.[0-9]+)?"
#define RGX_INT "(?!\\.)[0-9]+"
#define RGX_FLOAT_RANGE "(" RGX_FLOAT ")(-" RGX_FLOAT ")?"
#define RGX_INT_RANGE "(" RGX_INT ")(-" RGX_INT ")?"
  std::regex lens_focal_range_regex{RGX_INT_RANGE " ?mm"};
  std::regex lens_fnumber_regex0{"\\b[fF]\\/?" RGX_FLOAT_RANGE};
  std::regex lens_fnumber_regex1{"1\\:" RGX_FLOAT_RANGE};
  std::regex fuji_regex{"(XF|XC|GF)?" RGX_INT_RANGE "mmF" RGX_FLOAT_RANGE "(XM|Z)?"};
  std::regex combo_regex0{RGX_FLOAT_RANGE "[\\/\\:]([fF]?)" RGX_FLOAT_RANGE};
#undef RGX_FLOAT_RANGE
#undef RGX_INT_RANGE
#undef RGX_INT
#undef RGX_FLOAT

  ParsedLensName parse_lens_name(std::string_view name);
};

}  // namespace nexif
