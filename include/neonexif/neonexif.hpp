#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <variant>
#include <vector>
#include <cassert>
#include <cstring>
#include <array>
#include <bit>

namespace nexif {

inline int __indent = 0;
inline bool enable_debug_print = false;
inline int default_debug_print(const char *v)
{
  return std::printf("\033[2m[NeonEXIF] %s\033[0m\n", v);
}
inline int (*debug_print_fn)(const char *) = default_debug_print;
struct Indenter {
  Indenter() { __indent++; }
  ~Indenter() { __indent--; }
};
#define DEBUG_PRINT(fmt, args...)                                           \
  if (enable_debug_print) {                                                 \
    char _buf[1024];                                                        \
    std::snprintf(_buf, sizeof(_buf), "%*s" fmt, __indent * 4, "", ##args); \
    debug_print_fn(_buf);                                                   \
  }

template <typename T>
inline T byteswap(T t)
{
  if constexpr (std::is_same_v<T, float>) {
    uint32_t v;
    std::memcpy(&v, &t, 4);
    v = byteswap(v);
    std::memcpy(&t, &v, 4);
    return t;
  } else if constexpr (std::is_same_v<T, double>) {
    uint64_t v;
    std::memcpy(&v, &t, 8);
    v = byteswap(v);
    std::memcpy(&t, &v, 8);
    return t;
  } else {
#ifdef __cpp_lib_byteswap
    return std::byteswap(t);
#else
    static_assert(sizeof(T) <= 8);
    static_assert((sizeof(T) & (sizeof(T) - 1)) == 0);
    if constexpr (sizeof(T) == 1)
      return t;
    if constexpr (sizeof(T) == 2)
      return __builtin_bswap16(t);
    if constexpr (sizeof(T) == 4)
      return __builtin_bswap32(t);
    if constexpr (sizeof(T) == 8)
      return __builtin_bswap64(t);
#endif
  }
}

struct ParseError {
  enum Code {
    CANNOT_OPEN_FILE,
    UNKNOWN_FILE_TYPE,
    CORRUPT_DATA,
    TAG_NOT_FOUND,
    INTERNAL_ERROR,
  } code;
  const char *message{nullptr};
  const char *what{nullptr};
};

inline const char *to_str(ParseError::Code c)
{
  switch (c) {
    case ParseError::CANNOT_OPEN_FILE: return "Cannot open file";
    case ParseError::UNKNOWN_FILE_TYPE: return "Unknown file type";
    case ParseError::CORRUPT_DATA: return "Corrupt data";
    case ParseError::TAG_NOT_FOUND: return "Tag not found";
    case ParseError::INTERNAL_ERROR: return "Internal error";
  }
  std::abort();
}

struct ParseWarning {
  const char *msg{nullptr};
  const char *what{nullptr};
};

template <typename T>
struct [[nodiscard]] ParseResult {
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

#define DECL_OR_RETURN(_type, _var, _call) \
  _type _var;                              \
  {                                        \
    if (auto _r = _call) {                 \
      _var = _r.value();                   \
    } else {                               \
      return _r.error();                   \
    }                                      \
  }

#define ASSIGN_OR_RETURN(_var, _call) \
  {                                   \
    if (auto _r = _call) {            \
      _var = _r.value();              \
    } else {                          \
      return _r.error();              \
    }                                 \
  }

enum FileType {
  TIFF,
  CIFF,
  JPEG,
  FUJIFILM_RAF,
  MRW,
  SIGMA_FOVB
};
enum FileTypeVariant {
  STANDARD,
  TIFF_ORF,
  TIFF_RW2,
};

inline const char *to_str(FileType ft)
{
  switch (ft) {
    case TIFF: return "TIFF";
    case CIFF: return "CIFF";
    case JPEG: return "JPEG";
    case FUJIFILM_RAF: return "RAF";
    case MRW: return "MRW";
    case SIGMA_FOVB: return "FOVb";
  }
  std::abort();
}

inline const char *to_str(FileType ft, FileTypeVariant v)
{
  switch (ft) {
    case TIFF:
      switch (v) {
        case STANDARD: return "TIFF";
        case TIFF_ORF: return "TIFF/ORF";
        case TIFF_RW2: return "TIFF/RW2";
        default: std::abort();
      }
    case CIFF:
      switch (v) {
        case STANDARD: return "CIFF";
        default: std::abort();
      }
    case JPEG: return "JPEG";
    case FUJIFILM_RAF: return "RAF";
    case MRW: return "MRW";
    case SIGMA_FOVB: return "FOVb";
  }
  std::abort();
}

/** Data struct holding relative to `this` pointer to a const char*, meant for
 * refering to string data stored in a buffer closeby. */
struct CharData {
  CharData() = default;

  CharData(const CharData &o) = delete;
  CharData &operator=(const CharData &o) = delete;

  CharData &operator=(const std::string_view &sv)
  {
    assert(sv.length() <= 0xffff && "string too long");
    set(sv.data(), sv.length());
    return *this;
  }

  CharData(const char *ptr, uint16_t len)
  {
    set(ptr, len);
  }

  CharData(const std::string_view &sv)
  {
    assert(sv.length() <= 0xffff && "string too long");
    set(sv.data(), sv.length());
  }

  int16_t ptr_offset{0};
  uint16_t length{0};

  CharData &set(const char *ptr, uint16_t len)
  {
    if (ptr == nullptr) {
      ptr_offset = 0;
      assert(len == 0);
    } else {
      ptrdiff_t ptrdiff = (ptr - (const char *)this);
      assert(static_cast<int16_t>(ptrdiff) == ptrdiff && "data not close enough in memory to the CharData");
      ptr_offset = ptrdiff;
    }
    length = len;
    return *this;
  }

  const char *data() const
  {
    if (ptr_offset == 0)
      return nullptr;
    return (((const char *)this) + ptr_offset);
  }

  const std::string_view view() const
  {
    return {data(), length};
  }

  const std::string str() const
  {
    return {data(), length};
  }
};

template <typename T>
struct rational {
  T num, denom;
  operator float() const { return float(num) / denom; }
  operator double() const { return double(num) / denom; }

  inline bool operator==(const rational<T> &other) const
  {
    return num == other.num && denom == other.denom;
  }
};

using rational64s = rational<int32_t>;
using rational64u = rational<uint32_t>;

rational64u double_to_rational64u(double value, double accuracy = 1e-4);
rational64s double_to_rational64s(double value, double accuracy = 1e-4);

/** Variable length array */
template <typename T, uint8_t Max>
struct vla {
  T values[Max];
  uint8_t num{0};

  inline void push_back(const T &v)
  {
    values[num++] = v;
  }
};

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

enum class Illuminant : uint16_t {
  Unknown = 0,
  Daylight = 1,
  Fluorescent = 2,
  Tungsten_Incandescent_Light = 3,
  Flash = 4,
  Fine_Weather = 9,
  Cloudy_Weather = 10,
  Shade = 11,
  Daylight_Fluorescent = 12,    //  (D 5700 - 7100K)
  Day_White_Fluorescent = 13,   // (N 4600 - 5400K)
  Cool_White_Fluorescent = 14,  // (W 3900 - 4500K)
  White_Fluorescent = 15,       // (WW 3200 - 3700K)
  Standard_A = 17,              // Standard light A
  Standard_B = 18,
  Standard_C = 19,
  D55 = 20,
  D65 = 21,
  D75 = 22,
  D50 = 23,
  ISO_Studio_Tungsten = 24,
};

inline const char *to_str(Illuminant i)
{
#define X(v) \
  case Illuminant::v: return #v
  switch (i) {
    X(Unknown);
    X(Daylight);
    X(Fluorescent);
    X(Tungsten_Incandescent_Light);
    X(Flash);
    X(Fine_Weather);
    X(Cloudy_Weather);
    X(Shade);
    X(Daylight_Fluorescent);
    X(Day_White_Fluorescent);
    X(Cool_White_Fluorescent);
    X(White_Fluorescent);
    X(Standard_A);
    X(Standard_B);
    X(Standard_C);
    X(D55);
    X(D65);
    X(D75);
    X(D50);
    X(ISO_Studio_Tungsten);
  }
#undef X
  std::abort();
}

inline void illuminant_chromaticity(Illuminant i, double *x, double *y)
{
#define CASE(v) case Illuminant::v
#define RET(_x, _y) \
  *x = _x;          \
  *y = _y;          \
  return
  switch (i) {
    CASE(Unknown) :
      RET(0.3333, 0.3333);

    CASE(Daylight) :
      CASE(D65) :
      CASE(Fine_Weather) :
      RET(031272, 0.32903);

    CASE(Cloudy_Weather) :
      CASE(Shade) :
      CASE(D75) :
      RET(0.29902, 0.31485);

    CASE(Daylight_Fluorescent) :
      CASE(Day_White_Fluorescent) :
      RET(0.31310, 0.33727);  // I got nothing...

    CASE(Fluorescent) :
      CASE(Cool_White_Fluorescent) :
      RET(0.37208, 0.37529);
    CASE(White_Fluorescent) :
      RET(0.40910, 0.39430);

    CASE(Tungsten_Incandescent_Light) :
      CASE(ISO_Studio_Tungsten) :
      CASE(Standard_A) :
      RET(0.44757, 0.40745);

    CASE(Standard_B) :
      RET(0.34842, 0.35161);

    CASE(Standard_C) :
      RET(0.31006, 0.31616);

    CASE(Flash) :
      CASE(D55) :
      RET(0.33242, 0.34743);

    CASE(D50) :
      RET(0.34567, 0.35850);
  }
#undef CASE
#undef RET
  std::abort();
}

template <typename T>
struct Tag {
  bool is_set{false};
  uint16_t parsed_from{0};
  T value{};

  Tag &operator=(T v)
  {
    if constexpr (std::is_same_v<CharData, T>) {
      if (v.data() == nullptr || v.len == 0) {
        value.set(nullptr, 0);
        is_set = false;
        return *this;
      }
    }
    value = v;
    is_set = true;
    return *this;
  }

  template <typename O>
  Tag &operator=(O v)
  {
    if constexpr (std::is_same_v<std::string_view, O>) {
      if (v.data() == nullptr) {
        value.set(nullptr, 0);
        is_set = false;
        return *this;
      }
    }
    value = v;
    is_set = true;
    return *this;
  }

  operator bool() const
  {
    return is_set;
  }

  const T &value_or(const T &fallback) const
  {
    if (is_set) {
      return value;
    } else {
      return fallback;
    }
  }

  T &operator->() { return value; }
  const T operator->() const { return value; }
  T &operator*() { return value; }
  const T operator*() const { return value; }

  void clear()
  {
    is_set = false;
    parsed_from = 0;
    value = {};
  }
};

struct DateTime {
  int32_t year{0};
  int8_t month{0};
  int8_t day{0};

  int8_t hour{0};
  int8_t minute{0};
  int8_t second{0};

  uint16_t millis{0};
  uint32_t timezone_offset{0};

  inline int64_t monotonic() const
  {
    int64_t m = ((year * 12) + month) * 31 + day;
    m = ((m * 24 + hour - timezone_offset) * 60 + minute) * 60 + second;
    m = m * 1000 + millis;
    return m;
  }
};

namespace tiff {
ParseResult<DateTime> parse_date_time(std::string_view str);
}

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
  Tag<rational64u> exposure_time;
  Tag<rational64u> focal_length;
  Tag<rational64u> f_number;
  Tag<uint16_t> iso;
  Tag<uint16_t> exposure_program;
  Tag<DateTime> date_time_original;
  Tag<DateTime> date_time_digitized;

  Tag<CharData> exif_version;

  Tag<CharData> camera_owner_name;
  Tag<CharData> body_serial_number;

  Tag<std::array<rational64u, 4>> lens_specification;  ///< (MinFocalLen, MaxFocalLen, MinFNum@MinFL, MinFNum@MaxFL)
  Tag<CharData> lens_make;
  Tag<CharData> lens_model;
  Tag<CharData> lens_serial_number;

  Tag<CharData> image_title;
  Tag<CharData> photographer;
  Tag<CharData> image_editor;  ///< A person.
  Tag<CharData> raw_developing_software;
  Tag<CharData> image_editing_software;
  Tag<CharData> metadata_editing_software;
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
  Tag<CharData> processing_software;
  Tag<DateTime> date_time;

  Tag<vla<rational64s, 12>> color_matrix_1;
  Tag<vla<rational64s, 12>> color_matrix_2;
  Tag<vla<rational64s, 12>> reduction_matrix_1;
  Tag<vla<rational64s, 12>> reduction_matrix_2;
  Tag<vla<rational64s, 12>> calibration_matrix_1;
  Tag<vla<rational64s, 12>> calibration_matrix_2;
  Tag<Illuminant> calibration_illuminant_1;
  Tag<Illuminant> calibration_illuminant_2;
  Tag<vla<rational64u, 4>> as_shot_neutral;
  Tag<std::array<rational64u, 2>> as_shot_white_xy;
  Tag<vla<rational64u, 4>> analog_balance;

  Tag<rational64s> apex_aperture_value;
  Tag<rational64s> apex_shutter_speed_value;

  ExifIFD exif;

  char string_data[4096];
  uint32_t string_data_ptr{0};

  ExifData() = default;
  ExifData &operator=(const ExifData &o)
  {
    std::memcpy((void *)this, (void *)&o, sizeof(ExifData));
    return *this;
  }
  ExifData &operator=(ExifData &&o)
  {
    std::memcpy((void *)this, (void *)&o, sizeof(ExifData));
    return *this;
  }
  ExifData(const ExifData &o)
  {
    operator=(o);
  }
  ExifData(ExifData &&o)
  {
    operator=(std::move(o));
  }

  const ImageData *full_resolution_image() const
  {
    for (int i = 0; i < num_images; ++i) {
      if (images[i].type == FULL_RESOLUTION) {
        return &images[i];
      }
    }
    return nullptr;
  }

  std::string_view store_string_data(const char *ptr, uint32_t count = 0)
  {
    if (count == 0) {
      count = std::strlen(ptr);
      if (count == 0) {
        return {nullptr, 0};
      }
    }
    assert(string_data_ptr + count < sizeof(string_data) && "out of string storage");
    char *dst = &string_data[string_data_ptr];
    std::memcpy(dst, ptr, count);
    string_data[string_data_ptr + count] = 0;
    string_data_ptr += count + 1;
    return {dst, (size_t)count};
  }

  std::string_view store_string_data(const std::string &str)
  {
    return store_string_data(str.data(), str.length());
  }
};

ParseResult<ExifData> read_exif(
  const char *buffer,
  size_t length,
  FileType *ft = nullptr,
  FileTypeVariant *fvt = nullptr
);

ParseResult<ExifData> read_exif(
  const std::filesystem::path &path,
  FileType *ft = nullptr,
  FileTypeVariant *fvt = nullptr
);

size_t write_exif_data(const ExifData &data, std::vector<uint8_t> &output);

std::vector<uint8_t> generate_exif_jpeg_binary_data(const ExifData &data);

}  // namespace nexif
