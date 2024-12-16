#pragma once

#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "neonexif.hpp"

namespace nexif {
namespace tiff {

enum class IFD {
  IFD0 = 0,
  IFD1 = 1,
  IFD2 = 2,
  IFD3 = 3,
  IFD4 = 4,
  IFD_EXIF = 100,
  IFD_GPS = 101,
  IFD_MAKER_NOTES = 102,
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
x(0x0001, ASCII    , CharData, interop_index            , variable_count )    \
x(0x0002, UNDEFINED, CharData, interop_version          , scalar         )    \
x(0x000b, ASCII    , CharData, processing_software      , variable_count )    \
x(0x00fe, LONG     , uint32_t, subfile_type             , scalar         )    \
x(0x00ff, SHORT    , uint16_t, old_subfile_type         , scalar         )    \
x(0x0100, LONG     , uint32_t, image_width              , scalar         )    \
x(0x0101, LONG     , uint32_t, image_height             , scalar         )    \
x(0x0102, LONG     , uint32_t, bits_per_sample          , variable_count )    \
x(0x0103, SHORT    , uint16_t, compression              , scalar         )    \
x(0x010f, ASCII    , CharData, make                     , variable_count )    \
x(0x0110, ASCII    , CharData, model                    , variable_count )    \
x(0x0115, SHORT    , uint16_t, samplers_per_pixel       , scalar         )    \
x(0x013b, ASCII    , CharData, artist                   , variable_count )    \
x(0x8298, ASCII    , CharData, copyright                , variable_count )    \
x(0x8769, LONG     , uint32_t, exif_offset              , variable_count )    \
x(0x014a, LONG     , uint32_t, sub_ifd_offset           , variable_count )  // clang-format on

// clang-format off
#define ALL_EXIF_TAGS(x, scalar, fixed_count, variable_count)              \
x(0x829a, RATIONAL , rational64u, exposure_time       , scalar         )    \
x(0x829d, RATIONAL , rational64u, f_number            , scalar         )    \
x(0x8833, LONG     , uint32_t   , iso_speed           , scalar         )    \
x(0x8822, SHORT    , uint16_t   , exposure_program    , scalar         )    \
x(0x920a, RATIONAL , rational64u, focal_length        , scalar         )    \
x(0x9202, RATIONAL , rational64u, aperture_value      , scalar         )    \
x(0x9000, UNDEFINED, CharData   , exif_version        , variable_count )  // clang-format on

#define TAG_ENUM(tag, expected_tiff_type, cpp_type, name, count) name = tag##u,

enum class TagId : uint16_t {
  ALL_IFD0_TAGS(TAG_ENUM, , , )
    ALL_EXIF_TAGS(TAG_ENUM, , , )
};

#undef TAG_ENUM

template <uint16_t _TagId>
struct TagInfo {
  constexpr static bool defined = false;
  constexpr static uint16_t TagId = _TagId;
  constexpr static DType tiff_type = (DType)-1;
  using cpp_type = void;
  constexpr static int count = 0;
  constexpr static const char *name = nullptr;
};

#define TEMPLATE_TAG_INFO(_tag, _expected_tiff_type, _cpp_type, _name, _count) \
  template <>                                                                  \
  struct TagInfo<_tag##u> {                                                    \
    constexpr static bool defined = true;                                      \
    constexpr static uint16_t TagId = _tag##u;                                 \
    constexpr static DType tiff_type = DType::_expected_tiff_type;             \
    using cpp_type = _cpp_type;                                                \
    constexpr static int count = _count;                                       \
    constexpr static const char *name = #_name;                                \
  };

#define IDENT(x) x
ALL_IFD0_TAGS(TEMPLATE_TAG_INFO, 1, IDENT, 0);
#undef TEMPLATE_TAG_INFO

const char *to_str(uint16_t tag, IFD ifd);

}  // namespace tiff
}  // namespace nexif
