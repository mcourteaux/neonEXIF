#pragma once

#include "neonexif.hpp"
#include "mappedfile.h"

#include <cstring>

namespace nexif {

namespace {

int __indent = 0;
#define DEBUG_PRINT(fmt, args...) std::printf("%*s\033[2m" fmt "\033[0m\n", __indent * 4, "", ##args)
struct Indenter {
  Indenter() { __indent++; }
  ~Indenter() { __indent--; }
};

#define PARSE_ERROR(code, msg)    \
  ParseError                      \
  {                               \
    ParseError::Code::code, (msg) \
  }
#define ASSERT_OR_PARSE_ERROR(cond, code, msg) \
  if (!(cond)) {                               \
    return PARSE_ERROR(code, (msg));           \
  }

#define LOG_WARNING(reader, msg, reason)                  \
  do {                                                    \
    reader.warnings.push_back(ParseWarning(msg, reason)); \
    DEBUG_PRINT("Warning: %s (reason: %s)", msg, reason); \
  } while (false)

}

enum ByteOrder { Intel,
                 Minolta };


struct Reader {
  std::list<ParseWarning> &warnings;
  Reader(std::list<ParseWarning> &warnings) :
    warnings(warnings) {}

  char *data{nullptr};
  size_t file_length{0};
  ByteOrder byte_order;

  FileType file_type;
  FileTypeVariant file_type_variant;

  ~Reader()
  {
    if (data) {
      unmap_file(data, file_length);
    }
  }

  Reader(Reader &&) = delete;
  Reader(const Reader &) = delete;
  Reader &operator=(const Reader &) = delete;
  Reader &operator=(Reader &&) = delete;

  int ptr{0};

  inline void skip(int num) { ptr += num; }
  inline void seek(int offset) { ptr = offset; }

  template <typename T>
  inline T read()
  {
    const T t = *(const T *)&data[ptr];
    ptr += sizeof(T);
    if (byte_order == Minolta) {
      return std::byteswap(t);
    } else {
      return t;
    }
  }

  inline uint16_t read_u8() { return read<uint8_t>(); }
  inline int16_t read_s8() { return read<int8_t>(); }
  inline uint16_t read_u16() { return read<uint16_t>(); }
  inline int16_t read_s16() { return read<int16_t>(); }
  inline uint32_t read_u32() { return read<uint32_t>(); }
  inline int32_t read_s32() { return read<int32_t>(); }
  inline uint64_t read_u64() { return read<uint64_t>(); }
  inline int64_t read_s64() { return read<int64_t>(); }

  ExifData *exif_data;
  uint32_t ifd_offset_exif{0};

  const char *store_string_data(const char *ptr, int count)
  {
    char *dst = &exif_data->string_data[exif_data->string_data_ptr];
    std::memcpy(dst, ptr, count);
    exif_data->string_data[exif_data->string_data_ptr + count] = 0;
    exif_data->string_data_ptr += count + 1;
    return dst;
  }
};

}
