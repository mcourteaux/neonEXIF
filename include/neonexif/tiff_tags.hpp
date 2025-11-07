#pragma once

#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "neonexif.hpp"

namespace nexif {
namespace tiff {

struct IFD_BitMasks {
  constexpr static uint16_t IFD0 = 0x1;
  constexpr static uint16_t IFD1 = 0x2;
  constexpr static uint16_t IFD_EXIF = 0x100;
  constexpr static uint16_t IFD_GPS = 0x200;
  constexpr static uint16_t IFD_MAKERNOTE = 0x400;

  constexpr static uint16_t IFD_01 = 0x1f;

  constexpr static uint16_t IFD_ALL = 0xffff;
};

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

// clang-format off
#define ALL_IFD0_TAGS(x)                \
x(0x0001, IFD_BitMasks::IFD_01 , ASCII    , CharData   , interop_index              , count_string         )    \
x(0x0002, IFD_BitMasks::IFD_01 , UNDEFINED, CharData   , interop_version            , count_scalar         )    \
x(0x000b, IFD_BitMasks::IFD_01 , ASCII    , CharData   , processing_software        , count_string         )    \
x(0x00fe, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , subfile_type               , count_scalar         )    \
x(0x00ff, IFD_BitMasks::IFD_01 , SHORT    , uint16_t   , old_subfile_type           , count_scalar         )    \
x(0x0100, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , image_width                , count_scalar         )    \
x(0x0101, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , image_height               , count_scalar         )    \
x(0x0102, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , bits_per_sample            , count_limvar<8>      )    \
x(0x0103, IFD_BitMasks::IFD_01 , SHORT    , uint16_t   , compression                , count_scalar         )    \
x(0x0106, IFD_BitMasks::IFD_01 , SHORT    , uint16_t   , photometric_interpretation , count_scalar         )    \
x(0x010f, IFD_BitMasks::IFD_01 , ASCII    , CharData   , make                       , count_string         )    \
x(0x0110, IFD_BitMasks::IFD_01 , ASCII    , CharData   , model                      , count_string         )    \
x(0x0112, IFD_BitMasks::IFD_01 , SHORT    , Orientation, orientation                , count_scalar         )    \
x(0x0115, IFD_BitMasks::IFD_01 , SHORT    , uint16_t   , samples_per_pixel          , count_scalar         )    \
x(0x011a, IFD_BitMasks::IFD_01 , RATIONAL , rational64u, x_resolution               , count_scalar         )    \
x(0x011b, IFD_BitMasks::IFD_01 , RATIONAL , rational64u, y_resolution               , count_scalar         )    \
x(0x0128, IFD_BitMasks::IFD_01 , SHORT    , uint16_t   , resolution_unit            , count_scalar         )    \
x(0x0131, IFD_BitMasks::IFD_01 , ASCII    , CharData   , software                   , count_string         )    \
x(0x0132, IFD_BitMasks::IFD_01 , ASCII    , DateTime   , date_time                  , count_string         )    \
x(0x013b, IFD_BitMasks::IFD_01 , ASCII    , CharData   , artist                     , count_string         )    \
x(0x0201, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , data_offset                , count_scalar         )    \
x(0x0202, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , data_length                , count_scalar         )    \
x(0x8298, IFD_BitMasks::IFD_01 , ASCII    , CharData   , copyright                  , count_string         )    \
x(0x8769, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , exif_offset                , count_scalar         )    \
x(0x014a, IFD_BitMasks::IFD_01 , LONG     , uint32_t   , sub_ifd_offset             , count_var            )    \
x(0x927c, IFD_BitMasks::IFD_ALL, UNDEFINED, uint8_t    , makernote                  , count_var            )    \
x(0x002e, IFD_BitMasks::IFD_ALL, UNDEFINED, uint8_t    , makernote_alt              , count_var            )    \
x(0xc621, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, color_matrix_1             , count_limvar<12>     )    \
x(0xc622, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, color_matrix_2             , count_limvar<12>     )    \
x(0xc623, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, calibration_matrix_1       , count_limvar<12>     )    \
x(0xc624, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, calibration_matrix_2       , count_limvar<12>     )    \
x(0xc625, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, reduction_matrix_1         , count_limvar<12>     )    \
x(0xc626, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, reduction_matrix_2         , count_limvar<12>     )    \
x(0xc627, IFD_BitMasks::IFD_01 , RATIONAL , rational64u, analog_balance             , count_limvar<4>      )    \
x(0xc628, IFD_BitMasks::IFD_01 , RATIONAL , rational64u, as_shot_neutral            , count_limvar<4>      )    \
x(0xc629, IFD_BitMasks::IFD_01 , RATIONAL , rational64u, as_shot_white_xy           , count_fixed<2>       )    \
x(0xc65a, IFD_BitMasks::IFD_01 , SHORT    , Illuminant , calibration_illuminant_1   , count_scalar         )    \
x(0xc65b, IFD_BitMasks::IFD_01 , SHORT    , Illuminant , calibration_illuminant_2   , count_scalar         )    \
x(0x882a, IFD_BitMasks::IFD_01 , SSHORT   , Illuminant , timezone_offset            , count_scalar         )    \
x(0x9201, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, apex_aperture_value        , count_scalar         )    \
x(0x9202, IFD_BitMasks::IFD_01 , SRATIONAL, rational64s, apex_shutter_speed_value   , count_scalar         )    \

// clang-format on

// clang-format off
#define ALL_EXIF_TAGS(x)              \
x(0x829a, IFD_BitMasks::IFD_EXIF, RATIONAL , rational64u, exposure_time             , count_scalar         )    \
x(0x829d, IFD_BitMasks::IFD_EXIF, RATIONAL , rational64u, f_number                  , count_scalar         )    \
x(0x8827, IFD_BitMasks::IFD_EXIF, SHORT    , uint16_t   , iso                       , count_scalar         )    \
x(0x8822, IFD_BitMasks::IFD_EXIF, SHORT    , uint16_t   , exposure_program          , count_scalar         )    \
x(0x920a, IFD_BitMasks::IFD_ALL , RATIONAL , rational64u, focal_length              , count_scalar         )    \
x(0x9000, IFD_BitMasks::IFD_EXIF, UNDEFINED, CharData   , exif_version              , count_string         )    \
x(0x9003, IFD_BitMasks::IFD_EXIF, ASCII    , DateTime   , date_time_original        , count_string         )    \
x(0x9004, IFD_BitMasks::IFD_EXIF, ASCII    , DateTime   , date_time_digitized       , count_string         )    \
x(0x9290, IFD_BitMasks::IFD_EXIF, ASCII    , uint16_t   , subsectime                , count_string         )    \
x(0x9291, IFD_BitMasks::IFD_EXIF, ASCII    , uint16_t   , subsectime_original       , count_string         )    \
x(0x9292, IFD_BitMasks::IFD_EXIF, ASCII    , uint16_t   , subsectime_digitized      , count_string         )    \
x(0xa430, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , camera_owner_name         , count_string         )    \
x(0xa431, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , body_serial_number        , count_string         )    \
x(0xa433, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , lens_make                 , count_string         )    \
x(0xa434, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , lens_model                , count_string         )    \
x(0xa435, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , lens_serial_number        , count_string         )    \
x(0xa436, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , image_title               , count_string         )    \
x(0xa437, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , photographer              , count_string         )    \
x(0xa438, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , image_editor              , count_string         )    \
x(0xa43a, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , raw_developing_software   , count_string         )    \
x(0xa43b, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , image_editing_software    , count_string         )    \
x(0xa43c, IFD_BitMasks::IFD_EXIF, ASCII    , CharData   , metadata_editing_software , count_string         )    \
  //
// clang-format on
#define TAG_ENUM(tag, ifd_bitmask, expected_tiff_type, cpp_type, name, count) name = tag##u,

enum class TagId : uint16_t {
  // clang-format off
  ALL_IFD0_TAGS(TAG_ENUM)
  ALL_EXIF_TAGS(TAG_ENUM)
  // clang-format on
};
inline bool operator==(uint16_t l, TagId r)
{
  return l == (uint16_t)r;
}
inline bool operator==(TagId l, uint16_t r)
{
  return (uint16_t)l == r;
}

#undef TAG_ENUM

template <uint16_t _TagId, uint16_t _IFD_BitMask>
struct TagInfo {
  constexpr static bool defined = false;
  constexpr static uint16_t TagId = _TagId;
  constexpr static uint16_t IFD_BitMask = _IFD_BitMask;
  constexpr static DType tiff_type = (DType)0;
  using scalar_cpp_type = void;
  constexpr static int exif_count = 0;
  constexpr static int cpp_count = 0;
  constexpr static const char *name = nullptr;
};

namespace {
template <typename T, int CppCount, bool Var>
struct cpp_count_helper {};
template <typename T>
struct cpp_count_helper<T, 1, false> {
  using type = T;
};
template <typename T>
struct cpp_count_helper<T, 0, true> {
  using type = std::vector<T>;
};
template <typename T, int C>
struct cpp_count_helper<T, C, true> {
  using type = vla<T, C>;
};
template <typename T, int C>
struct cpp_count_helper<T, C, false> {
  using type = std::array<T, C>;
};
}  // namespace

#define TEMPLATE_TAG_INFO(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count_spec)           \
  template <>                                                                                               \
  struct TagInfo<_tag##u, _ifd_bitmask> {                                                                   \
    constexpr static bool defined = true;                                                                   \
    constexpr static uint16_t TagId = _tag##u;                                                              \
    constexpr static uint16_t IFD_BitMask = _ifd_bitmask;                                                   \
    constexpr static DType tiff_type = DType::_expected_tiff_type;                                          \
    using scalar_cpp_type = _cpp_type;                                                                      \
    using cpp_type = cpp_count_helper<scalar_cpp_type, _count_spec::cpp_count, _count_spec::cpp_var>::type; \
    using count_spec = _count_spec;                                                                         \
    constexpr static const char *name = #_name;                                                             \
  };                                                                                                        \
  using tag_##_name = TagInfo<_tag##u, _ifd_bitmask>;

ALL_IFD0_TAGS(TEMPLATE_TAG_INFO);
ALL_EXIF_TAGS(TEMPLATE_TAG_INFO);
#undef TEMPLATE_TAG_INFO

const char *to_str(uint16_t tag, uint16_t ifd_bit);

}  // namespace tiff
}  // namespace nexif
