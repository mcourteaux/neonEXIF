#pragma once

#include "neonexif.hpp"
#include "mappedfile.hpp"

#include <cstring>
#include <cassert>
#include <bit>

namespace nexif {

namespace {

int __indent = 0;
#define DEBUG_PRINT(fmt, args...) std::printf("%*s\033[2m" fmt "\033[0m\n", __indent * 4, "", ##args)
struct Indenter {
  Indenter() { __indent++; }
  ~Indenter() { __indent--; }
};

#define PARSE_ERROR(code, msg, what)      \
  ParseError                              \
  {                                       \
    ParseError::Code::code, (msg), (what) \
  }
#define ASSERT_OR_PARSE_ERROR(cond, code, msg, what) \
  if (!(cond)) {                                     \
    return PARSE_ERROR(code, (msg), (what));         \
  }

#define LOG_WARNING(reader, msg, what)                      \
  do {                                                      \
    reader.warnings.push_back(ParseWarning((msg), (what))); \
    DEBUG_PRINT("Warning: %s (what: %s)", (msg), (what));   \
  } while (false)

}  // namespace

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
      return nexif::byteswap(t);
    } else {
      return t;
    }
  }

  // clang-format off
  inline uint8_t  read_u8()  { return read<uint8_t>();  }
  inline int8_t   read_s8()  { return read<int8_t>();   }
  inline uint16_t read_u16() { return read<uint16_t>(); }
  inline int16_t  read_s16() { return read<int16_t>();  }
  inline uint32_t read_u32() { return read<uint32_t>(); }
  inline int32_t  read_s32() { return read<int32_t>();  }
  inline uint64_t read_u64() { return read<uint64_t>(); }
  inline int64_t  read_s64() { return read<int64_t>();  }
  // clang-format on

  ExifData *exif_data;
  uint32_t ifd_offset_exif{0};

  const char *store_string_data(const char *ptr, int count)
  {
    return exif_data->store_string_data(ptr, count);
  }
};

struct Writer {
  std::vector<uint8_t> &dst;
  size_t pos;
  int32_t tiff_base_offset{0};

  Writer(std::vector<uint8_t> &dst) : dst(dst), pos(dst.size()) {
  }

  int32_t current_in_tiff_pos() const {
    return pos - tiff_base_offset;
  }

  template<typename T>
  inline size_t write(T t) {
    size_t old_pos = pos;
    dst.insert(dst.begin() + old_pos, sizeof(T), 0);
    std::memcpy(&dst[old_pos], &t, sizeof(T));
    pos += sizeof(T);
    return old_pos;
  }

  template<typename T>
  inline void overwrite(size_t pos, T t) {
    assert(pos + sizeof(T) <= dst.size());
    std::memcpy(&dst[pos], &t, sizeof(T));
  }

  template<typename T>
  inline T read(size_t pos) {
    assert(pos + sizeof(T) <= dst.size());
    T t;
    std::memcpy(&t, &dst[pos], sizeof(T));
    return t;
  }

  inline size_t write_string(const char *str, uint32_t len) {
    size_t old_pos = pos;
    dst.insert(dst.begin() + old_pos, str, str + len);
    pos += len;
    return old_pos;
  }

  inline size_t write_all(const std::vector<uint8_t> &buf) {
    size_t old_pos = pos;
    dst.insert(dst.begin() + pos, buf.begin(), buf.end());
    pos += buf.size();
    return old_pos;
  }

  // clang-format off
  inline size_t write_u8 (uint8_t v)  { return write(v); }
  inline size_t write_s8 (int8_t v)   { return write(v); }
  inline size_t write_u16(uint16_t v) { return write(v); }
  inline size_t write_s16(int16_t v)  { return write(v); }
  inline size_t write_u32(uint32_t v) { return write(v); }
  inline size_t write_s32(int32_t v)  { return write(v); }
  inline size_t write_u64(uint64_t v) { return write(v); }
  inline size_t write_s64(int64_t v)  { return write(v); }

  inline void overwrite_u8 (size_t p, uint8_t v)  { overwrite(p, v); }
  inline void overwrite_s8 (size_t p, int8_t v)   { overwrite(p, v); }
  inline void overwrite_u16(size_t p, uint16_t v) { overwrite(p, v); }
  inline void overwrite_s16(size_t p, int16_t v)  { overwrite(p, v); }
  inline void overwrite_u32(size_t p, uint32_t v) { overwrite(p, v); }
  inline void overwrite_s32(size_t p, int32_t v)  { overwrite(p, v); }
  inline void overwrite_u64(size_t p, uint64_t v) { overwrite(p, v); }
  inline void overwrite_s64(size_t p, int64_t v)  { overwrite(p, v); }

  inline uint8_t  read_u8 (size_t p) { return read<uint8_t> (p);  }
  inline int8_t   read_s8 (size_t p) { return read<int8_t>  (p);   }
  inline uint16_t read_u16(size_t p) { return read<uint16_t>(p); }
  inline int16_t  read_s16(size_t p) { return read<int16_t> (p);  }
  inline uint32_t read_u32(size_t p) { return read<uint32_t>(p); }
  inline int32_t  read_s32(size_t p) { return read<int32_t> (p);  }
  inline uint64_t read_u64(size_t p) { return read<uint64_t>(p); }
  inline int64_t  read_s64(size_t p) { return read<int64_t> (p);  }
  // clang-format on
};

}  // namespace nexif
