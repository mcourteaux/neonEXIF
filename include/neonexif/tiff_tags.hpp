#pragma once

#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "neonexif.hpp"
#include "tiff.hpp"

namespace nexif {
namespace tiff {

// clang-format off
#define NEXIF_ALL_IFD0_TAGS(x)                \
x(0x0001, IFD_01 , ASCII    , CharData   , interop_index              , count_string         )    \
x(0x0002, IFD_01 , UNDEFINED, CharData   , interop_version            , count_scalar         )    \
x(0x000b, IFD_01 , ASCII    , CharData   , processing_software        , count_string         )    \
x(0x00fe, IFD_01 , LONG     , uint32_t   , subfile_type               , count_scalar         )    \
x(0x00ff, IFD_01 , SHORT    , uint16_t   , old_subfile_type           , count_scalar         )    \
x(0x0100, IFD_01 , LONG     , uint32_t   , image_width                , count_scalar         )    \
x(0x0101, IFD_01 , LONG     , uint32_t   , image_height               , count_scalar         )    \
x(0x0102, IFD_01 , LONG     , uint32_t   , bits_per_sample            , count_limvar<8>      )    \
x(0x0103, IFD_01 , SHORT    , uint16_t   , compression                , count_scalar         )    \
x(0x0106, IFD_01 , SHORT    , uint16_t   , photometric_interpretation , count_scalar         )    \
x(0x010f, IFD_01 , ASCII    , CharData   , make                       , count_string         )    \
x(0x0110, IFD_01 , ASCII    , CharData   , model                      , count_string         )    \
x(0x0112, IFD_01 , SHORT    , Orientation, orientation                , count_scalar         )    \
x(0x0115, IFD_01 , SHORT    , uint16_t   , samples_per_pixel          , count_scalar         )    \
x(0x011a, IFD_01 , RATIONAL , rational64u, x_resolution               , count_scalar         )    \
x(0x011b, IFD_01 , RATIONAL , rational64u, y_resolution               , count_scalar         )    \
x(0x0128, IFD_01 , SHORT    , uint16_t   , resolution_unit            , count_scalar         )    \
x(0x0131, IFD_01 , ASCII    , CharData   , software                   , count_string         )    \
x(0x0132, IFD_01 , ASCII    , DateTime   , date_time                  , count_string         )    \
x(0x013b, IFD_01 , ASCII    , CharData   , artist                     , count_string         )    \
x(0x0201, IFD_01 , LONG     , uint32_t   , data_offset                , count_scalar         )    \
x(0x0202, IFD_01 , LONG     , uint32_t   , data_length                , count_scalar         )    \
x(0x8298, IFD_01 , ASCII    , CharData   , copyright                  , count_string         )    \
x(0x8769, IFD_01 , LONG     , uint32_t   , exif_offset                , count_scalar         )    \
x(0x014a, IFD_01 , LONG     , uint32_t   , sub_ifd_offset             , count_var            )    \
x(0x927c, IFD_ALL, UNDEFINED, uint8_t    , makernote                  , count_var            )    \
x(0x002e, IFD_ALL, UNDEFINED, uint8_t    , makernote_alt              , count_var            )    \
x(0xc621, IFD_01 , SRATIONAL, rational64s, color_matrix_1             , count_limvar<12>     )    \
x(0xc622, IFD_01 , SRATIONAL, rational64s, color_matrix_2             , count_limvar<12>     )    \
x(0xc623, IFD_01 , SRATIONAL, rational64s, calibration_matrix_1       , count_limvar<12>     )    \
x(0xc624, IFD_01 , SRATIONAL, rational64s, calibration_matrix_2       , count_limvar<12>     )    \
x(0xc625, IFD_01 , SRATIONAL, rational64s, reduction_matrix_1         , count_limvar<12>     )    \
x(0xc626, IFD_01 , SRATIONAL, rational64s, reduction_matrix_2         , count_limvar<12>     )    \
x(0xc627, IFD_01 , RATIONAL , rational64u, analog_balance             , count_limvar<4>      )    \
x(0xc628, IFD_01 , RATIONAL , rational64u, as_shot_neutral            , count_limvar<4>      )    \
x(0xc629, IFD_01 , RATIONAL , rational64u, as_shot_white_xy           , count_fixed<2>       )    \
x(0xc65a, IFD_01 , SHORT    , Illuminant , calibration_illuminant_1   , count_scalar         )    \
x(0xc65b, IFD_01 , SHORT    , Illuminant , calibration_illuminant_2   , count_scalar         )    \
x(0x882a, IFD_01 , SSHORT   , Illuminant , timezone_offset            , count_scalar         )    \
x(0x9201, IFD_01 , SRATIONAL, rational64s, apex_aperture_value        , count_scalar         )    \
x(0x9202, IFD_01 , SRATIONAL, rational64s, apex_shutter_speed_value   , count_scalar         )

// clang-format on

// clang-format off
#define NEXIF_ALL_EXIF_TAGS(x)              \
x(0x829a, IFD_EXIF, RATIONAL , rational64u, exposure_time             , count_scalar         )    \
x(0x829d, IFD_EXIF, RATIONAL , rational64u, f_number                  , count_scalar         )    \
x(0x8827, IFD_EXIF, SHORT    , uint16_t   , iso                       , count_scalar         )    \
x(0x8822, IFD_EXIF, SHORT    , uint16_t   , exposure_program          , count_scalar         )    \
x(0x920a, IFD_ALL , RATIONAL , rational64u, focal_length              , count_scalar         )    \
x(0x9000, IFD_EXIF, UNDEFINED, CharData   , exif_version              , count_string         )    \
x(0x9003, IFD_EXIF, ASCII    , DateTime   , date_time_original        , count_string         )    \
x(0x9004, IFD_EXIF, ASCII    , DateTime   , date_time_digitized       , count_string         )    \
x(0x9290, IFD_EXIF, ASCII    , uint16_t   , subsectime                , count_string         )    \
x(0x9291, IFD_EXIF, ASCII    , uint16_t   , subsectime_original       , count_string         )    \
x(0x9292, IFD_EXIF, ASCII    , uint16_t   , subsectime_digitized      , count_string         )    \
x(0xa430, IFD_EXIF, ASCII    , CharData   , camera_owner_name         , count_string         )    \
x(0xa431, IFD_EXIF, ASCII    , CharData   , body_serial_number        , count_string         )    \
x(0xa432, IFD_EXIF, RATIONAL , rational64u, lens_specification        , count_fixed<4>       )    \
x(0xa433, IFD_EXIF, ASCII    , CharData   , lens_make                 , count_string         )    \
x(0xa434, IFD_EXIF, ASCII    , CharData   , lens_model                , count_string         )    \
x(0xa435, IFD_EXIF, ASCII    , CharData   , lens_serial_number        , count_string         )    \
x(0xa436, IFD_EXIF, ASCII    , CharData   , image_title               , count_string         )    \
x(0xa437, IFD_EXIF, ASCII    , CharData   , photographer              , count_string         )    \
x(0xa438, IFD_EXIF, ASCII    , CharData   , image_editor              , count_string         )    \
x(0xa43a, IFD_EXIF, ASCII    , CharData   , raw_developing_software   , count_string         )    \
x(0xa43b, IFD_EXIF, ASCII    , CharData   , image_editing_software    , count_string         )    \
x(0xa43c, IFD_EXIF, ASCII    , CharData   , metadata_editing_software , count_string         )    \
  //

// clang-format on

#define TAG_ENUM(tag, ifd_bitmask, expected_tiff_type, cpp_type, name, count) name = tag##u,

enum class TagId : uint16_t {
  // clang-format off
  NEXIF_ALL_IFD0_TAGS(TAG_ENUM)
  NEXIF_ALL_EXIF_TAGS(TAG_ENUM)
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
struct TagInfo;

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

#define NEXIF_TEMPLATE_TAG_INFO(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count_spec) \
  template <>                                                                                           \
  struct TagInfo<_tag##u, _ifd_bitmask> {                                                               \
    constexpr static uint16_t TagId = _tag##u;                                                          \
    constexpr static uint16_t IFD_BitMask = _ifd_bitmask;                                               \
    constexpr static nexif::tiff::DType tiff_type = nexif::tiff::DType::_expected_tiff_type;            \
    using scalar_cpp_type = _cpp_type;                                                                  \
    using cpp_type = nexif::tiff::cpp_count_helper<                                                     \
      scalar_cpp_type,                                                                                  \
      nexif::tiff::_count_spec::cpp_count,                                                              \
      nexif::tiff::_count_spec::cpp_var>::type;                                                         \
    using count_spec = nexif::tiff::_count_spec;                                                        \
    constexpr static const char *name = #_name;                                                         \
  };                                                                                                    \
  using tag_##_name = TagInfo<_tag##u, _ifd_bitmask>;

NEXIF_ALL_IFD0_TAGS(NEXIF_TEMPLATE_TAG_INFO);
NEXIF_ALL_EXIF_TAGS(NEXIF_TEMPLATE_TAG_INFO);

const char *to_str(uint16_t tag, uint16_t ifd_bit);

}  // namespace tiff
}  // namespace nexif
