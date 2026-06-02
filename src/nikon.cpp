#include "neonexif/neonexif.hpp"
#include "neonexif/tiff.hpp"
#include "neonexif/tag_helpers.hpp"

#include "nikon_lens_id.cpp"

#include <charconv>

// clang-format off

constexpr static uint16_t IFD_MAKERNOTE_NIKON = 0x20;

#define NEXIF_ALL_MAKERNOTE_NIKON_TAGS(x)                                               \
x(0x0001, IFD_MAKERNOTE_NIKON, UNDEFINED, char       , version                 , count_fixed<4>  ) \
x(0x0002, IFD_MAKERNOTE_NIKON, SHORT    , uint16_t   , iso                     , count_fixed<2>  ) \
x(0x0003, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , color_mode              , count_string    ) \
x(0x0004, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , quality                 , count_string    ) \
x(0x0005, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , white_balance           , count_string    ) \
x(0x0006, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , sharpness               , count_string    ) \
x(0x0007, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , focus_mode              , count_string    ) \
x(0x0008, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , flash_setting           , count_string    ) \
x(0x0009, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , flash_type              , count_string    ) \
x(0x001d, IFD_MAKERNOTE_NIKON, ASCII    , CharData   , serial_number           , count_string    ) \
x(0x0083, IFD_MAKERNOTE_NIKON, BYTE     , uint8_t    , lens_type               , count_scalar    ) \
x(0x0084, IFD_MAKERNOTE_NIKON, RATIONAL , rational64u, lens_specification      , count_fixed<4>  ) \
x(0x0093, IFD_MAKERNOTE_NIKON, SHORT    , uint16_t   , nef_compression         , count_scalar    ) \
x(0x0096, IFD_MAKERNOTE_NIKON, UNDEFINED, CharData   , linearization_table     , count_string    ) \
x(0x0097, IFD_MAKERNOTE_NIKON, UNDEFINED, CharData   , color_balance           , count_string    ) \
x(0x0098, IFD_MAKERNOTE_NIKON, UNDEFINED, CharData   , lens_data               , count_string    ) \
x(0x00a7, IFD_MAKERNOTE_NIKON, UNDEFINED, uint32_t   , shutter_count           , count_scalar    )

// clang-format on

namespace nexif {
namespace makernote {
namespace nikon {

extern const uint8_t xlat[2][256];

NEXIF_DECLARE_TAG_INFO;
NEXIF_MAKE_TO_STRING(NEXIF_ALL_MAKERNOTE_NIKON_TAGS);
NEXIF_ALL_MAKERNOTE_NIKON_TAGS(NEXIF_TEMPLATE_TAG_INFO);
NEXIF_MAKE_TAG_ENUM(NEXIF_ALL_MAKERNOTE_NIKON_TAGS);
NEXIF_MAKE_TAG_CMP;

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

  NikonMakernote &mn = data.makernote.emplace<NikonMakernote>();

  uint8_t lensdata_buffer[1024];
  int lensdata_len{0};

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

#define PARSE_NIKON_TAG(_name) NEXIF_PARSE_TAG(mn, _name, entry, IFD_MAKERNOTE_NIKON)

      PARSE_NIKON_TAG(version);
      // if (entry.tag == tag_version::TagId) {
      //   auto result = std::from_chars((const char*)entry.data, (const char*)entry.data + entry.size(), version);
      //   // TODO handle error?
      //   DEBUG_PRINT("Nikon version: %d", version);
      // }

      PARSE_NIKON_TAG(iso);
      PARSE_NIKON_TAG(color_mode);
      PARSE_NIKON_TAG(quality);
      PARSE_NIKON_TAG(white_balance);
      PARSE_NIKON_TAG(sharpness);
      PARSE_NIKON_TAG(focus_mode);
      PARSE_NIKON_TAG(flash_setting);
      PARSE_NIKON_TAG(flash_type);
      PARSE_NIKON_TAG(serial_number);
      PARSE_NIKON_TAG(lens_type);
      PARSE_NIKON_TAG(lens_specification);
      PARSE_NIKON_TAG(nef_compression);
      PARSE_NIKON_TAG(linearization_table);
      PARSE_NIKON_TAG(color_balance);
      PARSE_NIKON_TAG(shutter_count);

      if (entry.tag == tag_lens_data::TagId) {
        uint32_t offset = entry.offset(r);
        uint32_t size = std::min((int)sizeof(lensdata_buffer), entry.size());
        if (auto view = r.data_view(offset, size); view) {
          std::memcpy(lensdata_buffer, view.value().data(), size);
          lensdata_len = size;
        }
      }
    }

    ifd_offset = r.read_u32();
    DEBUG_PRINT("Next IFD offset: %d\n", ifd_offset);
  }

  // Parse the lensdata buffer
  if (lensdata_len > 4) {
    Reader lens_r(r.warnings);
    lens_r.byte_order = r.byte_order;
    lens_r.data = (char *)lensdata_buffer;
    lens_r.file_length = lensdata_len;
    char version_bytes[4];
    lens_r.read_4bytes((uint8_t *)version_bytes);
    int version = 0;
    std::from_chars(version_bytes, version_bytes + 4, version);
    DEBUG_PRINT("LensData version: %d\n", version);

    for (int i = 0; i < lensdata_len; ++i) {
      DEBUG_PRINT("%03d: %02x | '%c'", i, lensdata_buffer[i], lensdata_buffer[i]);
    }

    if (version >= 201) {
      // Decrypt with the reverse engineered algorithm.
      if (mn.shutter_count.is_set && mn.serial_number.is_set) {
        uint32_t shutter_count = mn.shutter_count.value;
        uint8_t *shutter_count_bytes = (uint8_t *)&shutter_count;
        uint8_t key = shutter_count_bytes[0] ^ shutter_count_bytes[1] ^ shutter_count_bytes[2] ^ shutter_count_bytes[3];

        uint32_t serial;
        auto serial_bytes = mn.serial_number.value.view();
        for (char b : serial_bytes) {
          serial *= 10;
          serial += std::isdigit(b) ? (b - '0') : (b % 10);
        }
        DEBUG_PRINT("serial: %d", serial);

        uint8_t ci = xlat[0][serial & 0xff];
        uint8_t cj = xlat[1][key];
        uint8_t ck = 0x60;
        for (int i = 4; i < lensdata_len; ++i) {
          cj += ci * ck++;
          lensdata_buffer[i] ^= cj;
        }

        for (int i = 4; i < lensdata_len; ++i) {
          DEBUG_PRINT("%03d: %02x | '%c'", i, lensdata_buffer[i], lensdata_buffer[i]);
        }
      } else {
        r.warnings.push_back({
          .msg = "Shutter counter or serial number not parsed, which is needed to decipher Nikon lens data",
          .what = "Cannot decrypt Nikon lens data",
        });
      }
    }

    int id_offset = 0;
    NikonMakernote::NikonMount mount;
    switch (version) {
      case 100: {
        id_offset = 11;
        mount = NikonMakernote::F_Mount;
      } break;
      case 101:
      case 201:
      case 203: {
        id_offset = 11;
        mount = NikonMakernote::F_Mount;
      } break;
      case 204: {
        id_offset = 12;
        mount = NikonMakernote::F_Mount;
      } break;
      case 400:
      case 401: {
        id_offset = 394;
        mount = NikonMakernote::One_Mount;
      } break;
      case 402: {
        id_offset = 395;
        mount = NikonMakernote::One_Mount;
      } break;
      case 403: {
        id_offset = 684;
        mount = NikonMakernote::One_Mount;
      } break;
      case 800:
      case 801:
      case 802: {
        id_offset = 48;  // Z-mount has a u16 ID field.
        mount = NikonMakernote::Z_Mount;
      } break;

      default: {
        r.warnings.push_back({.msg = "Cannot parse Nikon lensdata: unknown version."});
      } break;
    }

    if (id_offset) {
      mn.lens_mount = mount;
      mn.lens_mount.parsed_from = tag_lens_data::TagId;

      if (mount == NikonMakernote::F_Mount) {
        if (id_offset) {
          std::array<std::string_view, 8> cand_lenses;
          uint32_t num_cand = 0;
          mn.f_mount_lens_identifier = std::array<uint8_t, 8>{
            lensdata_buffer[id_offset + 0],
            lensdata_buffer[id_offset + 1],
            lensdata_buffer[id_offset + 2],
            lensdata_buffer[id_offset + 3],
            lensdata_buffer[id_offset + 4],
            lensdata_buffer[id_offset + 5],
            lensdata_buffer[id_offset + 6],
            mn.lens_type.value_or(0),
          };

          // Lookup lens id
          for (NikonFMountLensID &lens : nikon_dslr_fmount_lenses) {
            if (mn.f_mount_lens_identifier.value == lens.id) {
              if (num_cand < 8) {
                cand_lenses[num_cand++] = lens.name;
              }
            }
          }
          if (num_cand == 1) {
            data.exif.lens_model = data.store_string_data(cand_lenses[0]);
          } else {
            data.exif.possible_lenses = {cand_lenses, num_cand};
            data.exif.possible_lenses.parsed_from = tag_lens_data::TagId;
          }
          if (!data.exif.lens_model.is_set) {
            r.warnings.push_back({
              .what = "Unknown F-mount lens identifier. Please report your lens to ExifTool and NeonEXIF.",
            });
          }
        }
      } else if (mount == NikonMakernote::Z_Mount) {
        r.seek(id_offset);
        mn.z_mount_lens_identifier = r.read_u16();
        mn.z_mount_lens_identifier.parsed_from = tag_lens_data::TagId;

        // Lookup lens id
        for (NikonZMountLensID &lens : nikon_mirrorless_zmount_lenses) {
          if (mn.z_mount_lens_identifier.value == lens.id) {
            data.exif.lens_model = data.store_string_data(lens.name);
            break;
          }
        }
        if (!data.exif.lens_model.is_set) {
          r.warnings.push_back({
            .what = "Unknown Z-mount lens identifier. Please report your lens to ExifTool and NeonEXIF.",
          });
        }
      } else if (mount == NikonMakernote::One_Mount) {
        data.exif.lens_model = data.store_string_data((const char *)&lensdata_buffer[id_offset]);
      }
    }
  }

  // Copy over fields to their more general counterpart.
  data.exif.lens_specification = mn.lens_specification;

  // Construct Lens name
  if (!data.exif.lens_model && data.exif.lens_specification && mn.lens_type) {
    uint8_t bits = mn.lens_type.value;
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
        (int)(float)ls.value[0],
        (float)ls.value[2],
        suffix
      );
    } else {
      if (ls.value[2] == ls.value[3]) {
        std::snprintf(
          name, sizeof(name), "%s%d-%dmm f/%g%s",
          prefix,
          (int)(float)ls.value[0],
          (int)(float)ls.value[1],
          (float)ls.value[2],
          suffix
        );
      } else {
        std::snprintf(
          name, sizeof(name), "%s%d-%dmm f/%g-%g%s",
          prefix,
          (int)(float)ls.value[0],
          (int)(float)ls.value[1],
          (float)ls.value[2],
          (float)ls.value[3],
          suffix
        );
      }
    }
    data.exif.lens_model = data.store_string_data(name);
  }

  // Serial number canoncalize
  if (mn.serial_number.is_set) {
    data.exif.body_serial_number = mn.serial_number.value.view();
    data.exif.body_serial_number.parsed_from = mn.serial_number.parsed_from;
  }

  return std::nullopt;
}

const uint8_t xlat[2][256] = {
  {0xc1, 0xbf, 0x6d, 0x0d, 0x59, 0xc5, 0x13, 0x9d, 0x83, 0x61, 0x6b, 0x4f,
   0xc7, 0x7f, 0x3d, 0x3d, 0x53, 0x59, 0xe3, 0xc7, 0xe9, 0x2f, 0x95, 0xa7,
   0x95, 0x1f, 0xdf, 0x7f, 0x2b, 0x29, 0xc7, 0x0d, 0xdf, 0x07, 0xef, 0x71,
   0x89, 0x3d, 0x13, 0x3d, 0x3b, 0x13, 0xfb, 0x0d, 0x89, 0xc1, 0x65, 0x1f,
   0xb3, 0x0d, 0x6b, 0x29, 0xe3, 0xfb, 0xef, 0xa3, 0x6b, 0x47, 0x7f, 0x95,
   0x35, 0xa7, 0x47, 0x4f, 0xc7, 0xf1, 0x59, 0x95, 0x35, 0x11, 0x29, 0x61,
   0xf1, 0x3d, 0xb3, 0x2b, 0x0d, 0x43, 0x89, 0xc1, 0x9d, 0x9d, 0x89, 0x65,
   0xf1, 0xe9, 0xdf, 0xbf, 0x3d, 0x7f, 0x53, 0x97, 0xe5, 0xe9, 0x95, 0x17,
   0x1d, 0x3d, 0x8b, 0xfb, 0xc7, 0xe3, 0x67, 0xa7, 0x07, 0xf1, 0x71, 0xa7,
   0x53, 0xb5, 0x29, 0x89, 0xe5, 0x2b, 0xa7, 0x17, 0x29, 0xe9, 0x4f, 0xc5,
   0x65, 0x6d, 0x6b, 0xef, 0x0d, 0x89, 0x49, 0x2f, 0xb3, 0x43, 0x53, 0x65,
   0x1d, 0x49, 0xa3, 0x13, 0x89, 0x59, 0xef, 0x6b, 0xef, 0x65, 0x1d, 0x0b,
   0x59, 0x13, 0xe3, 0x4f, 0x9d, 0xb3, 0x29, 0x43, 0x2b, 0x07, 0x1d, 0x95,
   0x59, 0x59, 0x47, 0xfb, 0xe5, 0xe9, 0x61, 0x47, 0x2f, 0x35, 0x7f, 0x17,
   0x7f, 0xef, 0x7f, 0x95, 0x95, 0x71, 0xd3, 0xa3, 0x0b, 0x71, 0xa3, 0xad,
   0x0b, 0x3b, 0xb5, 0xfb, 0xa3, 0xbf, 0x4f, 0x83, 0x1d, 0xad, 0xe9, 0x2f,
   0x71, 0x65, 0xa3, 0xe5, 0x07, 0x35, 0x3d, 0x0d, 0xb5, 0xe9, 0xe5, 0x47,
   0x3b, 0x9d, 0xef, 0x35, 0xa3, 0xbf, 0xb3, 0xdf, 0x53, 0xd3, 0x97, 0x53,
   0x49, 0x71, 0x07, 0x35, 0x61, 0x71, 0x2f, 0x43, 0x2f, 0x11, 0xdf, 0x17,
   0x97, 0xfb, 0x95, 0x3b, 0x7f, 0x6b, 0xd3, 0x25, 0xbf, 0xad, 0xc7, 0xc5,
   0xc5, 0xb5, 0x8b, 0xef, 0x2f, 0xd3, 0x07, 0x6b, 0x25, 0x49, 0x95, 0x25,
   0x49, 0x6d, 0x71, 0xc7},
  {0xa7, 0xbc, 0xc9, 0xad, 0x91, 0xdf, 0x85, 0xe5, 0xd4, 0x78, 0xd5, 0x17,
   0x46, 0x7c, 0x29, 0x4c, 0x4d, 0x03, 0xe9, 0x25, 0x68, 0x11, 0x86, 0xb3,
   0xbd, 0xf7, 0x6f, 0x61, 0x22, 0xa2, 0x26, 0x34, 0x2a, 0xbe, 0x1e, 0x46,
   0x14, 0x68, 0x9d, 0x44, 0x18, 0xc2, 0x40, 0xf4, 0x7e, 0x5f, 0x1b, 0xad,
   0x0b, 0x94, 0xb6, 0x67, 0xb4, 0x0b, 0xe1, 0xea, 0x95, 0x9c, 0x66, 0xdc,
   0xe7, 0x5d, 0x6c, 0x05, 0xda, 0xd5, 0xdf, 0x7a, 0xef, 0xf6, 0xdb, 0x1f,
   0x82, 0x4c, 0xc0, 0x68, 0x47, 0xa1, 0xbd, 0xee, 0x39, 0x50, 0x56, 0x4a,
   0xdd, 0xdf, 0xa5, 0xf8, 0xc6, 0xda, 0xca, 0x90, 0xca, 0x01, 0x42, 0x9d,
   0x8b, 0x0c, 0x73, 0x43, 0x75, 0x05, 0x94, 0xde, 0x24, 0xb3, 0x80, 0x34,
   0xe5, 0x2c, 0xdc, 0x9b, 0x3f, 0xca, 0x33, 0x45, 0xd0, 0xdb, 0x5f, 0xf5,
   0x52, 0xc3, 0x21, 0xda, 0xe2, 0x22, 0x72, 0x6b, 0x3e, 0xd0, 0x5b, 0xa8,
   0x87, 0x8c, 0x06, 0x5d, 0x0f, 0xdd, 0x09, 0x19, 0x93, 0xd0, 0xb9, 0xfc,
   0x8b, 0x0f, 0x84, 0x60, 0x33, 0x1c, 0x9b, 0x45, 0xf1, 0xf0, 0xa3, 0x94,
   0x3a, 0x12, 0x77, 0x33, 0x4d, 0x44, 0x78, 0x28, 0x3c, 0x9e, 0xfd, 0x65,
   0x57, 0x16, 0x94, 0x6b, 0xfb, 0x59, 0xd0, 0xc8, 0x22, 0x36, 0xdb, 0xd2,
   0x63, 0x98, 0x43, 0xa1, 0x04, 0x87, 0x86, 0xf7, 0xa6, 0x26, 0xbb, 0xd6,
   0x59, 0x4d, 0xbf, 0x6a, 0x2e, 0xaa, 0x2b, 0xef, 0xe6, 0x78, 0xb6, 0x4e,
   0xe0, 0x2f, 0xdc, 0x7c, 0xbe, 0x57, 0x19, 0x32, 0x7e, 0x2a, 0xd0, 0xb8,
   0xba, 0x29, 0x00, 0x3c, 0x52, 0x7d, 0xa8, 0x49, 0x3b, 0x2d, 0xeb, 0x25,
   0x49, 0xfa, 0xa3, 0xaa, 0x39, 0xa7, 0xc5, 0xa7, 0x50, 0x11, 0x36, 0xfb,
   0xc6, 0x67, 0x4a, 0xf5, 0xa5, 0x12, 0x65, 0x7e, 0xb0, 0xdf, 0xaf, 0x4e,
   0xb3, 0x61, 0x7f, 0x2f}
};

}  // namespace nikon
}  // namespace makernote
}  // namespace nexif
