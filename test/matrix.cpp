#include <cstdio>
#include <iostream>
#include <chrono>

#include "neonexif/neonexif.hpp"

#define PRINT_MARK(_tag) std::printf("%s ", data._tag.is_set ? "\033[32mX\033[0m" : "\033[31m_\033[0m");

void print_mark_str(const nexif::Tag<nexif::CharData> &tag, int w)
{
  if (tag.is_set) {
    std::printf("\033[32m%-*s\033[0m", w, tag.value.data());
  } else {
    std::printf("\033[31m%-*s\033[0m", w, tag.value.data());
  }
}

void print_col_header(int &col, int w, const char *title) {
  for (int i = 0; i < col; ++i) {
    putchar(' ');
  }
  std::printf("v %s\n", title);
  col += w;
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    std::printf("Usage: %s <dir>\n", argv[0]);
    return 1;
  }
  std::printf("Walking %s ...\n\n", argv[1]);

  int col = 100;
  print_col_header(col, 2, "f-number");
  print_col_header(col, 2, "focal length");
  print_col_header(col, 2, "iso");

  std::filesystem::path dir{argv[1]};
  std::filesystem::recursive_directory_iterator end;
  for (std::filesystem::recursive_directory_iterator it{dir}; it != end; ++it) {
    std::filesystem::path file = *it;
    if (std::filesystem::status(file).type() != std::filesystem::file_type::regular) {
      continue;
    }
    size_t fs = std::filesystem::file_size(file);
    if (fs < 20'000) {
      continue;
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = nexif::read_exif(file);
    auto t1 = std::chrono::high_resolution_clock::now();

    if (!result) {
      auto err = result.error();
      std::printf("Error %s: %s %s\n", nexif::to_str(err.code), err.message, err.what);
      continue;
    }
    auto data = result.value();

    print_mark_str(data.make, 20);
    print_mark_str(data.model, 20);
    print_mark_str(data.exif.lens_make, 20);
    print_mark_str(data.exif.lens_model, 40);

    PRINT_MARK(exif.f_number);
    PRINT_MARK(exif.focal_length);
    PRINT_MARK(exif.iso);

    printf("  %s\n", file.lexically_relative(dir).c_str());
  }
}
