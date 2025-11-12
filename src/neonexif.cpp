#include "neonexif/neonexif.hpp"
#include "neonexif/reader.hpp"

#include <cstring>
#include <cassert>
#include <optional>
#include <cmath>
#include <string_view>

namespace nexif {

bool debug_print_enabled = false;

namespace tiff {
std::optional<ParseError> read_tiff(Reader &r, ExifData &data);
size_t write_tiff(Writer &w, const ExifData &data);
};  // namespace tiff

/**
 * Taken from https://stackoverflow.com/a/42197629/155137
 * Translated by ChatGPT here:
 * https://chatgpt.com/c/68bef95a-32fc-8320-825d-86f3e99ff95c
 */
rational64s double_to_rational64s(double value, double accuracy)
{
  if (accuracy <= 0.0 || accuracy >= 1.0)
    throw std::out_of_range("accuracy must be > 0 and < 1");

  int sign = (value > 0) - (value < 0);
  if (sign < 0)
    value = -value;

  double maxError = (sign == 0) ? accuracy : value * accuracy;
  uint32_t n = static_cast<uint32_t>(std::floor(value));
  value -= n;

  if (value < maxError)
    return {static_cast<int32_t>(sign * (int)n), 1};

  if (1 - maxError < value)
    return {static_cast<int32_t>(sign * (int)(n + 1)), 1};

  double z = value;
  int32_t prevDen = 0, den = 1, num = 0;

  do {
    z = 1.0 / (z - std::floor(z));
    uint32_t temp = den;
    den = den * static_cast<uint32_t>(std::floor(z)) + prevDen;
    prevDen = temp;
    num = static_cast<uint32_t>(std::round(value * den));
  } while (std::fabs(value - double(num) / den) > maxError && z != std::floor(z));

  return {static_cast<int32_t>((n * den + num) * sign), den};
}

rational64u double_to_rational64u(double value, double accuracy)
{
  const rational64s r = double_to_rational64s(value, accuracy);
  return {uint32_t(r.num), uint32_t(r.denom)};
}

namespace {

bool guess_file_type(Reader &reader)
{
  const char *data = reader.data;

  using namespace std::string_view_literals;
  const std::string_view fujifilm_magic = "FUJIFILMCCD-RAW"sv;
  if (std::memcmp(data, fujifilm_magic.data(), fujifilm_magic.length()) == 0) {
    reader.file_type = FUJIFILM_RAF;
    reader.file_type_variant = STANDARD;
    return true;
  }
  const std::string_view mrw_magic = "\0MRM"sv;
  if (std::memcmp(data, mrw_magic.data(), mrw_magic.length()) == 0) {
    reader.file_type = MRW;
    reader.file_type_variant = STANDARD;
    return true;
  }
  const std::string_view fovb_magic = "FOVb"sv;
  if (std::memcmp(data, fovb_magic.data(), fovb_magic.length()) == 0) {
    reader.file_type = SIGMA_FOVB;
    reader.file_type_variant = STANDARD;
    return true;
  }

  bool byte_order_indicator_found = false;
  if (data[0] == char(0xff) && data[1] == char(0xd8) && data[2] == char(0xff)) {
    // JPEG SOI marker + first byte of next marker.
    reader.file_type = JPEG;
    reader.file_type_variant = STANDARD;
    reader.byte_order = ByteOrder::Minolta;
    return true;
  } else if (data[0] == 'I' && data[1] == 'I') {
    reader.byte_order = Intel;
    DEBUG_PRINT("Byte order Intel");
    byte_order_indicator_found = true;
    if (reader.skip(2)) {
      return false;
    }
  } else if (data[0] == 'M' && data[1] == 'M') {
    reader.byte_order = Minolta;
    DEBUG_PRINT("Byte order Minolta");
    byte_order_indicator_found = true;
    if (reader.skip(2)) {
      return false;
    }
  }

  uint16_t magic = reader.read<uint16_t>();

  if (magic == 42) {
    reader.file_type = TIFF;
    reader.file_type_variant = STANDARD;
    DEBUG_PRINT("Detected TIFF");
    return true;
  } else if (magic == 0x4f52 || magic == 0x5352) {
    reader.file_type = TIFF;
    reader.file_type_variant = TIFF_ORF;
    DEBUG_PRINT("Detected TIFF/ORF");
    return true;
  } else if (magic == 0x55) {
    reader.file_type = TIFF;
    reader.file_type_variant = TIFF_RW2;
    DEBUG_PRINT("Detected TIFF/RW2");
    return true;
  }

  const int ciff_magic_offset = 6;
  const char *ciff_magic = "HEAPCCDR";
  if (std::equal(reader.data + ciff_magic_offset, reader.data + ciff_magic_offset + 8, ciff_magic)) {
    reader.file_type = CIFF;
    reader.file_type_variant = STANDARD;
    DEBUG_PRINT("Detected CIFF (CRW)");
    return true;
  }

  return false;
}

}  // namespace

std::optional<ParseError> read_exif(
  Reader &r,
  ExifData &data,
  FileType *ft,
  FileTypeVariant *ftv
);

std::optional<ParseError> find_and_parse_tiff_style_exif_segment(Reader &r, ExifData &data)
{
  DEBUG_PRINT("Searching for Exif00 marker");
  // Hard to parse for now.
  using namespace std::string_view_literals;
  std::string_view file_view{r.data, r.file_length};
  std::string_view exif_header = "Exif\0\0"sv;
  size_t offset = 0;
  while (offset != std::string_view::npos) {
    offset = file_view.find(exif_header, offset);
    if (offset == std::string_view::npos) {
      break;
    }
    std::string_view exif_header_mm = "Exif\0\0MM"sv;
    std::string_view exif_header_ii = "Exif\0\0II"sv;
    if (file_view.substr(offset, 8) == exif_header_ii) {
      break;
    } else if (file_view.substr(offset, 8) == exif_header_mm) {
      break;
    }
  }
  if (offset == std::string_view::npos) {
    return ParseError{ParseError::UNKNOWN_FILE_TYPE, "Cannot find Exif marker.", nullptr};
  }

  DEBUG_PRINT("Found Exif00 marker at offset %zu", offset);
  Reader tiff_reader{r.warnings};
  tiff_reader.data = r.data + offset + 6;
  tiff_reader.file_length = r.file_length - offset - 6;
  if (auto error = read_exif(tiff_reader, data, nullptr, nullptr)) {
    return error.value();
  } else {
    return std::nullopt;
  }
}

std::optional<ParseError> read_exif(
  Reader &r,
  ExifData &data,
  FileType *ft,
  FileTypeVariant *ftv
)
{
  DEBUG_PRINT("Input size: %zu\n", r.file_length);
  r.exif_data = &data;
  if (!guess_file_type(r)) {
    return PARSE_ERROR(UNKNOWN_FILE_TYPE, "Cannot determine file type.", nullptr);
  }
  if (ft)
    *ft = r.file_type;
  if (ftv)
    *ftv = r.file_type_variant;
  data.file_type = r.file_type;
  data.file_type_variant = r.file_type_variant;
  switch (data.file_type) {
    case TIFF: {
      if (auto error = tiff::read_tiff(r, data)) {
        return error.value();
      }
      return std::nullopt;
    }
    case JPEG: {
      uint32_t segment_offset = 0;
      while (segment_offset < r.file_length) {
        RETURN_IF_OPT_ERROR(r.seek(segment_offset));
        int marker = r.read_u16();
        DEBUG_PRINT("JPEG marker: %x", marker);
        if (marker == 0xFFD9 /* EOF */) {
          break;
        } else if (marker == 0xFFD8 /* SOI */) {
          segment_offset = 2;
        } else if (marker == 0xFFDA /* SOS */) {
          // The start-of-scan segment length is only the header.
          // It lies to us about the actual size of the segment.
          // So let's stop reading here, assuming there are no
          // useful segments behind this point in the JPEG file.
          break;
        } else {
          uint16_t length = r.read_u16();
          if (marker == 0xFFE1 /* APP1 */) {
            Reader tiff_reader{r.warnings};
            tiff_reader.data = r.data + 12;
            tiff_reader.file_length = length - sizeof(uint16_t);
            if (auto error = read_exif(tiff_reader, data, nullptr, nullptr)) {
              return error.value();
            } else {
              return std::nullopt;
            }
          }
          segment_offset += length + sizeof(uint16_t);  // Length includes the length field.
        }
      }
      return PARSE_ERROR(CORRUPT_DATA, "APP1 marker not found", nullptr);
    }
    case FUJIFILM_RAF: {
      r.byte_order = ByteOrder::Big;
      RETURN_IF_OPT_ERROR(r.seek(0x54));
      int offset = r.read_u32() + 12;
      int length = r.read_u32();
      DEBUG_PRINT("Fujifilm IFD0 offset: %x len=%x\n", offset, length);

      Reader tiff_reader{r.warnings};
      tiff_reader.data = r.data + offset;
      tiff_reader.file_length = r.file_length - offset;
      if (auto error = read_exif(tiff_reader, data, nullptr, nullptr)) {
        return error.value();
      } else {
        return std::nullopt;
      }
    }
    case MRW: {
      r.byte_order = ByteOrder::Big;
      RETURN_IF_OPT_ERROR(r.seek(4));
      int header_len = r.read_u32();
      bool tiff_found = false;
      while (header_len > 0) {
        uint32_t pos = r.ptr;
        uint32_t tag = r.read_u32();
        uint32_t len = r.read_u32();
        DEBUG_PRINT("MRW tag=%x len=%x", tag, len);
        switch (tag) {
          case 0x505244:
            DEBUG_PRINT("MRW::PRD");
            break;
          case 0x545457:
            DEBUG_PRINT("MRW::TTW");

            Reader tiff_reader{r.warnings};
            tiff_reader.data = r.data + r.ptr;
            tiff_reader.file_length = len;
            if (auto error = read_exif(tiff_reader, data, nullptr, nullptr)) {
              return error.value();
            } else {
              tiff_found = true;
            }
        }
        header_len -= 8 + len;
        RETURN_IF_OPT_ERROR(r.seek(pos + len + 2 * sizeof(uint32_t)));
      }
      if (tiff_found) {
        return std::nullopt;
      }
    }
    case SIGMA_FOVB: {
      return find_and_parse_tiff_style_exif_segment(r, data);
    }
    default: {
      // We don't know, let's just try to find it.
      auto err = find_and_parse_tiff_style_exif_segment(r, data);
      if (!err) {
        // It worked!
        return std::nullopt;
      }
    }
  }

  return PARSE_ERROR(UNKNOWN_FILE_TYPE, "Parser not implemented", to_str(data.file_type, data.file_type_variant));
}

ParseResult<ExifData> read_exif(
  const std::filesystem::path &path,
  FileType *ft,
  FileTypeVariant *ftv
)
{
  size_t file_length;
  char *data = map_file(path, &file_length);
  ASSERT_OR_PARSE_ERROR(data != NULL, CANNOT_OPEN_FILE, "Cannot open file.", nullptr);
  ASSERT_OR_PARSE_ERROR(file_length > 100, CORRUPT_DATA, "File too small.", nullptr);

  ParseResult<ExifData> result = read_exif(data, file_length, ft, ftv);
  unmap_file(data, file_length);
  return result;
}

ParseResult<ExifData> read_exif(
  const char *buffer,
  size_t length,
  FileType *ft,
  FileTypeVariant *ftv
)
{
  ASSERT_OR_PARSE_ERROR(buffer != NULL, CANNOT_OPEN_FILE, "No buffer provided.", nullptr);
  ASSERT_OR_PARSE_ERROR(length > 100, CORRUPT_DATA, "Buffer too small.", nullptr);

  ParseResult<ExifData> result{ExifData{}};
  Reader r{result.warnings};
  r.data = buffer;
  r.file_length = length;
  if (auto error = read_exif(r, std::get<0>(result._v), ft, ftv)) {
    result._v = error.value();
  }
  return result;
}

std::vector<uint8_t> generate_exif_jpeg_binary_data(const ExifData &data)
{
  std::vector<uint8_t> result;
  result.reserve(1024 * 8);

  // APP1 marker
  result.push_back(0xff);
  result.push_back(0xe1);

  // Length placeholder!
  result.push_back(0);
  result.push_back(0);

  result.push_back('E');
  result.push_back('x');
  result.push_back('i');
  result.push_back('f');
  result.push_back(0);
  result.push_back(0);

  Writer w(result);
  w.tiff_base_offset = w.pos;
  size_t size = tiff::write_tiff(w, data);

  // Fill in the size in the placeholder
  size += 8;  // size + Exif00
  result[2] = (size & 0xff00) >> 8;
  result[3] = (size & 0x00ff);

  return result;
}

};  // namespace nexif
