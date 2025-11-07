#include "neonexif/neonexif.hpp"
#include "neonexif/reader.hpp"

#include <cstring>
#include <cassert>
#include <optional>

namespace nexif {

bool debug_print_enabled = false;


namespace tiff {
std::optional<ParseError> read_tiff(Reader &r, ExifData &data);
size_t write_tiff(Writer &w, const ExifData &data);
};  // namespace tiff

namespace {

bool guess_file_type(Reader &reader)
{
  const char *data = reader.data;
  bool byte_order_indicator_found = false;
  if (data[0] == 'I' && data[1] == 'I') {
    reader.byte_order = Intel;
    DEBUG_PRINT("Byte order Intel");
    byte_order_indicator_found = true;
  } else if (data[0] == 'M' && data[1] == 'M') {
    reader.byte_order = Minolta;
    DEBUG_PRINT("Byte order Minolta");
    byte_order_indicator_found = true;
  }
  reader.skip(2);

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

std::optional<ParseError> read_exif(Reader &r, ExifData &data)
{
  DEBUG_PRINT("MMap size: %zu\n", r.file_length);
  r.exif_data = &data;
  if (!guess_file_type(r)) {
    return PARSE_ERROR(UNKNOWN_FILE_TYPE, "Cannot determine file type.", nullptr);
  }
  data.file_type = r.file_type;
  data.file_type_variant = r.file_type_variant;
  switch (data.file_type) {
    case TIFF: {
      if (auto error = tiff::read_tiff(r, data)) {
        return error.value();
      }
    } break;
    default: {
      return PARSE_ERROR(UNKNOWN_FILE_TYPE, "Parser not implemented", to_str(data.file_type));
    }
  }

  return std::nullopt;
}

ParseResult<ExifData> read_exif(const std::filesystem::path &path)
{
  size_t file_length;
  char *data = map_file(path, &file_length);
  ASSERT_OR_PARSE_ERROR(data != NULL, CANNOT_OPEN_FILE, "Cannot open file.", nullptr);
  ASSERT_OR_PARSE_ERROR(file_length > 100, CORRUPT_DATA, "File too small.", nullptr);

  ParseResult<ExifData> result = read_exif(data, file_length);
  unmap_file(data, file_length);
  return result;
}

ParseResult<ExifData> read_exif(const char *buffer, size_t length)
{
  ASSERT_OR_PARSE_ERROR(buffer != NULL, CANNOT_OPEN_FILE, "No buffer provided.", nullptr);
  ASSERT_OR_PARSE_ERROR(length > 100, CORRUPT_DATA, "Buffer too small.", nullptr);

  ParseResult<ExifData> result{ExifData{}};
  Reader r{result.warnings};
  r.data = buffer;
  r.file_length = length;
  if (auto error = read_exif(r, std::get<0>(result._v))) {
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
