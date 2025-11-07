#include "neonexif/tiff_tags.hpp"
#include "neonexif/reader.hpp"

#include <cassert>
#include <cstring>
#include <optional>

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
  constexpr static int BINARY_SIZE = 2 + 2 + 4 + 4;
  uint16_t tag;
  DType type;
  uint32_t count;
  uint32_t value;

  int32_t size() const
  {
    return count * size_of_dtype(type);
  }
};
static_assert(sizeof(ifd_entry) == ifd_entry::BINARY_SIZE);

inline ifd_entry read_ifd_entry(Reader &r)
{
  uint16_t tag = r.read_u16();
  DType type = (DType)r.read_u16();
  uint32_t count = r.read_u32();
  uint32_t value = r.read_u32();
  DEBUG_PRINT("IFD entry {0x%04x %-20s, %x:%-10s, %6d, %08x = %u}", tag, to_str(tag, -1), (int)type, to_str(type), count, value, value);
  return {tag, type, count, value};
}

inline size_t write_ifd_entry(Writer &w, const ifd_entry &e)
{
  size_t pos = w.current_in_tiff_pos();
  w.write_u16(e.tag);
  w.write_u16((uint16_t)e.type);
  w.write_u32(e.count);
  w.write_u32(e.value);
  return pos;
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
      inline_data = nexif::byteswap(inline_data);
    }
    // Data is inline
    std::memcpy(&t, ((char *)&inline_data) + idx * elem_size, elem_size);
  } else {
    // Data is elsewhere
    std::memcpy(&t, r.data + entry.value + elem_size * idx, elem_size);
  }
  if (r.byte_order == Minolta) {
    t = nexif::byteswap(t);
  }
  return t;
}

const char *to_str(uint16_t tag, uint16_t ifd_bit)
{
#define TAG_TO_STR(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count) \
  if (tag == _tag##u && (ifd_bit & _ifd_bitmask)) {                                   \
    return #_name;                                                                    \
  }
  ALL_IFD0_TAGS(TAG_TO_STR);
  ALL_EXIF_TAGS(TAG_TO_STR);
#undef TAG_TO_STR
  return nullptr;
}

template <typename TagInfo>
inline ParseResult<bool> parse_tag(Reader &r, Tag<typename TagInfo::cpp_type> &tag, const ifd_entry &entry, uint16_t ifd_bit)
{
  using BType = base_type<typename TagInfo::scalar_cpp_type>::type;
  constexpr uint16_t tag_idval = TagInfo::TagId;
  if (entry.tag == tag_idval) {
    const char *tag_str = to_str(entry.tag, ifd_bit);
    bool matches = matches_dtype<BType>(entry.type);
    bool fits = fits_dtype<BType>(entry.type);
    if (matches || fits) {
      if (TagInfo::count_spec::exif_count > 0
          && entry.count != TagInfo::count_spec::exif_count
          && !TagInfo::count_spec::exif_var) {
        LOG_WARNING(r, "Warning: unexpected count for:", tag_str);
      }
      if (!matches) {
        LOG_WARNING(r, "Warning: dtype did not match, but fits.", tag_str);
      }
      if constexpr (std::is_same_v<BType, CharData>) {
        ASSERT_OR_PARSE_ERROR(
          r.exif_data->string_data_ptr + entry.count < sizeof(r.exif_data->string_data),
          INTERNAL_ERROR, "Internal error: no enough string space.", tag_str
        );
        if (entry.count <= 4) {
          tag.value.set(r.store_string_data((char *)&entry.value, entry.count), entry.count);
          DEBUG_PRINT("store inline string data of length %d: %s", entry.count, tag.value.data());
        } else {
          tag.value.set(r.store_string_data(r.data + entry.value, entry.count), entry.count);
          DEBUG_PRINT("store external string data of length %d: %s", entry.count, tag.value.data());
        }
        tag.parsed_from = tag_idval;
        tag.is_set = true;
        return true;
      } else if constexpr (std::is_same_v<BType, rational64s> || std::is_same_v<BType, rational64u>) {
        int mark = r.ptr;
        r.seek(entry.value);
        if constexpr (TagInfo::count_spec::exif_count == 1) {
          tag.value.num = r.read_u32();
          tag.value.denom = r.read_u32();
        } else {
          for (int i = 0; i < entry.count; ++i) {
            typename TagInfo::scalar_cpp_type val;
            val.num = r.read_u32();
            val.denom = r.read_u32();
            if constexpr (TagInfo::count_spec::exif_var) {
              tag.value.push_back(val);
            } else {
              tag.value[i] = val;
            }
          }
        }
        tag.parsed_from = tag_idval;
        tag.is_set = true;
        r.seek(mark);
        return true;
      } else if constexpr (std::is_same_v<BType, DateTime>) {
        ASSERT_OR_PARSE_ERROR(entry.count >= 18, CORRUPT_DATA, "DateTime value not long enough", tag_str);
        std::string_view str{r.data + entry.value, entry.count};
        int Y, M, D, h, m, s;
        DEBUG_PRINT("Date string: %.*s\n", int(str.length()), str.data());
        std::sscanf(str.data(), "%04d:%02d:%02d %02d:%02d:%02d", &Y, &M, &D, &h, &m, &s);
        tag.value.year = Y, tag.value.month = M, tag.value.day = D;
        tag.value.hour = h, tag.value.minute = m, tag.value.second = s;
        tag.is_set = true;
        tag.parsed_from = tag_idval;
        return true;
      } else if (entry.size() <= 4) {
        tag.value = (typename TagInfo::cpp_type)fetch_entry_value<BType>(entry, 0, r);
        tag.parsed_from = tag_idval;
        tag.is_set = true;
        return true;
      }
      // TODO count > 1 not yet implemented
      return PARSE_ERROR(UNKNOWN_FILE_TYPE, "tag dtype parser not implemented", tag_str);
    } else {
      LOG_WARNING(r, "Dtype in tag is incorrect", tag_str);
      return true;
      // return PARSE_ERROR(CORRUPT_DATA, "dtype in tag is incorrect", tag_str);
    }
  }
  return false;
}

std::optional<ParseError> parse_subsectime_to_millis(Reader &r, const ifd_entry &entry, uint16_t *millis)
{
  int32_t s = entry.size();
  char buf[16] = {0};
  if (s <= 4) {
    std::memcpy(buf, &entry.value, s);
  } else {
    std::memcpy(buf, r.data + entry.value, std::min(int32_t(sizeof(buf)), s));
  }
  buf[std::min(15, s)] = 0;
  int val = std::atoi(buf);
  s--;  // We don't care about the null-terminator for scaling it to millis.
  for (int i = 0; s + i < 3; ++i) {
    val *= 10;
  }
  if (s > 3) {
    int div = 1;
    for (int i = 0; s - i > 3; ++i) {
      div *= 10;
    }
    s = (s + div / 2) / div;  // With proper rounding.
  }
  *millis = val;
  return std::nullopt;
}

#define PARSE_TAG(_struct, _name, _entry, _ifd_bits)             \
  {                                                              \
    using tag_info = TagInfo<(uint16_t)TagId::_name, _ifd_bits>; \
    static_assert(tag_info::defined);                            \
    assert(&(_struct) != nullptr);                               \
    auto tag_result = parse_tag<tag_info>(                       \
      r, _struct._name, _entry, _ifd_bits                        \
    );                                                           \
    if (!tag_result)                                             \
      return tag_result.error();                                 \
    else if (tag_result.value())                                 \
      continue;                                                  \
  }

#define PARSE_TAG_CUSTOM(_name, _ifd_bits, _parse_lambda)                  \
  {                                                                        \
    using tag_info = TagInfo<(uint16_t)TagId::_name, _ifd_bits>;           \
    static_assert(tag_info::defined);                                      \
    if (tag_info::TagId == entry.tag) {                                    \
      if (auto err = [&]() -> std::optional<ParseError> _parse_lambda()) { \
        return err;                                                        \
      }                                                                    \
      continue;                                                            \
    }                                                                      \
  }

ParseResult<bool> find_subifd(Reader &r, ifd_entry &entry, const char *tag_str)
{
  if (entry.tag == uint16_t(TagId::exif_offset)) {
    ASSERT_OR_PARSE_ERROR(entry.type == DType::LONG, CORRUPT_DATA, "IFD EXIF type wrong", tag_str);
    ASSERT_OR_PARSE_ERROR(entry.count == 1, CORRUPT_DATA, "Only one IDF EXIF offset expected", tag_str);
    DEBUG_PRINT("Found EXIF SubIFD offset: %d", entry.value);
    r.subifd_refs.push_back({entry.value, Reader::SubIFDRef::EXIF});
    return true;
  }
  if (entry.tag == uint16_t(TagId::sub_ifd_offset)) {
    ASSERT_OR_PARSE_ERROR(entry.type == DType::LONG, CORRUPT_DATA, "SubIFD datatype wrong", tag_str);
    for (int i = 0; i < entry.count; ++i) {
      uint32_t offset = fetch_entry_value<uint32_t>(entry, i, r);
      DEBUG_PRINT("Found SubIFD: %d", offset);
      r.subifd_refs.push_back({offset, Reader::SubIFDRef::OTHER});
    }
    return true;
  }
  if (entry.tag == uint16_t(TagId::makernote) || entry.tag == uint16_t(TagId::makernote_alt)) {
    ASSERT_OR_PARSE_ERROR(entry.type == DType::UNDEFINED, CORRUPT_DATA, "MakerNote datatype wrong", tag_str);
    DEBUG_PRINT("Found MakerNote SubIFD: offset=%d size=%d", entry.value, entry.count);
    r.subifd_refs.push_back({entry.value, Reader::SubIFDRef::MAKERNOTE});
    return true;
  }
  return false;
}
#define FIND_SUBIFDS()                                     \
  {                                                        \
    ParseResult<bool> pr = find_subifd(r, entry, tag_str); \
    if (!pr) {                                             \
      return pr.error();                                   \
    } else if (pr.value()) {                               \
      continue;                                            \
    }                                                      \
  }

std::optional<ParseError> parse_makernote_ifd(Reader &r, ImageData *current_image, ExifData &data, uint32_t *next_offset, uint32_t ifd_type)
{
  r.seek(*next_offset);

  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("Num IFD entries: %d", num_entries);
  assert(num_entries < 1000);
  Indenter indenter;
  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);
    Indenter indenter;
  }
  uint32_t next_ifd_offset = r.read_u32();
  DEBUG_PRINT("Next IFD offset: %d\n", next_ifd_offset);
  *next_offset = next_ifd_offset;

  return std::nullopt;
}

std::optional<ParseError> parse_exif_ifd(Reader &r, ExifData &data, uint32_t exif_offset, uint32_t *next_offset)
{
  r.seek(exif_offset);

  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("Num EXIF IFD entries: %d", num_entries);
  assert(num_entries < 1000);
  Indenter indenter;
  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);
    const char *tag_str = to_str(entry.tag, IFD_BitMasks::IFD_EXIF);
    Indenter indenter;

    FIND_SUBIFDS();

    // clang-format off
#define PARSE_EXIF_TAG(_name) PARSE_TAG(data.exif, _name, entry, IFD_BitMasks::IFD_EXIF)
    PARSE_EXIF_TAG(exposure_time);
    PARSE_EXIF_TAG(f_number);
    PARSE_EXIF_TAG(iso);
    PARSE_EXIF_TAG(exposure_program);
    PARSE_TAG(data.exif, focal_length, entry, IFD_BitMasks::IFD_ALL); // Can appear in both?
    PARSE_EXIF_TAG(exif_version);
    PARSE_EXIF_TAG(date_time_original);
    PARSE_EXIF_TAG(date_time_digitized);
    PARSE_TAG_CUSTOM(subsectime, IFD_BitMasks::IFD_EXIF, {
      return parse_subsectime_to_millis(r, entry, &data.date_time.value.millis);
    });
    PARSE_TAG_CUSTOM(subsectime_original, IFD_BitMasks::IFD_EXIF, {
      return parse_subsectime_to_millis(r, entry, &data.exif.date_time_original.value.millis);
    });
    PARSE_TAG_CUSTOM(subsectime_digitized, IFD_BitMasks::IFD_EXIF, {
      return parse_subsectime_to_millis(r, entry, &data.exif.date_time_digitized.value.millis);
    });

    PARSE_EXIF_TAG(camera_owner_name         );
    PARSE_EXIF_TAG(body_serial_number        );
    PARSE_EXIF_TAG(lens_make                 );
    PARSE_EXIF_TAG(lens_model                );
    PARSE_EXIF_TAG(lens_serial_number        );
    PARSE_EXIF_TAG(image_title               );
    PARSE_EXIF_TAG(photographer              );
    PARSE_EXIF_TAG(image_editor              );
    PARSE_EXIF_TAG(raw_developing_software   );
    PARSE_EXIF_TAG(image_editing_software    );
    PARSE_EXIF_TAG(metadata_editing_software );

#undef PARSE_EXIF_TAG
  }

  uint32_t next_ifd_offset = r.read_u32();
  DEBUG_PRINT("Next IFD offset: %d\n", next_ifd_offset);
  *next_offset = next_ifd_offset;

  return std::nullopt;
}

std::optional<ParseError> parse_tiff_ifd(Reader &r, ExifData &data, uint32_t ifd_offset, ImageData *current_image, int16_t ifd_type, uint32_t *next_offset)
{
  r.seek(ifd_offset);

  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("IFD at offset: %d -> Num entries: %d", ifd_offset, num_entries);

  Indenter indenter;

  // Dummy tags for reading purposes which will be post-processed.
  Tag<uint32_t> tag_subfile_type;
  Tag<uint16_t> tag_oldsubfile_type;

  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);
    ASSERT_OR_PARSE_ERROR(
      entry.type >= DType::BYTE && entry.type <= DType::DOUBLE,
      CORRUPT_DATA, "Unknown IFD entry data type", nullptr
    );
    Indenter indenter;

    const char *tag_str = to_str(entry.tag, IFD_BitMasks::IFD_01);

    FIND_SUBIFDS();

    if (entry.tag == (uint16_t)TagId::subfile_type) {
      // Create new SubIFD!
    }
#define PARSE_ROOT_TAG(_name) PARSE_TAG(data, _name, entry, IFD_BitMasks::IFD_01)
    if (ifd_type & IFD_BitMasks::IFD_01) {
      PARSE_ROOT_TAG(copyright);
      PARSE_ROOT_TAG(artist);
      PARSE_ROOT_TAG(make);
      PARSE_ROOT_TAG(model);
      PARSE_ROOT_TAG(software);
      PARSE_ROOT_TAG(processing_software);
      PARSE_ROOT_TAG(date_time);
      PARSE_ROOT_TAG(apex_aperture_value);
      PARSE_ROOT_TAG(apex_shutter_speed_value);

      PARSE_ROOT_TAG(color_matrix_1);
      PARSE_ROOT_TAG(color_matrix_2);
      PARSE_ROOT_TAG(reduction_matrix_1);
      PARSE_ROOT_TAG(reduction_matrix_2);
      PARSE_ROOT_TAG(calibration_matrix_1);
      PARSE_ROOT_TAG(calibration_matrix_2);
      PARSE_ROOT_TAG(calibration_illuminant_1);
      PARSE_ROOT_TAG(calibration_illuminant_2);
      PARSE_ROOT_TAG(as_shot_neutral);
      PARSE_ROOT_TAG(as_shot_white_xy);
      PARSE_ROOT_TAG(analog_balance);

      // Can appear in both!
      PARSE_TAG(data.exif, focal_length, entry, IFD_BitMasks::IFD_ALL);
    }

#undef PARSE_ROOT_TAG

    if (current_image != nullptr) {
#define PARSE_IFD0_TAG(_name) PARSE_TAG((*current_image), _name, entry, IFD_BitMasks::IFD_01)
      if (ifd_type & IFD_BitMasks::IFD_01) {
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
      }
#undef PARSE_IFD0_TAG
    }
    // clang-format on

    parse_tag<tiff::tag_subfile_type>(r, tag_subfile_type, entry, IFD_BitMasks::IFD_01);
    parse_tag<tiff::tag_old_subfile_type>(r, tag_oldsubfile_type, entry, IFD_BitMasks::IFD_01);
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

  return std::nullopt;
}

std::optional<ParseError> read_tiff(Reader &r, ExifData &data)
{
  r.seek(4);
  uint32_t root_ifd_offset = r.read_u32();
  DEBUG_PRINT("root IFD offset: %d", root_ifd_offset);

  int ifd_offset = root_ifd_offset;
  uint16_t ifd_type = IFD_BitMasks::IFD0;
  for (int ifd_idx = 0; ifd_idx < 5; ++ifd_idx) {
    ImageData *current_image = &data.images[data.num_images++];

    DEBUG_PRINT("move to IFD at offset: %d\n", ifd_offset);
    uint32_t next_ifd_offset;
    if (auto error = parse_tiff_ifd(r, data, ifd_offset, current_image, ifd_type, &next_ifd_offset)) {
      return error;
    }

    ASSERT_OR_PARSE_ERROR(ifd_offset % 2 == 0, CORRUPT_DATA, "IFD must align to word boundary", "root IFD");
    if (next_ifd_offset < r.file_length && next_ifd_offset != 0) {
      ifd_offset = next_ifd_offset;
      ifd_type = IFD_BitMasks::IFD1;  // We now go to thumbnails
    } else {
      break;
    }
  }

  for (int i = 0; i < r.subifd_refs.num; ++i) {
    auto &ref = r.subifd_refs.values[i];
    uint32_t next_offset = ref.offset;
    switch (ref.type) {
      case Reader::SubIFDRef::EXIF:
        do {
          if (auto error = parse_exif_ifd(r, data, next_offset, &next_offset)) {
            return error;
          }
        } while (next_offset != 0);
        ref.parsed = true;
        break;
      case Reader::SubIFDRef::OTHER:
        do {
          ImageData *current_image = &data.images[data.num_images++];
          parse_tiff_ifd(r, data, next_offset, current_image, ifd_type, &next_offset);
        } while (next_offset != 0);
        ref.parsed = true;
        break;
      case Reader::SubIFDRef::GPS:
      case Reader::SubIFDRef::INTEROP:
      case Reader::SubIFDRef::MAKERNOTE:
        // Unsupported!
        DEBUG_PRINT("Unsupported IFD type skipped: %d\n", ref.type);
        break;
    }
  }

  return std::nullopt;
}

struct IFD_Writer {
  uint32_t ifd_offset{0};
  uint32_t data_offset{0};
  std::vector<uint8_t> tags;  ///< Tags without IFD num-tags header or next-IFD footer
  std::vector<uint8_t> data;  ///< Arbitrary data
  Writer tags_writer{tags};
  Writer data_writer{data};
  int num_tags_written{0};
  int num_offsets_to_adjust{0};
};

template <typename TagInfo>
size_t write_tiff_tag_scalar(IFD_Writer &w, typename TagInfo::cpp_type value)
{
  static_assert(TagInfo::defined);
  // Variable count is compatible with count 1
  static_assert(TagInfo::count_spec::cpp_count == 1 || TagInfo::count_spec::cpp_count == 0);
  ifd_entry e;
  e.type = TagInfo::tiff_type;
  e.tag = TagInfo::TagId;
  e.count = 1;
  int required_size = size_of_dtype(e.type) * e.count;
  if (required_size <= sizeof(uint32_t)) {
    std::memcpy(&e.value, &value, std::min(sizeof(required_size), sizeof(value)));
  } else {
    // The data should be stored elsewhere, in the data section,
    // after the IFD. We don't know the offset to the data section yet
    // as that will be determined by how many tags we write in this
    // IFD. So instead, we leave a placeholder, and we will count
    // how many offsets need to be rewritten. Detecting which ones
    // is a matter of doing the "required_size" calculation above.
    auto offset = w.data_writer.write(value);
    e.value = (uint32_t)(offset);
    w.num_offsets_to_adjust++;
  }
  w.num_tags_written++;
  return write_ifd_entry(w.tags_writer, e);
}

template <typename TagInfo>
size_t write_tiff_tag_string(IFD_Writer &w, const char *str, uint32_t length)
{
  static_assert(TagInfo::defined);
  static_assert(TagInfo::tiff_type == DType::ASCII);
  // Variable count is compatible with count 1
  static_assert(TagInfo::count_spec::cpp_count == 1);
  ifd_entry e;
  e.type = TagInfo::tiff_type;
  e.tag = TagInfo::TagId;
  e.count = length;
  if (length <= sizeof(uint32_t)) {
    e.value = 0;
    std::memcpy(&e.value, str, length);
  } else {
    // The data should be stored elsewhere, in the data section,
    // after the IFD. We don't know the offset to the data section yet
    // as that will be determined by how many tags we write in this
    // IFD. So instead, we leave a placeholder, and we will count
    // how many offsets need to be rewritten. Detecting which ones
    // is a matter of doing the "required_size" calculation above.
    auto offset = w.data_writer.write_string(str, length);
    e.value = (uint32_t)(offset);
    w.num_offsets_to_adjust++;
  }
  w.num_tags_written++;
  return write_ifd_entry(w.tags_writer, e);
}

template <typename TagInfo>
void write_tiff_tag(IFD_Writer &w, const Tag<typename TagInfo::cpp_type> &tag)
{
  if (tag.is_set) {
    using cppt = typename TagInfo::cpp_type;
    if constexpr (std::is_same_v<cppt, CharData>) {
      write_tiff_tag_string<TagInfo>(w, tag.value.data(), tag.value.length);
    } else if constexpr (std::is_same_v<cppt, DateTime>) {
      char buffer[20];
      const DateTime &dt = tag.value;
      int num = std::snprintf(
        buffer, 20, "%04d:%02d:%02d %02d:%02d:%02d",
        dt.year, dt.month, dt.day,
        dt.hour, dt.minute, dt.second
      );
      if (num != 19) {
        DEBUG_PRINT("Unexpected date time string length: %s (len = %d)", buffer, num);
      }
      write_tiff_tag_string<TagInfo>(w, buffer, num + 1);
    } else {
      if constexpr (TagInfo::count_spec::cpp_count == 1) {
        if constexpr (std::is_trivial_v<cppt>) {
          write_tiff_tag_scalar<TagInfo>(w, tag.value);
        }
      }
    }
  }
}

struct OutstandingOffset {
  size_t ifd_offset;
  size_t in_ifd_entry_offset;
  enum { invalid,
         waiting,
         written } state{invalid};

  OutstandingOffset() = default;
  OutstandingOffset(const OutstandingOffset &) = delete;
  OutstandingOffset(OutstandingOffset &&o)
  {
    operator=(std::move(o));
  }
  OutstandingOffset &operator=(OutstandingOffset &&o)
  {
    ifd_offset = o.ifd_offset;
    in_ifd_entry_offset = o.in_ifd_entry_offset;
    state = o.state;
    o.state = invalid;
    return *this;
  }

  ~OutstandingOffset()
  {
    assert(state != waiting);
  }

  void set(Writer &w, uint32_t offset)
  {
    w.overwrite_u32(w.tiff_base_offset + ifd_offset + in_ifd_entry_offset + 8, offset);
    state = written;
  }
};

template <typename TagInfo>
OutstandingOffset write_outstanding_tiff_offset_tag(IFD_Writer &w)
{
  static_assert(std::is_same_v<typename TagInfo::cpp_type, uint32_t>);
  size_t in_ifd_offset = write_tiff_tag_scalar<TagInfo>(w, 0xffff);
  OutstandingOffset oo;
  oo.ifd_offset = w.ifd_offset;
  oo.in_ifd_entry_offset = in_ifd_offset + 2;  // 2 for num_tags
  oo.state = OutstandingOffset::waiting;
  return oo;
}

void add_data_offset_to_non_inlined_values(IFD_Writer &w)
{
  int num_adjusted = 0;
  assert(w.num_offsets_to_adjust == 0 || w.data_offset != 0);
  for (int i = 0; i < w.num_tags_written; ++i) {
    size_t ifd_entry_offset = sizeof(ifd_entry) * i;

    DType type = (DType)w.tags_writer.read_u16(ifd_entry_offset + 2);
    uint32_t count = w.tags_writer.read_u32(ifd_entry_offset + 4);
    int required_bytes = size_of_dtype(type) * count;
    assert(required_bytes > 0);
    if (required_bytes > 4) {
      auto value_offset = ifd_entry_offset + 8;
      uint32_t offset = w.tags_writer.read_u32(value_offset);
      DEBUG_PRINT("rewrite offset value: %u to %u", offset, offset + w.data_offset);
      w.tags_writer.overwrite(value_offset, offset + w.data_offset);
      num_adjusted++;
    }
  }
  assert(num_adjusted == w.num_offsets_to_adjust);
}

size_t write_ifd(Writer &w, IFD_Writer &ifd_w)
{
  ifd_w.ifd_offset = w.current_in_tiff_pos();
  ifd_w.data_offset = ifd_w.ifd_offset
    + sizeof(uint16_t)                            // num tags
    + ifd_w.num_tags_written * sizeof(ifd_entry)  // tags
    + sizeof(uint32_t)                            // offset to next ifd
    ;
  DEBUG_PRINT(" ifd_offset: %u", ifd_w.ifd_offset);
  DEBUG_PRINT("data_offset: %u", ifd_w.data_offset);
  add_data_offset_to_non_inlined_values(ifd_w);

  w.write_u16(ifd_w.num_tags_written);
  w.write_all(ifd_w.tags);
  DEBUG_PRINT("tags data size: %zu", ifd_w.tags.size());
  auto next_ifd_offset_pos = w.write_u32(0);  // next-ifd

  w.write_all(ifd_w.data);

  return next_ifd_offset_pos;
}

size_t write_tiff(Writer &w, const ExifData &data)
{
  size_t tiff_header_pos = w.current_in_tiff_pos();
  // Intel byte order!
  w.write_u8('I');
  w.write_u8('I');
  w.write_u8(42);
  w.write_u8(0);

  uint32_t root_ifd_offset = w.current_in_tiff_pos() + sizeof(uint32_t);
  uint32_t exif_ifd_offset;
  size_t exif_entry_offset;
  OutstandingOffset outstanding_exif_offset;

  assert(root_ifd_offset == 8);
  DEBUG_PRINT("root ifd offset: %u", root_ifd_offset);
  w.write_u32(root_ifd_offset);

  // Root IFD contains ExifIFD offset
  {
    Indenter _;
    IFD_Writer root_ifd{root_ifd_offset};

    write_tiff_tag<tag_copyright>(root_ifd, data.copyright);
    write_tiff_tag<tag_artist>(root_ifd, data.artist);
    write_tiff_tag<tag_make>(root_ifd, data.make);
    write_tiff_tag<tag_model>(root_ifd, data.model);
    write_tiff_tag<tag_software>(root_ifd, data.software);
    write_tiff_tag<tag_processing_software>(root_ifd, data.processing_software);
    write_tiff_tag<tag_date_time>(root_ifd, data.date_time);
    write_tiff_tag<tag_apex_aperture_value>(root_ifd, data.apex_aperture_value);
    write_tiff_tag<tag_apex_shutter_speed_value>(root_ifd, data.apex_shutter_speed_value);
    outstanding_exif_offset = write_outstanding_tiff_offset_tag<tag_exif_offset>(root_ifd);
    write_ifd(w, root_ifd);
  }

  // Exif IFD
  {
    exif_ifd_offset = w.current_in_tiff_pos();
    outstanding_exif_offset.set(w, exif_ifd_offset);
    DEBUG_PRINT("exif ifd offset: %u", exif_ifd_offset);
    Indenter _;
    IFD_Writer exif_ifd{exif_ifd_offset};
    write_tiff_tag_scalar<tag_subfile_type>(exif_ifd, 1);

    write_tiff_tag<tag_exposure_time>(exif_ifd, data.exif.exposure_time);
    write_tiff_tag<tag_f_number>(exif_ifd, data.exif.f_number);
    write_tiff_tag<tag_focal_length>(exif_ifd, data.exif.focal_length);
    write_tiff_tag<tag_iso>(exif_ifd, data.exif.iso);
    write_tiff_tag<tag_exposure_program>(exif_ifd, data.exif.exposure_program);
    write_tiff_tag<tag_date_time_original>(exif_ifd, data.exif.date_time_original);
    write_tiff_tag<tag_date_time_digitized>(exif_ifd, data.exif.date_time_digitized);
    // TODO: write subsec fields, and timezone offset

    // clang-format off
    write_tiff_tag<tag_camera_owner_name         >(exif_ifd, data.exif.camera_owner_name          );
    write_tiff_tag<tag_body_serial_number        >(exif_ifd, data.exif.body_serial_number         );
    write_tiff_tag<tag_lens_make                 >(exif_ifd, data.exif.lens_make                  );
    write_tiff_tag<tag_lens_model                >(exif_ifd, data.exif.lens_model                 );
    write_tiff_tag<tag_lens_serial_number        >(exif_ifd, data.exif.lens_serial_number         );
    write_tiff_tag<tag_image_title               >(exif_ifd, data.exif.image_title                );
    write_tiff_tag<tag_photographer              >(exif_ifd, data.exif.photographer               );
    write_tiff_tag<tag_image_editor              >(exif_ifd, data.exif.image_editor               );
    write_tiff_tag<tag_raw_developing_software   >(exif_ifd, data.exif.raw_developing_software    );
    write_tiff_tag<tag_image_editing_software    >(exif_ifd, data.exif.image_editing_software     );
    write_tiff_tag<tag_metadata_editing_software >(exif_ifd, data.exif.metadata_editing_software  );
    // clang-format on

    write_ifd(w, exif_ifd);
  }

  size_t final_pos = w.current_in_tiff_pos();
  return final_pos - tiff_header_pos;
}

}  // namespace tiff
}  // namespace nexif
