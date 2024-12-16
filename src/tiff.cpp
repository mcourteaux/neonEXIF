#include "tiff_tags.hpp"
#include "reader.hpp"

#include <cassert>
#include <cstring>

namespace nexif {
namespace tiff {

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
  DEBUG_PRINT("IFD entry {%04x, %2x, %d, %d}", tag, (int)type, count, value);
  return {tag, type, count, value};
}

template <typename T>
T fetch_entry_value(const ifd_entry &entry, int idx, Reader &r)
{
  assert(idx < entry.count);
  int32_t elem_size = size_of_dtype(entry.type);
  T t;
  if (entry.size() <= 4) {
    // Data is inline
    std::memcpy(&t, ((char *)&entry.value) + idx * elem_size, elem_size);
  } else {
    // Data is elsewhere
    std::memcpy(&t, r.data + entry.value + elem_size * idx, elem_size);
  }
  if (r.byte_order == Minolta) {
    t = std::byteswap(t);
  }
  return t;
}

const char *to_str(uint16_t tag, IFD ifd)
{
#define TAG_TO_STR(_tag, _expected_tiff_type, _cpp_type, _name, _count) \
  if (tag == _tag##u) {                                                 \
    return #_name;                                                      \
  }
  if (ifd == IFD::IFD0) {
    ALL_IFD0_TAGS(TAG_TO_STR, , , );
  } else if (ifd == IFD::IFD_EXIF) {
    ALL_EXIF_TAGS(TAG_TO_STR, , , );
  }
  return nullptr;
}

template <typename T, int ExpectedCount>
inline ParseResult<bool> parse_tag(Reader &r, uint16_t TagID, Tag<T> &tag, const ifd_entry &entry, IFD ifd)
{
  if (entry.tag == TagID) {
    bool matches = matches_dtype<T>(entry.type);
    bool fits = fits_dtype<T>(entry.type);
    if (matches || fits) {
      if (ExpectedCount > 0 && entry.count != ExpectedCount) {
        LOG_WARNING(r, "Warning: unexpected count for:", to_str(entry.tag, ifd));
      }
      if (!matches) {
        LOG_WARNING(r, "Warning: dtype did not match, but fits.", "");
      }
      if constexpr (std::is_same_v<T, CharData>) {
        ASSERT_OR_PARSE_ERROR(
          r.exif_data->string_data_ptr + entry.count < sizeof(r.exif_data->string_data),
          INTERNAL_ERROR, "Internal error: no enough string space."
        );
        if (entry.count <= 4) {
          DEBUG_PRINT("store inline string data of length %d", entry.count);
          tag = r.store_string_data((char *)&entry.value, entry.count);
        } else {
          DEBUG_PRINT("store external string data of length %d", entry.count);
          tag = r.store_string_data(r.data + entry.value, entry.count);
        }
        tag.parsed_from = TagID;
        tag.is_set = true;
        return true;
      } else if constexpr (std::is_same_v<T, rational64s> || std::is_same_v<T, rational64u>) {
        int mark = r.ptr;
        r.seek(entry.value);
        tag.value.num = r.read_u32();
        tag.value.denom = r.read_u32();
        tag.parsed_from = TagID;
        tag.is_set = true;
        r.seek(mark);
        return true;
      } else if (entry.size() <= 4) {
        tag = entry.value;
        tag.parsed_from = TagID;
        tag.is_set = true;
        return true;
      }
      // TODO count > 1 not yet implemented
      return PARSE_ERROR(UNKNOWN_FILE_TYPE, "tag dtype parser not implemented");
    } else {
      return PARSE_ERROR(CORRUPT_DATA, "dtype in tag is incorrect");
    }
  }
  return false;
}

#define PARSE_TAG(_struct, _tag_id, _tiff_type, _cpp_type, _name, _count, _entry, _ifd)      \
  {                                                                                          \
    auto tag_result = parse_tag<_cpp_type, _count>(r, _tag_id, _struct._name, _entry, _ifd); \
    if (!tag_result)                                                                         \
      return tag_result.error();                                                             \
    else if (tag_result.value())                                                             \
      continue;                                                                              \
  }

std::optional<ParseError> parse_exif_ifd(Reader &r, ExifData &data)
{
  r.seek(r.ifd_offset_exif);
  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("Num IFD Exif entries: %d", num_entries);
  Indenter indenter;
  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);

#define FIXED_COUNT(x) x
#define PARSE_EXIF_TAG(_tag_id, _tiff_type, _cpp_type, _name, _count) \
  PARSE_TAG(data.exif, _tag_id, _tiff_type, _cpp_type, _name, _count, entry, IFD::IFD_EXIF);
    ALL_EXIF_TAGS(PARSE_EXIF_TAG, 1, FIXED_COUNT, -1);
  }
#undef PARSE_EXIF_TAG
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

#define FIXED_COUNT(x) x
#define PARSE_IFD0_TAG(_tag_id, _tiff_type, _cpp_type, _name, _count) \
  PARSE_TAG(data, _tag_id, _tiff_type, _cpp_type, _name, _count, entry, IFD::IFD_EXIF);
    ALL_IFD0_TAGS(PARSE_IFD0_TAG, 1, FIXED_COUNT, -1);
  }
#undef PARSE_IFD0_TAG


    /*
    PARSE_TAG(tag_subfile_type, entry);
    PARSE_TAG(tag_oldsubfile_type, entry);

    if (entry.tag == TagId::ExifOffset) {
      ASSERT_OR_PARSE_ERROR(entry.type == IFD_DType::LONG, CORRUPT_DATA, "IFD EXIF type wrong");
      ASSERT_OR_PARSE_ERROR(entry.count == 1, CORRUPT_DATA, "Only one IDF EXIF offset expected");
      DEBUG_PRINT("Found IFD exif offset: %d", entry.value);
      r.ifd_offset_exif = entry.value;
      continue;
    }

    if (entry.tag == TagId::SubIFDOffset) {
      DEBUG_PRINT("SubIFD found: %d count=%d", entry.value, entry.count);
      assert(subifd_counter < sizeof(subifd_entries) / sizeof(subifd_entries[0]));
      subifd_entries[subifd_counter++] = entry;
    }

    PARSE_TAG(current_image->image_width, entry);
    PARSE_TAG(current_image->image_height, entry);
    PARSE_TAG(current_image->compression, entry);
    PARSE_TAG(current_image->samples_per_pixel, entry);
    PARSE_TAG(current_image->x_resolution, entry);
    PARSE_TAG(current_image->y_resolution, entry);
    PARSE_TAG(current_image->resolution_unit, entry);

    PARSE_TAG(data.copyright, entry);
    PARSE_TAG(data.artist, entry);
    PARSE_TAG(data.make, entry);
    PARSE_TAG(data.model, entry);
    */
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
    ImageData *current_image = &data.images[ifd_idx];
    data.num_images++;

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

  parse_exif_ifd(r, data);

  return std::nullopt;
}

}  // namespace tiff
}  // namespace nexif
