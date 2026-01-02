#pragma once

#include "neonexif/neonexif.hpp"
#include "neonexif/reader.hpp"

#include <cstring>
#include <cassert>
#include <optional>

namespace nexif {
namespace tiff {

constexpr static uint16_t IFD0 = 0x1;
constexpr static uint16_t IFD1 = 0x2;
constexpr static uint16_t IFD_EXIF = 0x4;
constexpr static uint16_t IFD_INTEROP = 0x8;
constexpr static uint16_t IFD_GPS = 0x10;
constexpr static uint16_t IFD_01 = 0x3;
constexpr static uint16_t IFD_ALL = 0xffff;

enum class DType : uint16_t {
  BYTE = 1,        // unsigned
  ASCII = 2,       // a single byte
  SHORT = 3,       // unsigned 16-bit
  LONG = 4,        // unsigned 32-bit
  RATIONAL = 5,    // two LONGs (num/denom)
  SBYTE = 6,       // signed byte 8-bit
  UNDEFINED = 7,   // one byte
  SSHORT = 8,      // signed 16-bit
  SLONG = 9,       // 32-bit signed
  SRATIONAL = 10,  // two SLONGs (num/denom).
  FLOAT = 11,      // 32-bit IEEE float
  DOUBLE = 12,     // 64-bit IEEE float
};

inline const char *to_str(DType d)
{
  switch (d) {
    case DType::BYTE: return "BYTE";
    case DType::ASCII: return "ASCII";
    case DType::SHORT: return "SHORT";
    case DType::LONG: return "LONG";
    case DType::RATIONAL: return "RATIONAL";
    case DType::SBYTE: return "SBYTE";
    case DType::UNDEFINED: return "UNDEFINED";
    case DType::SSHORT: return "SSHORT";
    case DType::SLONG: return "SLONG";
    case DType::SRATIONAL: return "SRATIONAL";
    case DType::FLOAT: return "FLOAT";
    case DType::DOUBLE: return "DOUBLE";
  }
  return "Unknown";
}

inline int32_t size_of_dtype(DType dt)
{
  switch (dt) {
    case DType::BYTE:
    case DType::SBYTE:
    case DType::UNDEFINED:
    case DType::ASCII: return 1;
    case DType::SSHORT:
    case DType::SHORT: return 2;
    case DType::FLOAT:
    case DType::LONG:
    case DType::SLONG: return 4;
    case DType::DOUBLE:
    case DType::RATIONAL:
    case DType::SRATIONAL: return 8;
  };
  return 0;
}

template <typename T>
inline bool matches_dtype(DType dtype)
{
  if constexpr (std::is_same_v<T, uint8_t>) {
    return dtype == DType::UNDEFINED || dtype == DType::BYTE;
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    return dtype == DType::UNDEFINED || dtype == DType::SBYTE;
  } else if constexpr (std::is_same_v<T, CharData>) {
    return dtype == DType::ASCII || dtype == DType::UNDEFINED;
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    return dtype == DType::SHORT;
  } else if constexpr (std::is_same_v<T, int16_t>) {
    return dtype == DType::SSHORT;
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return dtype == DType::LONG;
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return dtype == DType::SLONG;
  } else if constexpr (std::is_same_v<T, rational64u>) {
    return dtype == DType::RATIONAL;
  } else if constexpr (std::is_same_v<T, rational64s>) {
    return dtype == DType::SRATIONAL;
  } else if constexpr (std::is_same_v<T, float>) {
    return dtype == DType::FLOAT;
  } else if constexpr (std::is_same_v<T, double>) {
    return dtype == DType::DOUBLE;
  } else if constexpr (std::is_same_v<T, DateTime>) {
    return dtype == DType::ASCII;
  }
  return false;
}
template <typename T>
bool fits_dtype(DType dtype)
{
  if (dtype == DType::ASCII)
    return false;

  // clang-format off
  if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t>) {
    return dtype == DType::BYTE
        || dtype == DType::UNDEFINED
        || dtype == DType::SBYTE
        ;
  } else if constexpr (std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t>) {
    return dtype == DType::BYTE
        || dtype == DType::UNDEFINED
        || dtype == DType::SBYTE
        || dtype == DType::SHORT
        || dtype == DType::SSHORT
        ;
  } else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, int32_t>) {
    return dtype == DType::BYTE
        || dtype == DType::UNDEFINED
        || dtype == DType::SBYTE
        || dtype == DType::SHORT
        || dtype == DType::SSHORT
        || dtype == DType::SLONG
        || dtype == DType::LONG
        ;
  } else if constexpr (std::is_same_v<T, float>) {
    return dtype == DType::FLOAT;
  } else if constexpr (std::is_same_v<T, double>) {
    return dtype == DType::FLOAT
        || dtype == DType::DOUBLE
        ;
  }
  // clang-format on
  return false;
}

template <int ExifC, int CppCount, bool ExifVar, bool CppVar>
struct count_spec {
  static constexpr int exif_count = ExifC;
  static constexpr int cpp_count = CppCount;
  static constexpr bool exif_var = ExifVar;
  static constexpr bool cpp_var = CppVar;
};
using count_scalar = count_spec<1, 1, false, false>;
template <int C>
using count_limvar = count_spec<C, C, true, true>;
template <int C>
using count_fixed = count_spec<C, C, false, false>;
using count_var = count_spec<0, 0, true, true>;
using count_string = count_spec<0, 1, true, false>;

struct ifd_entry {
  constexpr static int BINARY_SIZE = 2 + 2 + 4 + 4;
  uint16_t tag;
  DType type;
  uint32_t count;
  uint8_t data[4];

  uint32_t offset(Reader &r) const
  {
    uint32_t o;
    std::memcpy(&o, data, 4);
    if (r.byte_order != std::endian::native) {
      o = nexif::byteswap(o);
    }
    return o;
  }

  int32_t size() const
  {
    return count * size_of_dtype(type);
  }
};
static_assert(sizeof(ifd_entry) == ifd_entry::BINARY_SIZE);

ifd_entry read_ifd_entry(Reader &r);

void debug_print_ifd_entry(Reader &r, const ifd_entry &e, const char *(*to_str)(uint16_t tag));
void debug_print_ifd_entry(Reader &r, const ifd_entry &e, const char *tag_name);

namespace {
template <typename T, bool is_enum = std::is_enum_v<T>>
struct base_type {
  using type = T;
};

template <typename T>
struct base_type<T, true> {
  using type = std::underlying_type_t<T>;
};
}  // namespace

template <typename T>
ParseResult<T> fetch_entry_value(const ifd_entry &entry, int idx, Reader &r)
{
  ASSERT_OR_PARSE_ERROR(idx < entry.count, CORRUPT_DATA, "entry index out of bounds", nullptr);
  int32_t elem_size = size_of_dtype(entry.type);
  assert(sizeof(T) >= elem_size);
  T t{0};
  if (entry.size() <= 4) {
    // Data is inline
    std::memcpy(&t, ((char *)&entry.data) + idx * elem_size, elem_size);
  } else {
    // Data is elsewhere
    uint32_t offset = entry.offset(r) + elem_size * idx;
    if (offset + elem_size <= r.file_length) {
      std::memcpy(&t, r.data + offset, elem_size);
    } else {
      return PARSE_ERROR(CORRUPT_DATA, "data offset out of bounds", nullptr);
    }
  }
  if (r.byte_order != std::endian::native) {
    if constexpr (std::is_same_v<T, rational64u> || std::is_same_v<T, rational64s>) {
      t.num = nexif::byteswap(t.num);
      t.denom = nexif::byteswap(t.denom);
    } else {
      t = nexif::byteswap(t);
    }
  }
  return t;
}

template <typename TagInfo>
inline ParseResult<bool> parse_tag(Reader &r, Tag<typename TagInfo::cpp_type> &tag, const ifd_entry &entry)
{
  using BType = base_type<typename TagInfo::scalar_cpp_type>::type;
  const char *tag_str = TagInfo::name;
  constexpr uint16_t tag_idval = TagInfo::TagId;
  if (entry.tag == tag_idval) {
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
          tag.value = r.exif_data->store_string_data((char *)&entry.data, entry.count);
          DEBUG_PRINT("store inline string data of length %d: %.*s", entry.count, entry.count, tag.value.data());
        } else {
          DECL_OR_RETURN(std::string_view, sv, r.data_view(entry.offset(r), entry.count));
          tag.value = r.exif_data->store_string_data(sv.data(), sv.size());
          DEBUG_PRINT("store external string data of length %zu: %.*s", sv.length(), (int)sv.length(), sv.data());
        }
        DEBUG_PRINT("CharData %p: %+d  (len %u)", (void *)&tag.value, tag.value.ptr_offset, tag.value.length);
        tag.parsed_from = tag_idval;
        tag.is_set = true;
        return true;
      } else if constexpr (std::is_same_v<BType, rational64s> || std::is_same_v<BType, rational64u>) {
        int mark = r.ptr;
        RETURN_IF_OPT_ERROR(r.seek(entry.offset(r)));
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
        RETURN_IF_OPT_ERROR(r.seek(mark));
        return true;
      } else if constexpr (std::is_same_v<BType, DateTime>) {
        DECL_OR_RETURN(std::string_view, str, r.data_view(entry.offset(r), entry.count));
        DECL_OR_RETURN(DateTime, dt, parse_date_time(str));
        tag.value.year = dt.year, tag.value.month = dt.month, tag.value.day = dt.day;
        tag.value.hour = dt.hour, tag.value.minute = dt.minute, tag.value.second = dt.second;
        tag.is_set = true;
        tag.parsed_from = tag_idval;
        return true;
      } else if (entry.size() <= 4) {
        DECL_OR_RETURN(BType, value, fetch_entry_value<BType>(entry, 0, r));
        tag.value = (typename TagInfo::cpp_type)value;
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

std::optional<ParseError> read_tiff(
  Reader &r,
  ExifData &data
);

size_t write_tiff(Writer &w, const ExifData &data);

}  // namespace tiff
}  // namespace nexif
