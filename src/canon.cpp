#include "neonexif/neonexif.hpp"
#include "neonexif/tiff.hpp"
#include "neonexif/tag_helpers.hpp"
#include <cmath>

#include "canon_lens_id.cpp"

// clang-format off

constexpr static uint16_t IFD_MAKERNOTE_CANON = 0x40;

#define NEXIF_ALL_MAKERNOTE_CANON_TAGS(x)                                                          \
x(0x0001, IFD_MAKERNOTE_CANON, SHORT    , uint16_t   , camera_settings         , count_var       ) \
x(0x000c, IFD_MAKERNOTE_CANON, LONG     , uint32_t   , serial_number           , count_scalar    ) \
x(0x000d, IFD_MAKERNOTE_CANON, UNDEFINED, uint8_t    , camera_info             , count_var       ) \
x(0x0009, IFD_MAKERNOTE_CANON, ASCII    , CharData   , owner_name              , count_string    ) \
x(0x0095, IFD_MAKERNOTE_CANON, ASCII    , CharData   , lens_model              , count_string    ) \
x(0x0098, IFD_MAKERNOTE_CANON, SHORT    , uint16_t   , crop_info               , count_fixed<4>  )

// clang-format on

namespace nexif {

namespace makernote::canon {
using namespace std::string_view_literals;

NEXIF_DECLARE_TAG_INFO;
NEXIF_MAKE_TO_STRING(NEXIF_ALL_MAKERNOTE_CANON_TAGS);
NEXIF_ALL_MAKERNOTE_CANON_TAGS(NEXIF_TEMPLATE_TAG_INFO);
NEXIF_MAKE_TAG_ENUM(NEXIF_ALL_MAKERNOTE_CANON_TAGS);
NEXIF_MAKE_TAG_CMP;

struct ParseInfo {
  std::regex models;

  struct Field {
    int offset{-1};
    bool rev{false};
  } lens_type,
    min_focal, max_focal,
    lens_model;
} parse_infos[] = {
  {.models = std::regex{"EOS-1Ds?"},
   .lens_type = {13, true},
   .min_focal = {14},
   .max_focal = {16}},  // overlap??
  {.models = std::regex{"\\b1Ds? Mark II"},
   .lens_type = {12, true},
   .min_focal = {17, true},
   .max_focal = {19, true}},
  {.models = std::regex{"\\b1Ds? Mark II N"},
   .lens_type = {12, true},
   .min_focal = {17, true},
   .max_focal = {19, true}},
  {.models = std::regex{"\\b1Ds? Mark III"},
   .lens_type = {273, true},
   .min_focal = {275, true},
   .max_focal = {277, true}},
  {.models = std::regex{"\\b1D Mark IV"},
   .lens_type = {335, true},
   .min_focal = {337, true},
   .max_focal = {339, true}},
  {.models = std::regex{"EOS-1D X$"},
   .lens_type = {423, true},
   .min_focal = {425, true},
   .max_focal = {427, true}},
  {.models = std::regex{"EOS 5D$"},
   .lens_type = {12, true},
   .min_focal = {147, true},
   .max_focal = {149, true}},
  {.models = std::regex{"EOS 5D Mark II$"},
   .lens_type = {230, true},
   .min_focal = {232, true},
   .max_focal = {234, true}},
  {.models = std::regex{"EOS 5D Mark III$"},
   .lens_type = {339, true},
   .min_focal = {341, true},
   .max_focal = {343, true}},
  {.models = std::regex{"EOS 6D$"},
   .lens_type = {353, true},
   .min_focal = {355, true},
   .max_focal = {357, true}},
  {.models = std::regex{"EOS 7D$"},
   .lens_type = {274, true},
   .min_focal = {276, true},
   .max_focal = {278, true}},
  {.models = std::regex{"EOS 40D$"},
   .lens_type = {214, true},
   .min_focal = {216, true},
   .max_focal = {218, true}},
  {.models = std::regex{"EOS 50D$"},
   .lens_type = {234, true},
   .min_focal = {236, true},
   .max_focal = {238, true}},
  {.models = std::regex{"\\b(EOS 60D|1200D|REBEL T5|Kiss X70)\\b"},
   .lens_type = {232, true},
   .min_focal = {234, true},
   .max_focal = {236, true}},
  {.models = std::regex{"EOS-70D$"},
   .lens_type = {358, true},
   .min_focal = {360, true},
   .max_focal = {362, true}},
  {.models = std::regex{"EOS-80D$"},
   .lens_type = {393, true},
   .min_focal = {395, true},
   .max_focal = {397, true}},
  {.models = std::regex{"\\b(450D|EBEL XSi|Kiss X2)\\b"},
   .lens_type = {222, true},
   .lens_model = {2355}},
  {.models = std::regex{"\\b(500D|REBEL T1i|Kiss X3)\\b"},
   .lens_type = {246, true},
   .min_focal = {248, true},
   .max_focal = {250, true}},
  {.models = std::regex{"\\b(550D|REBEL T2i|Kiss X4)\\b"},
   .lens_type = {255, true},
   .min_focal = {257, true},
   .max_focal = {259, true}},
  {.models = std::regex{"\b(600D|REBEL T3i|Kiss X5|1100D)\\b"},
   .lens_type = {234, true},
   .min_focal = {236, true},
   .max_focal = {238, true}},
  {.models = std::regex{"\\b(650D|REBEL T4i|Kiss X6i|700D|REBEL T5i|Kiss X7i)\\b"},
   .lens_type = {295, true},
   .min_focal = {297, true},
   .max_focal = {299, true}},
  {.models = std::regex{"\\b(750D|Rebel T6i|Kiss X8i|760D|Rebel T6s|8000D)\\b"},
   .lens_type = {388, true},
   .min_focal = {390, true},
   .max_focal = {392, true}},
  {.models = std::regex{"\\b(1000D|REBEL XS|Kiss F)\\b"},
   .lens_type = {226, true},
   .min_focal = {228, true},
   .max_focal = {230, true},
   .lens_model = {2359}},
};

template <bool RoundThirds>
float ev_from_s16(const int16_t v)
{
  int32_t abs = std::abs((int)v);
  int32_t ipart = (abs & 0xffe0) >> 5;
  int32_t fpart = (abs & 0x001f);
  float frac = fpart * (1.0f / 32.0f);
  if constexpr (RoundThirds) {
    if (fpart == 0x0c)
      frac = (1.0f / 3.0f);
    if (fpart == 0x14)
      frac = (2.0f / 3.0f);
  }
  return ipart + frac;
}

float f_number_from_aperture(float aperture_val)
{
  // f_num = sqrt(2^(EV)) = 2^(0.5 * EV)
  return std::exp2(aperture_val * 0.5f);
}

std::optional<ParseError> parse_makernote(Reader &r, ExifData &data)
{
  DEBUG_PRINT("Parse Canon Makernote");
  CanonMakernote &mn = data.makernote.emplace<CanonMakernote>();

  float min_aperture = 0;
  float max_aperture = 0;

  std::string_view lens_model;

  uint16_t num_entries = r.read_u16();

  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    tiff::ifd_entry entry = tiff::read_ifd_entry(r);
    const char *tag_str = to_str(entry.tag, IFD_MAKERNOTE_CANON);
    debug_print_ifd_entry(r, entry, tag_str);

#define PARSE_CANON_TAG(_name) NEXIF_PARSE_TAG(mn, _name, entry, IFD_MAKERNOTE_CANON)

    PARSE_CANON_TAG(serial_number);

    if (entry.tag == tag_camera_settings::TagId) {
      if (auto pr = tiff::fetch_entry_value<uint16_t>(entry, 22, r)) {
        mn.lens_type = pr.value();
        mn.lens_type.parsed_from = entry.tag;
        DEBUG_PRINT("lens type from CS: %d", mn.lens_type.value);
      }
      float focal_units = 1.0f;  // units / mm
      if (auto pr = tiff::fetch_entry_value<uint16_t>(entry, 25, r)) {
        focal_units = pr.value();
      }
      if (auto pr = tiff::fetch_entry_value<uint16_t>(entry, 23, r)) {
        mn.max_focal_length = pr.value() / focal_units;
        mn.max_focal_length.parsed_from = entry.tag;
      }
      if (auto pr = tiff::fetch_entry_value<uint16_t>(entry, 24, r)) {
        mn.min_focal_length = pr.value() / focal_units;
        mn.min_focal_length.parsed_from = entry.tag;
      }
      if (auto pr = tiff::fetch_entry_value<int16_t>(entry, 26, r)) {
        max_aperture = f_number_from_aperture(ev_from_s16<false>(pr.value()));
      }
      if (auto pr = tiff::fetch_entry_value<uint16_t>(entry, 27, r)) {
        min_aperture = f_number_from_aperture(ev_from_s16<false>(pr.value()));
      }
    } else if (entry.tag == tag_camera_info::TagId) {
      int lens_id_offset = 0;
      if (data.model) {
        std::string_view model = data.model.value.view();
        for (ParseInfo pi : parse_infos) {
          if (std::regex_search(model.begin(), model.end(), pi.models)) {
            if (!mn.lens_type.is_set) {
              if (auto pr = tiff::fetch_entry_value_raw_offset<uint16_t>(entry, pi.lens_type.offset, r)) {
                if (pi.lens_type.rev) {
                  mn.lens_type = nexif::byteswap(pr.value());
                } else {
                  mn.lens_type = pr.value();
                }
                mn.lens_type.parsed_from = entry.tag;
                DEBUG_PRINT("lens type from CI: %d", mn.lens_type.value);
              }
            }
            break;
          }
        }
      }
    } else if (entry.tag == canon::tag_lens_model::TagId) {
      if (auto result = tiff::parse_tag<canon::tag_lens_model>(r, data.exif.lens_model, entry)) {
        auto name = data.exif.lens_model.value.view();
        DEBUG_PRINT("lens model:  %.*s", int(name.length()), name.data());
      }
    } else if (entry.tag == 0x0096) {
      if (entry.type == tiff::DType::ASCII) {
        if (auto v = entry.data_view(r)) {
          mn.internal_serial_number = data.store_string_data(v.value());
          DEBUG_PRINT("serial number: %.*s", int(v.value().length()), v.value().data());
        }
      }
    } else if (entry.tag == 0x4019) {
      if (entry.size() >= 5) {
        if (auto v = entry.data_view(r)) {
          char buf[16];
          std::snprintf(
            buf, sizeof(buf), "%02x%02x%02x%02x%02x",
            v.value().data()[0],
            v.value().data()[1],
            v.value().data()[2],
            v.value().data()[3],
            v.value().data()[4]
          );
          if (!mn.lens_serial_number) {
            mn.lens_serial_number = data.store_string_data(buf);
            mn.lens_serial_number.parsed_from = entry.tag;
          }
          DEBUG_PRINT("lens serial number: %s", buf);
        }
      }
    }
  }

  DEBUG_PRINT("Min focal: %d", mn.min_focal_length.value_or(0));
  DEBUG_PRINT("Max focal: %d", mn.min_focal_length.value_or(0));
  DEBUG_PRINT("Max aperture: %f", max_aperture);
  DEBUG_PRINT("Min aperture: %f", min_aperture);
  DEBUG_PRINT("Lens type: %d", mn.lens_type.value_or(0));

  std::array<CanonLensID *, 8> candidates;
  uint32_t num_cand = 0;

  if (mn.lens_type && mn.min_focal_length && mn.max_focal_length) {
    parse_all_lenses();
    for (CanonLensID &lens : canon_lenses) {
      // DEBUG_PRINT(
      //   "testing lens: %.*s  (%d %d %f %f)",
      //   (int)lens.name.length(), lens.name.data(),
      //   lens.min_focal, lens.max_focal,
      //   lens.min_fnum_at_min_focal, lens.min_fnum_at_max_focal
      //);
      if (true
          && num_cand < 8
          && lens.id == mn.lens_type.value
          && lens.min_focal == mn.min_focal_length.value
          && lens.max_focal == mn.min_focal_length.value
          && std::abs(lens.min_fnum_at_min_focal - max_aperture) < 0.05f) {
        DEBUG_PRINT("Could be lens: %.*s", (int)lens.name.length(), lens.name.data());
        candidates[num_cand++] = &lens;
      }
    }
  }

  if (num_cand == 1) {
    CanonLensID &lens = *candidates[0];
    data.exif.lens_model = data.store_string_data(lens.name);
    data.exif.lens_specification = std::array<rational64u, 4>{
      rational64u{mn.min_focal_length.value, 1},
      rational64u{mn.max_focal_length.value, 1},
      rational64u{(uint32_t)(lens.min_fnum_at_min_focal * 10), 10},
      rational64u{(uint32_t)(lens.min_fnum_at_max_focal * 10), 10},
    };
  } else {
    data.exif.lens_specification = std::array<rational64u, 4>{
      rational64u{mn.min_focal_length, 1},
      rational64u{mn.min_focal_length, 1},
      0, 0
    };

    std::array<std::string_view, 8> cand_names;
    for (int i = 0; i < num_cand; ++i) {
      cand_names[i] = candidates[i]->name;
    }
    data.exif.possible_lenses = {cand_names, num_cand};
    data.exif.possible_lenses.parsed_from = tag_camera_info::TagId;
  }

  if (!data.exif.body_serial_number.is_set) {
    if (mn.serial_number) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%u", mn.serial_number.value);
      data.exif.body_serial_number = data.store_string_data(buf);
    } else if (mn.internal_serial_number) {
      data.exif.body_serial_number = mn.internal_serial_number.value.view();
    }
  }

  return {};
}

}  // namespace makernote::canon
}  // namespace nexif
