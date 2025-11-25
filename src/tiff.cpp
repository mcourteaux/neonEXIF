#include "neonexif/tiff.hpp"
#include "neonexif/tiff_tags.hpp"
#include "neonexif/reader.hpp"

#include <cassert>
#include <cstring>
#include <optional>
#include <bit>

namespace nexif {
namespace makernote::nikon {
std::optional<ParseError> parse_makernote(Reader &r, ExifData &data);
}

namespace tiff {

ifd_entry read_ifd_entry(Reader &r)
{
  ifd_entry e;
  e.tag = r.read_u16();
  e.type = (DType)r.read_u16();
  if (!(e.type >= DType::BYTE && e.type <= DType::DOUBLE)) {
    r.warnings.emplace_back("Unknown IFD entry data type", nullptr);
  };
  e.count = r.read_u32();
  r.read_4bytes(e.data);
  return e;
}

inline size_t write_ifd_entry(Writer &w, const ifd_entry &e)
{
  size_t pos = w.current_in_tiff_pos();
  w.write_u16(e.tag);
  w.write_u16((uint16_t)e.type);
  w.write_u32(e.count);
  w.write<const uint8_t[4]>(e.data);
  return pos;
}

ParseResult<DateTime> parse_date_time(std::string_view str)
{
  ASSERT_OR_PARSE_ERROR(str.length() >= 18, CORRUPT_DATA, "DateTime value not long enough", str.data());
  int Y, M, D, h, m, s;
  DEBUG_PRINT("Date string: %.*s\n", int(str.length()), str.data());
  std::sscanf(str.data(), "%04d:%02d:%02d %02d:%02d:%02d", &Y, &M, &D, &h, &m, &s);
  DateTime dt;
  dt.year = Y, dt.month = M, dt.day = D;
  dt.hour = h, dt.minute = m, dt.second = s;
  return dt;
}

void debug_print_ifd_entry(Reader &r, const ifd_entry &e, const char *tag_name)
{
  if (enable_debug_print) {
    constexpr size_t cap = 1024;
    char buf[cap];
    int len = 0;
#define PRINT(args...) if (cap > len) len += std::snprintf(buf + len, cap - len, ##args)
    PRINT(
      "IFD entry {0x%04x %-20s, %x:%-10s, %6d, %02x%02x%02x%02x} ",
      e.tag, tag_name, (int)e.type, to_str(e.type), e.count,
      e.data[0], e.data[1], e.data[2], e.data[3]
    );
    if (e.size() > 4) {
      // offset!
      PRINT("@0x%x -> ", e.offset(r));
    }
    if (e.count < 60) {
      if (e.type == DType::ASCII) {
        if (e.count <= 4) {
          PRINT("\"%.*s\"", e.count, (const char *)e.data);
        } else {
          auto sv = r.data_view(e.offset(r), e.count);
          if (sv) {
            PRINT("\"%.*s\"", e.count, sv.value().data());
          } else {
            PRINT("[out of bounds]");
          }
        }
      } else {
        for (int i = 0; i < e.count; ++i) {
#define PRINT_IF_TYPE(_etype, _ctype, _fmt, _args...)               \
  if (e.type == _etype) {                                           \
    ParseResult<_ctype> _r = fetch_entry_value<_ctype>(e, i, r);    \
    if (_r) {                                                       \
      _ctype v = _r.value();                                        \
      PRINT(_fmt, ##_args);                                         \
    } else {                                                        \
      ParseError err = _r.error();                                  \
      PRINT("[%s:%s:%s]", to_str(err.code), err.message, err.what); \
    }                                                               \
    continue;                                                       \
  }
          PRINT_IF_TYPE(DType::SHORT, uint16_t, "%u ", (unsigned)v);
          PRINT_IF_TYPE(DType::SSHORT, int16_t, "%d ", (int)v)
          PRINT_IF_TYPE(DType::BYTE, uint8_t, "0x%02x ", (int)v);
          PRINT_IF_TYPE(DType::SBYTE, int8_t, "0x%02x ", (int)v)
          PRINT_IF_TYPE(DType::LONG, uint32_t, "%u ", (unsigned)v);
          PRINT_IF_TYPE(DType::SLONG, int32_t, "%d ", (int)v);
          PRINT_IF_TYPE(DType::FLOAT, float, "%f ", v);
          PRINT_IF_TYPE(DType::DOUBLE, double, "%f ", v);
          PRINT_IF_TYPE(DType::RATIONAL, rational64u, "%u/%u ", v.num, v.denom);
          PRINT_IF_TYPE(DType::SRATIONAL, rational64s, "%d/%d ", v.num, v.denom);

          PRINT("[CORRUPT_DATA:Unknown DType]");
          break;
        }
      }
    }
#undef PRINT_IF_TYPE
#undef PRINT
    debug_print_fn(buf);
  }
}  // namespace tiff

void debug_print_ifd_entry(Reader &r, const ifd_entry &e, const char *(*tag_to_str)(uint16_t tag))
{
  debug_print_ifd_entry(r, e, tag_to_str(e.tag));
}

const char *to_str(uint16_t tag, uint16_t ifd_bit)
{
#define TAG_TO_STR(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count) \
  if (tag == _tag##u && (ifd_bit & _ifd_bitmask)) {                                   \
    return #_name;                                                                    \
  }
  NEXIF_ALL_IFD0_TAGS(TAG_TO_STR);
  NEXIF_ALL_EXIF_TAGS(TAG_TO_STR);
#undef TAG_TO_STR
  return nullptr;
}

std::optional<ParseError> parse_subsectime_to_millis(Reader &r, const ifd_entry &entry, uint16_t *millis)
{
  int32_t s = entry.size();
  char buf[16] = {0};
  if (s <= 4) {
    std::memcpy(buf, &entry.data, s);
  } else {
    std::memcpy(buf, r.data + entry.offset(r), std::min(int32_t(sizeof(buf)), s));
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
    s = (s + (div >> 1)) / div;  // With proper rounding.
  }
  *millis = val;
  return std::nullopt;
}

#define PARSE_TAG(_struct, _name, _entry, _ifd_bits)             \
  {                                                              \
    using tag_info = TagInfo<(uint16_t)TagId::_name, _ifd_bits>; \
    assert(&(_struct) != nullptr);                               \
    auto tag_result = parse_tag<tag_info>(                       \
      r, _struct._name, _entry                                   \
    );                                                           \
    if (!tag_result)                                             \
      return tag_result.error();                                 \
    else if (tag_result.value())                                 \
      continue;                                                  \
  }

#define PARSE_TAG_CUSTOM(_name, _ifd_bits, _parse_lambda)                  \
  {                                                                        \
    using tag_info = TagInfo<(uint16_t)TagId::_name, _ifd_bits>;           \
    if (tag_info::TagId == entry.tag) {                                    \
      if (auto err = [&]() -> std::optional<ParseError> _parse_lambda()) { \
        return err;                                                        \
      }                                                                    \
      continue;                                                            \
    }                                                                      \
  }

ParseResult<bool> find_subifd(Reader &r, const ifd_entry &entry, const char *tag_str)
{
  if (entry.tag == uint16_t(TagId::exif_offset)) {
    ASSERT_OR_PARSE_ERROR(entry.type == DType::LONG, CORRUPT_DATA, "IFD EXIF type wrong", tag_str);
    ASSERT_OR_PARSE_ERROR(entry.count == 1, CORRUPT_DATA, "Only one IDF EXIF offset expected", tag_str);
    uint32_t offset = entry.offset(r);
    DEBUG_PRINT("Found EXIF SubIFD offset: %d", offset);
    r.subifd_refs.push_back({offset, 0, Reader::SubIFDRef::EXIF});
    return true;
  }
  if (entry.tag == uint16_t(TagId::sub_ifd_offset)) {
    ASSERT_OR_PARSE_ERROR(entry.type == DType::LONG, CORRUPT_DATA, "SubIFD datatype wrong", tag_str);
    for (int i = 0; i < entry.count; ++i) {
      DECL_OR_RETURN(uint32_t, offset, fetch_entry_value<uint32_t>(entry, i, r));
      DEBUG_PRINT("Found SubIFD: %d", offset);
      r.subifd_refs.push_back({offset, 0, Reader::SubIFDRef::OTHER});
    }
    return true;
  }
  if (entry.tag == uint16_t(TagId::makernote) || entry.tag == uint16_t(TagId::makernote_alt)) {
    ASSERT_OR_PARSE_ERROR(entry.type == DType::UNDEFINED, CORRUPT_DATA, "MakerNote datatype wrong", tag_str);
    uint32_t offset = entry.offset(r);
    DEBUG_PRINT("Found MakerNote: offset=%d size=%d", offset, entry.count);
    r.subifd_refs.push_back({offset, entry.count, Reader::SubIFDRef::MAKERNOTE});
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

std::optional<ParseError> parse_exif_ifd(Reader &r, ExifData &data, uint32_t exif_offset, uint32_t *next_offset)
{
  RETURN_IF_OPT_ERROR(r.seek(exif_offset));

  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("Num EXIF IFD entries: %d", num_entries);
  assert(num_entries < 1000);
  Indenter indenter;
  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);
    const char *tag_str = to_str(entry.tag, IFD_EXIF);
    debug_print_ifd_entry(r, entry, tag_str);
    Indenter indenter;

    FIND_SUBIFDS();

    // clang-format off
#define PARSE_EXIF_TAG(_name) PARSE_TAG(data.exif, _name, entry, IFD_EXIF)
    PARSE_EXIF_TAG(exposure_time);
    PARSE_EXIF_TAG(f_number);
    PARSE_EXIF_TAG(iso);
    PARSE_EXIF_TAG(exposure_program);
    PARSE_TAG(data.exif, focal_length, entry, IFD_ALL); // Can appear in both?
    PARSE_EXIF_TAG(exif_version);
    PARSE_EXIF_TAG(date_time_original);
    PARSE_EXIF_TAG(date_time_digitized);
    PARSE_TAG_CUSTOM(subsectime, IFD_EXIF, {
      return parse_subsectime_to_millis(r, entry, &data.date_time.value.millis);
    });
    PARSE_TAG_CUSTOM(subsectime_original, IFD_EXIF, {
      return parse_subsectime_to_millis(r, entry, &data.exif.date_time_original.value.millis);
    });
    PARSE_TAG_CUSTOM(subsectime_digitized, IFD_EXIF, {
      return parse_subsectime_to_millis(r, entry, &data.exif.date_time_digitized.value.millis);
    });

    PARSE_EXIF_TAG(camera_owner_name         );
    PARSE_EXIF_TAG(body_serial_number        );
    PARSE_EXIF_TAG(lens_specification        );
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
  RETURN_IF_OPT_ERROR(r.seek(ifd_offset));

  uint16_t num_entries = r.read_u16();
  DEBUG_PRINT("IFD at offset: %d -> Num entries: %d", ifd_offset, num_entries);

  Indenter indenter;

  // Dummy tags for reading purposes which will be post-processed.
  Tag<uint32_t> tag_subfile_type;
  Tag<uint16_t> tag_oldsubfile_type;

  for (int i = 0; i < num_entries; ++i) {
    // Read IFD entry.
    ifd_entry entry = read_ifd_entry(r);
    const char *tag_str = to_str(entry.tag, IFD_01);
    debug_print_ifd_entry(r, entry, tag_str);
    Indenter indenter;

    FIND_SUBIFDS();

    if (entry.tag == (uint16_t)TagId::subfile_type) {
      // Create new SubIFD!
    }
#define PARSE_ROOT_TAG(_name) PARSE_TAG(data, _name, entry, IFD_01)
    if (ifd_type & IFD_01) {
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
      PARSE_TAG(data.exif, focal_length, entry, IFD_ALL);
    }

#undef PARSE_ROOT_TAG

    if (current_image != nullptr) {
#define PARSE_IFD0_TAG(_name) PARSE_TAG((*current_image), _name, entry, IFD_01)
      if (ifd_type & IFD_01) {
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

    parse_tag<tiff::tag_subfile_type>(r, tag_subfile_type, entry);
    parse_tag<tiff::tag_old_subfile_type>(r, tag_oldsubfile_type, entry);
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

std::optional<ParseError> parse_makernote(Reader &r, ExifData &data, uint32_t offset, uint32_t length)
{
  using namespace std::string_view_literals;
  std::string_view magic_nikon = "Nikon\0"sv;
  if (std::memcmp(r.data + offset, magic_nikon.data(), magic_nikon.length()) == 0) {
    Reader mnr(r.warnings);
    mnr.data = r.data + offset + 10;
    mnr.file_length = length - 10;
    return makernote::nikon::parse_makernote(mnr, data);
  }

  return PARSE_ERROR(UNKNOWN_FILE_TYPE, "MakerNote of unknown type", nullptr);
}

std::optional<ParseError> read_tiff(Reader &r, ExifData &data)
{
  if (r.data[0] == 'I' && r.data[1] == 'I') {
    r.byte_order = std::endian::little;
  } else if (r.data[0] == 'M' && r.data[1] == 'M') {
    r.byte_order = std::endian::big;
  } else {
    return PARSE_ERROR(CORRUPT_DATA, "Not a TIFF file", "II or MM header not found");
  }
  RETURN_IF_OPT_ERROR(r.seek(4));
  const uint32_t root_ifd_offset = r.read_u32();
  DEBUG_PRINT("root IFD offset: %d", root_ifd_offset);

  uint32_t ifd_offset = root_ifd_offset;
  uint16_t ifd_type = IFD0;
  for (int ifd_idx = 0; ifd_idx < 5; ++ifd_idx) {
    DEBUG_PRINT("move to IFD at offset: %d\n", ifd_offset);
    uint32_t next_ifd_offset;

    ImageData *current_image = &data.images[data.num_images++];
    if (auto error = parse_tiff_ifd(r, data, ifd_offset, current_image, ifd_type, &next_ifd_offset)) {
      return error;
    }

    RELAXED_ASSERT_PARSE_ERROR_OR_WARNING(ifd_offset % 2 == 0, r, CORRUPT_DATA, "IFD must align to word boundary", "root IFD");
    if (next_ifd_offset < r.file_length && next_ifd_offset != 0) {
      ifd_offset = next_ifd_offset;
      ifd_type = IFD1;  // We now go to thumbnails
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
            if (r.strict_mode) {
              return error;
            } else {
              r.warnings.emplace_back(error->message, error->what);
              break;
            }
          }
        } while (next_offset != 0);
        ref.parsed = true;
        break;
      case Reader::SubIFDRef::OTHER:
        do {
          ImageData *current_image = &data.images[data.num_images++];
          if (auto error = parse_tiff_ifd(r, data, next_offset, current_image, ifd_type, &next_offset)) {
            if (r.strict_mode) {
              return error;
            } else {
              r.warnings.emplace_back(error->message, error->what);
              break;
            }
          }
        } while (next_offset != 0);
        ref.parsed = true;
        break;
      case Reader::SubIFDRef::MAKERNOTE: {
        if (auto error = parse_makernote(r, data, next_offset, ref.length)) {
          if (r.strict_mode) {
            return error;
          } else {
            r.warnings.emplace_back(error->message, error->what);
            break;
          }
        }
        break;
      }
      case Reader::SubIFDRef::GPS:
      case Reader::SubIFDRef::INTEROP:
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
  // Variable count is compatible with count 1
  static_assert(TagInfo::count_spec::cpp_count == 1 || TagInfo::count_spec::cpp_count == 0);
  ifd_entry e;
  e.type = TagInfo::tiff_type;
  e.tag = TagInfo::TagId;
  e.count = 1;
  int required_size = size_of_dtype(e.type) * e.count;
  if (required_size <= sizeof(uint32_t)) {
    std::memcpy(&e.data, &value, std::min(sizeof(required_size), sizeof(value)));
  } else {
    // The data should be stored elsewhere, in the data section,
    // after the IFD. We don't know the offset to the data section yet
    // as that will be determined by how many tags we write in this
    // IFD. So instead, we leave a placeholder, and we will count
    // how many offsets need to be rewritten. Detecting which ones
    // is a matter of doing the "required_size" calculation above.
    uint32_t offset = w.data_writer.write(value);
    std::memcpy(&e.data, &offset, 4);
    w.num_offsets_to_adjust++;
  }
  w.num_tags_written++;
  return write_ifd_entry(w.tags_writer, e);
}

template <typename TagInfo>
size_t write_tiff_tag_string(IFD_Writer &w, const char *str, uint32_t length)
{
  static_assert(TagInfo::tiff_type == DType::ASCII);
  // Variable count is compatible with count 1
  static_assert(TagInfo::count_spec::cpp_count == 1);
  ifd_entry e;
  e.type = TagInfo::tiff_type;
  e.tag = TagInfo::TagId;
  e.count = length;
  if (length <= sizeof(uint32_t)) {
    std::memcpy(&e.data, str, length);
    for (int i = length; i < 4; ++i) {
      e.data[i] = 0;
    }
  } else {
    // The data should be stored elsewhere, in the data section,
    // after the IFD. We don't know the offset to the data section yet
    // as that will be determined by how many tags we write in this
    // IFD. So instead, we leave a placeholder, and we will count
    // how many offsets need to be rewritten. Detecting which ones
    // is a matter of doing the "required_size" calculation above.
    uint32_t offset = w.data_writer.write_string(str, length);
    std::memcpy(&e.data[0], &offset, sizeof(uint32_t));
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
      return;
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
      char buf_millis[4];
      std::snprintf(buf_millis, sizeof(buf_millis), "%03d", dt.millis);
      if (std::is_same_v<TagInfo, tag_date_time_original>) {
        write_tiff_tag_string<tag_subsectime_original>(w, buf_millis, 4);
      } else if (std::is_same_v<TagInfo, tag_date_time>) {
        write_tiff_tag_string<tag_subsectime>(w, buf_millis, 4);
      } else if (std::is_same_v<TagInfo, tag_date_time_digitized>) {
        write_tiff_tag_string<tag_subsectime_digitized>(w, buf_millis, 4);
      }
      return;
    } else {
      if constexpr (TagInfo::count_spec::cpp_count == 1) {
        if constexpr (std::is_trivial_v<cppt>) {
          write_tiff_tag_scalar<TagInfo>(w, tag.value);
          return;
        }
      }
    }

    DEBUG_PRINT("Cannot write tag %04x with count %d", TagInfo::TagId, TagInfo::count_spec::cpp_count);
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

    uint16_t tag = w.tags_writer.read_u16(ifd_entry_offset);
    DType type = (DType)w.tags_writer.read_u16(ifd_entry_offset + 2);
    uint32_t count = w.tags_writer.read_u32(ifd_entry_offset + 4);
    int required_bytes = size_of_dtype(type) * count;
    assert(required_bytes > 0);
    if (required_bytes > 4) {
      uint32_t value_offset = ifd_entry_offset + (2 + 2 + 4);
      uint32_t offset = w.tags_writer.read_u32(value_offset);
      DEBUG_PRINT("rewrite 0x%04x offset value: %u to %u", tag, offset, offset + w.data_offset);
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
  assert(ifd_w.num_tags_written * sizeof(ifd_entry) == ifd_w.tags.size());
  DEBUG_PRINT(" ifd_offset: %u", ifd_w.ifd_offset);
  DEBUG_PRINT("data_offset: %u", ifd_w.data_offset);
  DEBUG_PRINT("tags_size  : %zu", ifd_w.tags.size());
  DEBUG_PRINT("data_size  : %zu", ifd_w.data.size());
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
  if (std::endian::native == std::endian::little) {
    // Intel byte order!
    w.write_u8('I');
    w.write_u8('I');
  } else {
    // Minolta byte order!
    w.write_u8('M');
    w.write_u8('M');
  }
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
    write_tiff_tag<tag_lens_specification        >(exif_ifd, data.exif.lens_specification         );
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
