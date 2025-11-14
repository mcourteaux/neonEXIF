#include "neonexif/neonexif.hpp"
#include "neonexif/tiff.hpp"
#include "neonexif/tiff_tags.hpp"

// clang-format off

constexpr static uint16_t IFD_MAKERNOTE_NIKON = 0x20;

#define NEXIF_ALL_MAKERNOTE_NIKON_TAGS(x)                                               \
x(0x0001, IFD_MAKERNOTE_NIKON, UNDEFINED, CharData   , version                 , count_fixed<4>  ) \
x(0x0002, IFD_MAKERNOTE_NIKON, SHORT    , uint16_t   , iso                     , count_fixed<2>  ) \
x(0x0003, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , color_mode              , count_string    ) \
x(0x0004, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , quality                 , count_string    ) \
x(0x0005, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , white_balance           , count_string    ) \
x(0x0006, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , sharpness               , count_string    ) \
x(0x0007, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , focus_mode              , count_string    ) \
x(0x0008, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , flash_setting           , count_string    ) \
x(0x0009, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , flash_type              , count_string    ) \
x(0x0083, IFD_MAKERNOTE_NIKON, BYTE     , uint8_t    , lens_type               , count_fixed<1>  ) \
x(0x0084, IFD_MAKERNOTE_NIKON, RATIONAL , rational64u, lens_specification      , count_fixed<4>  )  // clang-format on

namespace nexif {
namespace makernote {
namespace nikon {

template <uint16_t _TagId, uint16_t _IFD_BitMask>
struct TagInfo;

const char *to_str(uint16_t tag, uint16_t ifd_bit)
{
#define TAG_TO_STR(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count) \
  if (tag == _tag##u && (ifd_bit & _ifd_bitmask)) {                                   \
    return #_name;                                                                    \
  }
  NEXIF_ALL_MAKERNOTE_NIKON_TAGS(TAG_TO_STR);
#undef TAG_TO_STR
  return nullptr;
}

NEXIF_ALL_MAKERNOTE_NIKON_TAGS(NEXIF_TEMPLATE_TAG_INFO);

std::optional<ParseError> parse_makernote(Reader &r, ExifData &data)
{
  if (r.data[0] == 'I' && r.data[1] == 'I') {
    r.byte_order = std::endian::little;
  } else if (r.data[0] == 'M' && r.data[1] == 'M') {
    r.byte_order = std::endian::big;
  } else {
    return PARSE_ERROR(CORRUPT_DATA, "Nikon header is not a TIFF file", "II or MM header not found");
  }

  RETURN_IF_OPT_ERROR(r.seek(4));
  const uint32_t root_ifd_offset = r.read_u32();
  DEBUG_PRINT("Root IFD at offset: %d", root_ifd_offset);

  uint32_t version = 0;

  Tag<uint8_t> lens_type;

  uint32_t ifd_offset = root_ifd_offset;
  while (ifd_offset) {
    RETURN_IF_OPT_ERROR(r.seek(ifd_offset));
    uint16_t num_entries = r.read_u16();
    DEBUG_PRINT("IFD at offset: %d -> Num entries: %d", ifd_offset, num_entries);
    Indenter indenter;

    for (int i = 0; i < num_entries; ++i) {
      // Read IFD entry.
      tiff::ifd_entry entry = tiff::read_ifd_entry(r);
      const char *tag_str = to_str(entry.tag, IFD_MAKERNOTE_NIKON);
      debug_print_ifd_entry(r, entry, tag_str);

      if (entry.tag == tag_version::TagId) {
        version = std::atoi((const char *)entry.data);
        DEBUG_PRINT("Nikon version: %d", version);
      }

      if (auto result = tiff::parse_tag<nikon::tag_lens_specification>(r, data.exif.lens_specification, entry)) {
      } else {
        LOG_WARNING(r, result.error().message, result.error().what);
      }

      tiff::parse_tag<nikon::tag_lens_type>(r, lens_type, entry);
    }

    ifd_offset = r.read_u32();
    DEBUG_PRINT("Next IFD offset: %d\n", ifd_offset);
  }

  // Construct Lens name
  if (data.exif.lens_specification && lens_type) {
    uint8_t bits = lens_type.value;
    char name[128];
    const char *prefix = "";
    char suffix[5]{0};
    if (bits & 0x80) {
      prefix = "AF-P ";
    } else if (!(bits & 0x1)) {
      prefix = "AF ";
    } else {
      prefix = "MF ";
    }
    if (bits & 0x40) {
      suffix[0] = 'E';
    } else if (bits & 0x04) {
      suffix[0] = 'G';
    } else if (bits & 0x02) {
      suffix[0] = 'D';
    }
    if (bits & 0x08) {
      suffix[1] = ' ';
      suffix[2] = 'V';
      suffix[3] = 'R';
    }
    const auto &ls = data.exif.lens_specification;
    if (ls.value[0] == ls.value[1]) {
      std::snprintf(
        name, sizeof(name), "%s%dmm f/%g%s",
        prefix,
        (int)(float)data.exif.lens_specification.value[0],
        (float)data.exif.lens_specification.value[2],
        suffix
      );
    } else {
      std::snprintf(
        name, sizeof(name), "%s%d-%dmm f/%g-%g%s",
        prefix,
        (int)(float)data.exif.lens_specification.value[0],
        (int)(float)data.exif.lens_specification.value[1],
        (float)data.exif.lens_specification.value[2],
        (float)data.exif.lens_specification.value[3],
        suffix
      );
    }
    data.exif.lens_model = data.store_chardata(name);
  }
  return std::nullopt;
}

}  // namespace nikon
}  // namespace makernote
}  // namespace nexif
