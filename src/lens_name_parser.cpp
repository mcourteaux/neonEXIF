#include "neonexif/lens_name_parser.hpp"
#include "neonexif/from_chars.hpp"
#include <cassert>

namespace nexif {

ParsedLensName LensNameParser::parse_lens_name(std::string_view name)
{
  using namespace std::string_view_literals;
  ParsedLensName result;
  bool found_fnumbers = false, found_frange = false;

  std::cmatch match;
  constexpr int skip_dash = 1;
  for (auto &r : {lens_fnumber_regex0, lens_fnumber_regex1}) {
    if (std::regex_search(name.begin(), name.end(), match, r)) {
      assert(match.size() == 5 && "Unexpected regex match size");

      // first number:
      nexif::from_chars(match[1].first, match[1].second, result.min_fnum_at_min_focal);

      if (std::csub_match p2 = match[3]; p2.length()) {
        // two numbers:
        nexif::from_chars(p2.first + skip_dash, p2.second, result.min_fnum_at_max_focal);
      } else {
        result.min_fnum_at_max_focal = result.min_fnum_at_min_focal;
      }

      found_fnumbers = true;
      break;
    }
  }
  if (std::regex_search(name.begin(), name.end(), match, lens_focal_range_regex)) {
    assert(match.size() == 3 && "Unexpected regex match size");
    // first number:
    nexif::from_chars(match[1].first, match[1].second, result.min_focal);

    if (std::csub_match p2 = match[2]; p2.length()) {
      // two numbers:
      nexif::from_chars(p2.first + skip_dash, p2.second, result.max_focal);
    } else {
      result.max_focal = result.min_focal;
    }
    found_frange = true;
  }

  if (!found_fnumbers || !found_frange) {
    if (std::regex_search(name.begin(), name.end(), match, fuji_regex)) {
      // (Code) INT_RANGE  FLOAT_RANGE
      assert(match.size() == 9 && "Unexpected regex match size");

      nexif::from_chars(match[2].first, match[2].second, result.min_focal);

      if (std::csub_match p2 = match[3]; p2.length()) {
        // two numbers:
        nexif::from_chars(p2.first + skip_dash, p2.second, result.max_focal);
      } else {
        result.max_focal = result.min_focal;
      }

      // third number:
      nexif::from_chars(match[4].first, match[4].second, result.min_fnum_at_min_focal);

      if (std::csub_match p2 = match[6]; p2.length()) {
        // fourth numbers:
        nexif::from_chars(p2.first + skip_dash, p2.second, result.min_fnum_at_max_focal);
      } else {
        result.min_fnum_at_max_focal = result.min_fnum_at_min_focal;
      }

      found_fnumbers = true;
      found_frange = true;
    } else if (std::regex_search(name.begin(), name.end(), match, combo_regex0)) {
      // FLOAT_RANGE (Sep) FLOAT_RANGE
      assert(match.size() == 10 && "Unexpected regex match size");
      float f1, f2, f3, f4;

      nexif::from_chars(match[1].first, match[1].second, f1);
      if (std::csub_match p2 = match[3]; p2.length()) {
        nexif::from_chars(p2.first + skip_dash, p2.second, f2);
      } else {
        f2 = f1;
      }

      nexif::from_chars(match[6].first, match[6].second, f3);
      if (std::csub_match p2 = match[8]; p2.length()) {
        nexif::from_chars(p2.first + skip_dash, p2.second, f4);
      } else {
        f4 = f3;
      }

      bool parsed = false;
      if (std::csub_match sep_match = match[5]; sep_match.length()) {
        std::string_view sep(sep_match.first, sep_match.second);
        if (sep == "f"sv || sep == "F"sv) {
          // We are sure the second set of numbers is f-number:
          result.min_focal = f1;
          result.max_focal = f2;
          result.min_fnum_at_min_focal = f3;
          result.min_fnum_at_max_focal = f4;
          parsed = true;
        }
      }

      if (!parsed) {
        // Check the values of the numbers: f-numbers are smaller than focal lengths
        if (f1 < f3) {
          result.min_focal = f3;
          result.max_focal = f4;
          result.min_fnum_at_min_focal = f1;
          result.min_fnum_at_max_focal = f2;
        } else {
          result.min_focal = f1;
          result.max_focal = f2;
          result.min_fnum_at_min_focal = f3;
          result.min_fnum_at_max_focal = f4;
        }
        parsed = true;
      }

      found_frange = true;
      found_fnumbers = true;
    }
  }

  return result;
}

}  // namespace nexif
