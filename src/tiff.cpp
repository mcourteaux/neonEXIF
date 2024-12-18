#include "tiff_tags.hpp"
#include "reader.hpp"

#include <cassert>
#include <cstring>

namespace nexif {
namespace tiff {

template <typename T, bool is_enum = std::is_enum_v<T>>
struct base_type {
  using type = T;
};

template <typename T>
struct base_type<T, true> {
  using type = std::underlying_type_t<T>;
};

struct ifd_entry {
  uint16_t tag;
  DType type;
  uint32_t count;
  uint32_t value;

  int32_t size() const
  {
    return count * size_of_dtype(type);
  }
};

inline ifd_entry read_ifd_entry(Reader &r)
{
  uint16_t tag = r.read_u16();
  DType type = (DType)r.read_u16();
  uint32_t count = r.read_u32();
  uint32_t value = r.read_u32();
  DEBUG_PRINT("IFD entry {0x%04x %-20s, %x:%-10s, %3d, %08x = %u}", tag, to_str(tag, -1), (int)type, to_str(type), count, value, value);
  return {tag, type, count, value};
}

template <typename T>
T fetch_entry_value(const ifd_entry &entry, int idx, Reader &r)
{
  assert(idx < entry.count);
  int32_t elem_size = size_of_dtype(entry.type);
  assert(sizeof(T) >= elem_size);
  T t{0};
  if (entry.size() <= 4) {
    uint32_t inline_data = entry.value;
    if (r.byte_order == Minolta) {
      // Unswap it
      inline_data = std::byteswap(inline_data);
    }
    // Data is inline
    std::memcpy(&t, ((char *)&inline_data) + idx * elem_size, elem_size);
  } else {
    // Data is elsewhere
    std::memcpy(&t, r.data + entry.value + elem_size * idx, elem_size);
  }
  if (r.byte_order == Minolta) {
    t = std::byteswap(t);
  }
  return t;
}

const char *to_str(uint16_t tag, uint16_t ifd_bit)
{
#define TAG_TO_STR(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count) \
  if (tag == _tag##u && (ifd_bit & _ifd_bitmask)) {                                   \
    return #_name;                                                                    \
  }
  ALL_IFD0_TAGS(TAG_TO_STR, , , );
  ALL_EXIF_TAGS(TAG_TO_STR, , , );
#undef TAG_TO_STR
  return nullptr;
}

template <typename T, int ExpectedCount>
inline ParseResult<bool> parse_tag(Reader &r, TagId TagID, Tag<T> &tag, const ifd_entry &entry, uint16_t ifd_bit)
{
  using BType = base_type<T>::type;
  if (entry.tag == (uint16_t)TagID) {
    bool matches = matches_dtype<BType>(entry.type);
    bool fits = fits_dtype<BType>(entry.type);
    if (matches || fits) {
      if (ExpectedCount > 0 && entry.count != ExpectedCount) {
        LOG_WARNING(r, "Warning: unexpected count for:", to_str(entry.tag, ifd_bit));
      }
      if (!matches) {
        LOG_WARNING(r, "Warning: dtype did not match, but fits.", to_str(entry.tag, ifd_bit));
      }
      if constexpr (std::is_same_v<BType, CharData>) {
        ASSERT_OR_PARSE_ERROR(
          r.exif_data->string_data_ptr + entry.count < sizeof(r.exif_data->string_data),
          INTERNAL_ERROR, "Internal error: no enough string space."
        );
        if (entry.count <= 4) {
          DEBUG_PRINT("store inline string data of length %d", entry.count);
          tag.value = r.store_string_data((char *)&entry.value, entry.count);
        } else {
          DEBUG_PRINT("store external string data of length %d", entry.count);
          tag.value = r.store_string_data(r.data + entry.value, entry.count);
        }
        tag.parsed_from = (uint16_t)TagID;
        tag.is_set = true;
        return true;
      } else if constexpr (std::is_same_v<BType, rational64s> || std::is_same_v<BType, rational64u>) {
        int mark = r.ptr;
        r.seek(entry.value);
        tag.value.num = r.read_u32();
        tag.value.denom = r.read_u32();
        tag.parsed_from = (uint16_t)TagID;
        tag.is_set = true;
        r.seek(mark);
        return true;
      } else if (entry.size() <= 4) {
        tag.value = (T)fetch_entry_value<BType>(entry, 0, r);
        tag.parsed_from = (uint16_t)TagID;
        tag.is_set = true;
        return true;
      }
      // TODO count > 1 not yet implemented
      return PARSE_ERROR(UNKNOWN_FILE_TYPE, "tag dtype parser not implemented");
    } else {
      std::abort();
      return PARSE_ERROR(CORRUPT_DATA, "dtype in tag is incorrect");
    }
  }
  return false;
}

#define PARSE_TAG(_struct, _name, _entry, _ifd_bit)                   \
  {                                                                   \
    using tag_info = TagInfo<(uint16_t)TagId::_name, _ifd_bit>;       \
    static_assert(tag_info::defined);                                 \
    assert(&(_struct) != nullptr);                                    \
    auto tag_result = parse_tag<tag_info::cpp_type, tag_info::count>( \
      r, TagId::_name, _struct._name, _entry, _ifd_bit                \
    );                                                                \
    if (!tag_result)                                                  \
      return tag_result.error();                                      \
    else if (tag_result.value())                                      \
      continue;                                                       \
  }

std::optional<ParseError> parse_exif_ifd(Reader &r, ExifData &data, uint32_t *next_offset)
{
  r.seek(r.ifd_offset_exif);
  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("Num IFD Exif entries: %d", num_entries);
  assert(num_entries < 1000);
  Indenter indenter;
  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);

    // clang-format off
    PARSE_TAG(data.exif, exposure_time    , entry, IFD_BitMasks::IFD_EXIF);
    PARSE_TAG(data.exif, f_number         , entry, IFD_BitMasks::IFD_EXIF);
    PARSE_TAG(data.exif, iso              , entry, IFD_BitMasks::IFD_EXIF);
    PARSE_TAG(data.exif, exposure_program , entry, IFD_BitMasks::IFD_EXIF);
    PARSE_TAG(data.exif, focal_length     , entry, IFD_BitMasks::IFD_EXIF);
    PARSE_TAG(data.exif, aperture_value   , entry, IFD_BitMasks::IFD_EXIF);
    PARSE_TAG(data.exif, exif_version     , entry, IFD_BitMasks::IFD_EXIF);
    // clang-format on
  }

  uint32_t next_ifd_offset = r.read_u32();
  DEBUG_PRINT("Next IFD offset: %d\n", next_ifd_offset);
  *next_offset = next_ifd_offset;

  return std::nullopt;
}

std::optional<ParseError> read_tiff_ifd(Reader &r, ExifData &data, uint32_t ifd_offset, ImageData *current_image, uint32_t *next_offset)
{
  r.seek(ifd_offset);

  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("IFD at offset: %d -> Num entries: %d", ifd_offset, num_entries);

  Indenter indenter;

  // Dummy tags for reading purposes which will be post-processed.
  Tag<uint32_t> tag_subfile_type;
  Tag<uint16_t> tag_oldsubfile_type;

  ifd_entry subifd_entries[10];
  int subifd_counter = 0;

  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);
    ASSERT_OR_PARSE_ERROR(
      true
        && entry.type >= DType::BYTE
        && entry.type <= DType::SRATIONAL
        && entry.type != (DType)8,
      CORRUPT_DATA, "Unknown IFD entry data type"
    );

    if (entry.tag == (uint16_t)TagId::subfile_type) {
      // Create new SubIFD!
    }

#define PARSE_IFD0_TAG(_name) PARSE_TAG((*current_image), _name, entry, IFD_BitMasks::IFD_ALL_NORMAL)
#define PARSE_ROOT_TAG(_name) PARSE_TAG(data, _name, entry, IFD_BitMasks::IFD_ALL_NORMAL)
    PARSE_ROOT_TAG(copyright);
    PARSE_ROOT_TAG(artist);
    PARSE_ROOT_TAG(make);
    PARSE_ROOT_TAG(model);
    PARSE_ROOT_TAG(software);
    PARSE_IFD0_TAG(image_width);
    PARSE_IFD0_TAG(image_height);
    PARSE_IFD0_TAG(compression);
    PARSE_IFD0_TAG(photometric_interpretation);
    PARSE_IFD0_TAG(orientation);
    PARSE_IFD0_TAG(samples_per_pixel);
    PARSE_IFD0_TAG(x_resolution);
    PARSE_IFD0_TAG(y_resolution);
    PARSE_IFD0_TAG(resolution_unit);

    PARSE_IFD0_TAG(data_offset);
    PARSE_IFD0_TAG(data_length);
#undef PARSE_IFD0_TAG

    if (entry.tag == uint16_t(TagId::exif_offset)) {
      ASSERT_OR_PARSE_ERROR(entry.type == DType::LONG, CORRUPT_DATA, "IFD EXIF type wrong");
      ASSERT_OR_PARSE_ERROR(entry.count == 1, CORRUPT_DATA, "Only one IDF EXIF offset expected");
      DEBUG_PRINT("Found IFD exif offset: %d", entry.value);
      r.ifd_offset_exif = entry.value;
      continue;
    }

    if (entry.tag == uint16_t(TagId::sub_ifd_offset)) {
      DEBUG_PRINT("SubIFD found: %d count=%d", entry.value, entry.count);
      assert(subifd_counter < sizeof(subifd_entries) / sizeof(subifd_entries[0]));
      subifd_entries[subifd_counter++] = entry;
    }

    parse_tag<uint32_t, 1>(r, TagId::subfile_type, tag_subfile_type, entry, IFD_BitMasks::IFD_ALL_NORMAL);
    parse_tag<uint16_t, 1>(r, TagId::old_subfile_type, tag_oldsubfile_type, entry, IFD_BitMasks::IFD_ALL_NORMAL);
  }

  if (tag_subfile_type.is_set) {
    switch (tag_subfile_type.value) {
      case 0x0: current_image->type = FULL_RESOLUTION; break;
      case 0x1: current_image->type = REDUCED_RESOLUTION; break;
    }
  } else if (tag_oldsubfile_type.is_set) {
    switch (tag_oldsubfile_type.value) {
      case 0x1: current_image->type = FULL_RESOLUTION; break;
      case 0x2: current_image->type = REDUCED_RESOLUTION; break;
    }
  }

  uint32_t next_ifd_offset = r.read_u32();
  DEBUG_PRINT("Next IFD offset: %d\n", next_ifd_offset);
  *next_offset = next_ifd_offset;

  for (int subifd_entry_idx = 0; subifd_entry_idx < subifd_counter; ++subifd_entry_idx) {
    ifd_entry entry = subifd_entries[subifd_entry_idx];
    int mark = r.ptr;
    for (int i = 0; i < entry.count; ++i) {
      DEBUG_PRINT("Parse subifd #%d / %d", i, entry.count);
      uint32_t next_subifd_offset = fetch_entry_value<uint32_t>(entry, i, r);
      while (next_subifd_offset != 0) {
        assert(data.num_images < sizeof(data.images) / sizeof(*data.images));
        current_image = &data.images[data.num_images++];
        if (auto error = read_tiff_ifd(r, data, next_subifd_offset, current_image, &next_subifd_offset)) {
          LOG_WARNING(r, "Cannot parse SubIFD", error->message);
          break;
        }
      }
    }
    r.seek(mark);
  }

  return std::nullopt;
}

std::optional<ParseError> read_tiff(Reader &r, ExifData &data)
{
  r.seek(4);
  uint32_t root_ifd_offset = r.read_u32();
  DEBUG_PRINT("root IFD offset: %d", root_ifd_offset);

  int ifd_offset = root_ifd_offset;
  for (int ifd_idx = 0; ifd_idx < 5; ++ifd_idx) {
    ImageData *current_image = &data.images[data.num_images++];

    DEBUG_PRINT("move to IFD at offset: %d\n", ifd_offset);
    uint32_t next_ifd_offset;
    if (auto error = read_tiff_ifd(r, data, ifd_offset, current_image, &next_ifd_offset)) {
      return error;
    }

    ASSERT_OR_PARSE_ERROR(ifd_offset % 2 == 0, CORRUPT_DATA, "IFD must align to word boundary");
    if (next_ifd_offset < r.file_length && next_ifd_offset != 0) {
      ifd_offset = next_ifd_offset;
    } else {
      break;
    }
  }

  if (r.ifd_offset_exif) {
    uint32_t next_offset;
    parse_exif_ifd(r, data, &next_offset);
  }

  return std::nullopt;
}

}  // namespace tiff
}  // namespace nexif
