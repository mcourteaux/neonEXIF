#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <variant>

namespace nexif {

struct ParseError {
  enum Code {
    CANNOT_OPEN_FILE,
    UNKNOWN_FILE_TYPE,
    CORRUPT_DATA,
    INTERNAL_ERROR,
  } code;
  const char *message{nullptr};
  const char *what{nullptr};
};

struct ParseWarning {
  const char *msg{nullptr};
  const char *what{nullptr};
};

template <typename T>
struct ParseResult {
  std::variant<T, ParseError> _v;
  std::list<ParseWarning> warnings;

  // clang-format off
  ParseResult(T t) : _v(t) {}
  ParseResult(ParseError::Code code, const char *msg) : _v(ParseError(code, msg)) {}
  ParseResult(ParseError err) : _v(err) {}
  // clang-format on

  operator bool() const
  {
    return _v.index() == 0;
  }
  const T &value() const
  {
    return std::get<0>(_v);
  }
  const ParseError &error() const
  {
    return std::get<1>(_v);
  }
};

enum FileType { TIFF,
                CIFF,
                JPEG };
enum FileTypeVariant { STANDARD,
                       TIFF_ORF,
                       TIFF_RW2 };

inline const char *to_str(FileType ft)
{
  switch (ft) {
    case TIFF: return "TIFF";
    case CIFF: return "CIFF";
    case JPEG: return "JPEG";
  }
  std::abort();
}

/** Data struct holding relative to `this` pointer to a const char*, meant for
 * refering to string data stored in a buffer closeby. */
struct CharData {
  CharData() = default;
  CharData(const CharData &o)
  {
    operator=(o.data());
  }
  CharData &operator=(const CharData &o)
  {
    return operator=(o.data());
  }

  CharData(const char *ptr)
  {
    operator=(ptr);
  }
  int16_t ptr_offset{0};

  CharData &operator=(const char *ptr)
  {
    if (ptr == nullptr) {
      ptr_offset = 0;
    } else {
      ptr_offset = (ptr - (const char *)this);
    }
    return *this;
  }

  const char *data() const
  {
    if (ptr_offset == 0)
      return nullptr;
    return (((const char *)this) + ptr_offset);
  }
};

template <typename T>
struct rational {
  T num, denom;
  operator float() const { return float(num) / denom; }
  operator double() const { return double(num) / denom; }
};

using rational64s = rational<int32_t>;
using rational64u = rational<uint32_t>;

enum Orientation : uint16_t {
  HORIZONTAL = 1,
  MIRROR_HORIZONTAL = 2,
  ROTATE_180 = 3,
  MIRROR_VERTICAL = 4,
  MIRROR_HORIZONTAL_ROTATE_270CW = 5,
  ROTATE_90CW = 6,
  MIRROR_HORIZONTAL_ROTATE_90CW = 7,
  ROTATE_270CW = 8
};

inline const char *to_str(Orientation ori)
{
  switch (ori) {
    case HORIZONTAL: return "Horizontal";
    case MIRROR_HORIZONTAL: return "Mirror Horizontal";
    case ROTATE_180: return "Rotate 180";
    case MIRROR_VERTICAL: return "Mirror Vertical";
    case MIRROR_HORIZONTAL_ROTATE_270CW: return "Mirror Horizontal Rotate 270CW";
    case ROTATE_90CW: return "Rotate 90CW";
    case MIRROR_HORIZONTAL_ROTATE_90CW: return "Mirror Horizontal Rotate 90CW";
    case ROTATE_270CW: return "Rotate 270CW";
  }
  std::abort();
}

enum SubfileType {
  NONE,
  FULL_RESOLUTION,
  REDUCED_RESOLUTION,
  OTHER,
};

inline const char *to_str(SubfileType ft)
{
  switch (ft) {
    case NONE: return "None";
    case FULL_RESOLUTION: return "Full Resolution";
    case REDUCED_RESOLUTION: return "Reduced Resolution";
    case OTHER: return "OTHER";
  }
  std::abort();
}

template <typename T>
struct Tag {
  bool is_set{false};
  uint16_t parsed_from{0};
  T value{};

  Tag &operator=(T v)
  {
    value = v;
    is_set = true;
    return *this;
  }

  operator T &() { return value; }
  operator const T &() const { return value; }
};

struct ImageData {
  SubfileType type{NONE};
  Tag<uint32_t> image_width;
  Tag<uint32_t> image_height;
  Tag<uint16_t> compression;
  Tag<uint16_t> photometric_interpretation;
  Tag<Orientation> orientation;
  Tag<uint16_t> samples_per_pixel;
  Tag<rational64u> x_resolution;
  Tag<rational64u> y_resolution;
  Tag<uint16_t> resolution_unit;

  Tag<uint32_t> data_offset;
  Tag<uint32_t> data_length;
};

struct ExifIFD {
  // Shot
  Tag<rational64u> exposure_time;
  Tag<rational64u> f_number;
  Tag<uint16_t> iso;
  Tag<uint16_t> exposure_program;

  // Lens
  Tag<rational64u> focal_length;
  Tag<rational64u> aperture_value;

  Tag<CharData> exif_version;
};

struct ExifData {
  FileType file_type;
  FileTypeVariant file_type_variant;
  ImageData images[5];
  int num_images{0};

  Tag<CharData> copyright;
  Tag<CharData> artist;
  Tag<CharData> make;
  Tag<CharData> model;
  Tag<CharData> software;

  ExifIFD exif;

  char string_data[4096];
  int string_data_ptr{0};
};

ParseResult<ExifData> read_exif(const std::filesystem::path &path);
}  // namespace nexif
