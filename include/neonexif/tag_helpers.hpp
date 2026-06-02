#pragma once

#include "neonexif.hpp"
#include <vector>

#define NEXIF_TAG_ENUM_ENTRY(tag, ifd_bitmask, expected_tiff_type, cpp_type, name, count) name = tag##u,

#define NEXIF_MAKE_TAG_ENUM(TAG_LIST) \
  enum class TagId : uint16_t {       \
    TAG_LIST(NEXIF_TAG_ENUM_ENTRY)    \
  }

#define NEXIF_TAG_TO_STR(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count) \
  if (tag == _tag##u && (ifd_bit & _ifd_bitmask)) {                                         \
    return #_name;                                                                          \
  }

#define NEXIF_MAKE_TO_STRING(TAG_LIST)               \
  const char *to_str(uint16_t tag, uint16_t ifd_bit) \
  {                                                  \
    TAG_LIST(NEXIF_TAG_TO_STR);                      \
    return nullptr;                                  \
  }

#define NEXIF_MAKE_TAG_CMP                    \
  inline bool operator==(uint16_t l, TagId r) \
  {                                           \
    return l == (uint16_t)r;                  \
  }                                           \
  inline bool operator==(TagId l, uint16_t r) \
  {                                           \
    return (uint16_t)l == r;                  \
  }

#define NEXIF_DECLARE_TAG_INFO                      \
  template <uint16_t _TagId, uint16_t _IFD_BitMask> \
  struct TagInfo

namespace nexif {
namespace tiff {
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
  using type = nexif::vla<T, C>;
};
template <typename T, int C>
struct cpp_count_helper<T, C, false> {
  using type = std::array<T, C>;
};
}  // namespace

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
}  // namespace tiff
}  // namespace nexif

#define NEXIF_TEMPLATE_TAG_INFO(_tag, _ifd_bitmask, _expected_tiff_type, _cpp_type, _name, _count_spec) \
  template <>                                                                                           \
  struct TagInfo<_tag##u, _ifd_bitmask> {                                                               \
    constexpr static uint16_t TagId = _tag##u;                                                          \
    constexpr static uint16_t IFD_BitMask = _ifd_bitmask;                                               \
    constexpr static ::nexif::tiff::DType tiff_type = ::nexif::tiff::DType::_expected_tiff_type;        \
    using scalar_cpp_type = _cpp_type;                                                                  \
    using cpp_type = ::nexif::tiff::cpp_count_helper<                                                   \
      scalar_cpp_type,                                                                                  \
      ::nexif::tiff::_count_spec::cpp_count,                                                            \
      ::nexif::tiff::_count_spec::cpp_var>::type;                                                       \
    using count_spec = ::nexif::tiff::_count_spec;                                                      \
    constexpr static const char *name = #_name;                                                         \
  };                                                                                                    \
  using tag_##_name = TagInfo<_tag##u, _ifd_bitmask>;
