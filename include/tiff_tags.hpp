#pragma once

#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "neonexif.hpp"

namespace nexif {
namespace tiff {

struct IFD_BitMask {
  constexpr static uint16_t IFD0 = 0x1;
  constexpr static uint16_t IFD1 = 0x2;
  constexpr static uint16_t IFD2 = 0x4;
  constexpr static uint16_t IFD3 = 0x8;
  constexpr static uint16_t IFD4 = 0x10;
  constexpr static uint16_t IFD_EXIF = 0x100;
  constexpr static uint16_t IFD_GPS = 0x200;
  constexpr static uint16_t IFD_MAKER_NOTES = 0x400;

  constexpr static uint16_t IFD_ALL_NORMAL = 0x1f;
};

enum class DType : uint16_t {
  BYTE = 1,        // unsigned
  ASCII = 2,       // a single byte
  SHORT = 3,       // unsigned 16-bit
  LONG = 4,        // unsigned 32-bit
  RATIONAL = 5,    // two LONGs (num/denom)
  UNDEFINED = 7,   // one byte
  SLONG = 9,       // 32-bit signed
  SRATIONAL = 10,  // two SLONGs (num/denom).
};

inline const char *to_str(DType d) {
  switch (d) {
    case DType::BYTE: return "BYTE";
    case DType::ASCII: return "ASCII";
    case DType::SHORT: return "SHORT";
    case DType::LONG: return "LONG";
    case DType::RATIONAL: return "RATIONAL";
    case DType::UNDEFINED: return "UNDEFINED";
    case DType::SLONG: return "SLONG";
    case DType::SRATIONAL: return "SRATIONAL";
  }
  std::abort();
}

inline int32_t size_of_dtype(DType dt)
{
  switch (dt) {
    case DType::BYTE:
    case DType::UNDEFINED:
    case DType::ASCII: return 1;
    case DType::SHORT: return 2;
    case DType::LONG:
    case DType::SLONG: return 4;
    case DType::RATIONAL:
    case DType::SRATIONAL: return 8;
  };
  std::abort();
}

template <typename T>
inline bool matches_dtype(DType dtype)
{
  if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t>) {
    return dtype == DType::BYTE || dtype == DType::UNDEFINED;
  } else if constexpr (std::is_same_v<T, CharData>) {
    return dtype == DType::ASCII;
  } else if constexpr (std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t>) {
    return dtype == DType::SHORT;
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return dtype == DType::LONG;
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return dtype == DType::SLONG;
  } else if constexpr (std::is_same_v<T, rational64u>) {
    return dtype == DType::RATIONAL;
  } else if constexpr (std::is_same_v<T, rational64s>) {
    return dtype == DType::SRATIONAL;
  }
  return false;
}
template <typename T>
bool fits_dtype(DType dtype)
{
  if (dtype == DType::ASCII)
    return false;

  if constexpr (std::is_same_v<T, uint8_t> || std::is_same_v<T, int8_t>) {
    return dtype == DType::BYTE || dtype == DType::UNDEFINED;
  } else if constexpr (std::is_same_v<T, uint16_t> || std::is_same_v<T, int16_t>) {
    return dtype <= DType::SHORT;
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return dtype <= DType::LONG;
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return dtype <= DType::SLONG;
  }
  return false;
}

// clang-format off
#define ALL_IFD0_TAGS(x, scalar, fixed_count, variable_count)               \
x(0x0001, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , interop_index              , variable_count )    \
x(0x0002, IFD_BitMask::IFD_ALL_NORMAL, UNDEFINED, CharData   , interop_version            , scalar         )    \
x(0x000b, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , processing_software        , variable_count )    \
x(0x00fe, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , subfile_type               , scalar         )    \
x(0x00ff, IFD_BitMask::IFD_ALL_NORMAL, SHORT    , uint16_t   , old_subfile_type           , scalar         )    \
x(0x0100, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , image_width                , scalar         )    \
x(0x0101, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , image_height               , scalar         )    \
x(0x0102, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , bits_per_sample            , variable_count )    \
x(0x0103, IFD_BitMask::IFD_ALL_NORMAL, SHORT    , uint16_t   , compression                , scalar         )    \
x(0x0106, IFD_BitMask::IFD_ALL_NORMAL, SHORT    , uint16_t   , photometric_interpretation , scalar         )    \
x(0x010f, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , make                       , variable_count )    \
x(0x0110, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , model                      , variable_count )    \
x(0x0112, IFD_BitMask::IFD_ALL_NORMAL, SHORT    , Orientation, orientation                , scalar         )    \
x(0x0115, IFD_BitMask::IFD_ALL_NORMAL, SHORT    , uint16_t   , samples_per_pixel          , scalar         )    \
x(0x011a, IFD_BitMask::IFD_ALL_NORMAL, RATIONAL , rational64u, x_resolution               , scalar         )    \
x(0x011b, IFD_BitMask::IFD_ALL_NORMAL, RATIONAL , rational64u, y_resolution               , scalar         )    \
x(0x0128, IFD_BitMask::IFD_ALL_NORMAL, SHORT    , uint16_t   , resolution_unit            , scalar         )    \
x(0x0131, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , software                   , variable_count )    \
x(0x013b, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , artist                     , variable_count )    \
x(0x0201, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , data_offset                , scalar         )    \
x(0x0202, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , data_length                , scalar         )    \
x(0x8298, IFD_BitMask::IFD_ALL_NORMAL, ASCII    , CharData   , copyright                  , variable_count )    \
x(0x8769, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , exif_offset                , variable_count )    \
x(0x014a, IFD_BitMask::IFD_ALL_NORMAL, LONG     , uint32_t   , sub_ifd_offset             , variable_count )  // clang-format on

// clang-format off
#define ALL_EXIF_TAGS(x, scalar, fixed_count, variable_count)              \
x(0x829a, IFD_BitMask::IFD_EXIF, RATIONAL , rational64u, exposure_time       , scalar         )    \
x(0x829d, IFD_BitMask::IFD_EXIF, RATIONAL , rational64u, f_number            , scalar         )    \
x(0x8827, IFD_BitMask::IFD_EXIF, SHORT    , uint16_t   , iso                 , variable_count )    \
x(0x8822, IFD_BitMask::IFD_EXIF, SHORT    , uint16_t   , exposure_program    , scalar         )    \
x(0x920a, IFD_BitMask::IFD_EXIF, RATIONAL , rational64u, focal_length        , scalar         )    \
x(0x9202, IFD_BitMask::IFD_EXIF, RATIONAL , rational64u, aperture_value      , scalar         )    \
x(0x9000, IFD_BitMask::IFD_EXIF, UNDEFINED, CharData   , exif_version        , variable_count )  // clang-format on

#define TAG_ENUM(tag, ifd_bitmask, expected_tiff_type, cpp_type, name, count) name = tag##u,

enum class TagId : uint16_t {
  // clang-format off
  ALL_IFD0_TAGS(TAG_ENUM, , , )
  ALL_EXIF_TAGS(TAG_ENUM, , , )
  // clang-format on
};

#undef TAG_ENUM

template <uint16_t _TagId, uint16_t _IFD_BitMask>
struct TagInfo {
  constexpr static bool defined = false;
  constexpr static uint16_t TagId = _TagId;
  constexpr static uint16_t IFD_BitMask = _IFD_BitMask;
  constexpr static DType tiff_type = (DType)-1;
  using cpp_type = void;
  constexpr static int count = 0;
  constexpr static const char *name = nullptr;
};

#define TEMPLATE_TAG_INFO(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count) \
  template <>                                                                                \
  struct TagInfo<_tag##u, _ifd_bitmask> {                                                    \
    constexpr static bool defined = true;                                                    \
    constexpr static uint16_t TagId = _tag##u;                                               \
    constexpr static uint16_t IFD_BitMask = _ifd_bitmask;                                    \
    constexpr static DType tiff_type = DType::_expected_tiff_type;                           \
    using cpp_type = _cpp_type;                                                              \
    constexpr static int count = _count;                                                     \
    constexpr static const char *name = #_name;                                              \
  };

#define IDENT(x) x
ALL_IFD0_TAGS(TEMPLATE_TAG_INFO, 1, IDENT, 0);
ALL_EXIF_TAGS(TEMPLATE_TAG_INFO, 1, IDENT, 0);
#undef TEMPLATE_TAG_INFO

const char *to_str(uint16_t tag, uint16_t ifd_bit);

}  // namespace tiff
}  // namespace nexif
