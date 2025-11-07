#include <cstdio>
#include <iostream>
#include <chrono>

#include "neonexif/neonexif.hpp"

#define PRINT_MARK(_tag) std::printf("%s ", data._tag.is_set ? "\033[32mX\033[0m" : "\033[31m_\033[0m");

void print_mark_str(const nexif::Tag<nexif::CharData> &tag, int w)
{
  if (tag.is_set) {
    size_t s = std::min(int(tag.value.length), w);
    while (s > 0 && tag.value.data() + s - 1 == 0) {
      s--;
    }
    std::printf(" \033[32m%-*.*s\033[0m", w, s, tag.value.data());
  } else {
    std::printf(" \033[31m%-*s\033[0m", w, "(null)");
  }
}

void print_col_header(int &col, int w, const char *title)
{
  static std::vector<int> offsets;
  size_t oi = 0;
  for (int i = 0; i < col; ++i) {
    if (oi < offsets.size()) {
      if (offsets[oi] == i) {
        putchar('|');
        oi++;
      } else {
        putchar(' ');
      }
    } else {
      putchar(' ');
    }
  }
  std::printf(".- %s\n", title);
  offsets.push_back(col);
  col += w;
}

int main(int argc, char **argv)
{
  nexif::enable_debug_print = getenv("NEONEXIF_DEBUG");
  if (argc != 2) {
    std::printf("Usage: %s <dir>\n", argv[0]);
    return 1;
  }
  std::printf("Walking %s ...\n\n", argv[1]);

  int col = 0;
  print_col_header(col, 2, "exif version");
  print_col_header(col, 2, "f-number");
  print_col_header(col, 2, "focal length");
  print_col_header(col, 2, "iso");
  print_col_header(col, 2, "date-time original");
  col += 2;
  print_col_header(col, 2, "artist");
  print_col_header(col, 2, "copyright");
  print_col_header(col, 2, "software");
  col += 2;
  print_col_header(col, 2, "color_matrix_1");
  print_col_header(col, 2, "color_matrix_2");
  print_col_header(col, 2, "calibration_illuminant_1");
  print_col_header(col, 2, "calibration_illuminant_2");
  print_col_header(col, 2, "as_shot_neutral");
  print_col_header(col, 2, "as_shot_white_xy");
  print_col_header(col, 2, "analog_balance");

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
    if (file.filename() == "silvernode.thumb_db") {
      continue;
    }
    std::string ext = file.extension().generic_string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](uint8_t c) { return std::tolower(c); });
    if (ext == ".exe" || ext == ".zip" || ext == ".7z" || ext == ".exe") {
      continue;
    }
    std::filesystem::path relpath = file.lexically_relative(dir);

    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = nexif::read_exif(file);
    auto t1 = std::chrono::high_resolution_clock::now();

    if (!result) {
      auto err = result.error();
      std::printf("%s: Error %s: %s %s\n", relpath.c_str(), nexif::to_str(err.code), err.message, err.what);
      continue;
    }
    auto data = result.value();

    PRINT_MARK(exif.exif_version);
    PRINT_MARK(exif.f_number);
    PRINT_MARK(exif.focal_length);
    PRINT_MARK(exif.iso);
    PRINT_MARK(exif.date_time_original);

    std::printf("| ");
    PRINT_MARK(artist);
    PRINT_MARK(copyright);
    PRINT_MARK(software);
    std::printf("| ");
    PRINT_MARK(color_matrix_1);
    PRINT_MARK(color_matrix_2);
    PRINT_MARK(calibration_illuminant_1);
    PRINT_MARK(calibration_illuminant_2);
    PRINT_MARK(as_shot_neutral);
    PRINT_MARK(as_shot_white_xy);
    PRINT_MARK(analog_balance);

    std::printf(" |  ");

    print_mark_str(data.make, 12);
    print_mark_str(data.model, 20);
    print_mark_str(data.exif.lens_make, 20);
    print_mark_str(data.exif.lens_model, 20);

    printf("  %s\n", relpath.c_str());
  }
}
