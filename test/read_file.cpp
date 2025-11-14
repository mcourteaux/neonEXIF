#include <cstdio>
#include <iostream>
#include <chrono>

#include "neonexif/neonexif.hpp"

std::ostream &operator<<(std::ostream &o, const nexif::rational64u &r)
{
  o << r.num << "/" << r.denom;
  if (r.denom != 0) {
    o << " = " << (double(r.num) / r.denom);
  }
  return o;
}
std::ostream &operator<<(std::ostream &o, const nexif::rational64s &r)
{
  o << r.num << "/" << r.denom;
  if (r.denom != 0) {
    o << " = " << (double(r.num) / r.denom);
  }
  return o;
}

std::ostream &operator<<(std::ostream &o, const nexif::CharData &d)
{
  if (d.ptr_offset) {
    return o << "\"\033[32m" << d.data() << "\033[0m\"";
  }
  return o << "(None)";
}

std::ostream &operator<<(std::ostream &o, const nexif::DateTime &d)
{
  char buffer[64];
  std::snprintf(
    buffer, sizeof(buffer),
    "%d-%02d-%02d %d:%02d:%02d,%03d",
    d.year, d.month, d.day, d.hour, d.minute, d.second, d.millis
  );
  o << buffer;
  return o;
}

std::ostream &operator<<(std::ostream &o, nexif::Orientation ori)
{
  return o << to_str(ori);
}

std::ostream &operator<<(std::ostream &o, nexif::Illuminant illum)
{
  return o << to_str(illum);
}

template <typename T, uint8_t C>
std::ostream &operator<<(std::ostream &o, const nexif::vla<T, C> &array)
{
  for (int i = 0; i < array.num; ++i) {
    if (i > 0) {
      o << ",  ";
    }
    o << array.values[i];
  }
  o << "  (" << (int)array.num << "elems/" << (int)C << "cap)";
  return o;
}

template <typename T, size_t C>
std::ostream &operator<<(std::ostream &o, const std::array<T, C> &array)
{
  for (size_t i = 0; i < C; ++i) {
    if (i > 0) {
      o << ",  ";
    }
    o << array[i];
  }
  return o;
}

#define print_tag(data, tag)                             \
  {                                                      \
    std::printf(                                         \
      " -> \033[%dm%04x\033[0m \033[33m" #tag "\033[0m", \
      data.tag.is_set ? 32 : 31, data.tag.parsed_from    \
    );                                                   \
    if (data.tag.is_set) {                               \
      std::cout << " = " << data.tag.value;              \
    } else {                                             \
      std::cout << " \033[2m(not set)\033[0m";           \
    }                                                    \
    std::cout << "\n";                                   \
  }

void print_image(const nexif::ImageData &id)
{
  std::printf("Type: %s\n", nexif::to_str(id.type));
  print_tag(id, image_width);
  print_tag(id, image_height);
  print_tag(id, compression);
  print_tag(id, photometric_interpretation);
  print_tag(id, orientation);
  print_tag(id, samples_per_pixel);
  print_tag(id, x_resolution);
  print_tag(id, y_resolution);
  print_tag(id, resolution_unit);
  print_tag(id, data_offset);
  print_tag(id, data_length);
}

int main(int argc, char **argv)
{
  nexif::enable_debug_print = true;
  if (argc == 2) {
    std::printf("Reading %s\n", argv[1]);
    auto t0 = std::chrono::high_resolution_clock::now();
    auto result = nexif::read_exif(std::filesystem::path(argv[1], std::filesystem::path::generic_format));
    auto t1 = std::chrono::high_resolution_clock::now();
    if (result) {
      const nexif::ExifData &exif = result.value();
      std::printf("Parse successful.\n");
      if (result.warnings.empty()) {
        std::printf("No Warnings.\n");
      } else {
        std::printf("%zu Warnings:\n", result.warnings.size());
      }
      for (auto warn : result.warnings) {
        std::printf("  Message: %s\n", warn.msg);
        if (warn.what) {
          std::printf("  ^>  What   : %s\n", warn.what);
        }
      }
      std::printf("File type: %s\n", nexif::to_str(exif.file_type));
      print_tag(exif, date_time);
      print_tag(exif, copyright);
      print_tag(exif, artist);
      print_tag(exif, make);
      print_tag(exif, model);
      print_tag(exif, software);

      print_tag(exif, color_matrix_1);
      print_tag(exif, color_matrix_2);
      print_tag(exif, calibration_matrix_1);
      print_tag(exif, calibration_matrix_2);
      print_tag(exif, calibration_illuminant_1);
      print_tag(exif, calibration_illuminant_2);
      print_tag(exif, as_shot_neutral);
      print_tag(exif, as_shot_white_xy);
      print_tag(exif, analog_balance);
      print_tag(exif, apex_aperture_value);
      print_tag(exif, apex_shutter_speed_value);

      std::printf("EXIF:\n");
      print_tag(exif.exif, exposure_time);
      print_tag(exif.exif, f_number);
      print_tag(exif.exif, iso);
      print_tag(exif.exif, exposure_program);
      print_tag(exif.exif, focal_length);
      print_tag(exif.exif, date_time_original);
      print_tag(exif.exif, date_time_digitized);

      print_tag(exif.exif, exif_version);
      print_tag(exif.exif, camera_owner_name);
      print_tag(exif.exif, body_serial_number);
      print_tag(exif.exif, lens_specification);
      print_tag(exif.exif, lens_make);
      print_tag(exif.exif, lens_model);
      print_tag(exif.exif, lens_serial_number);
      print_tag(exif.exif, image_title);
      print_tag(exif.exif, photographer);
      print_tag(exif.exif, image_editor);
      print_tag(exif.exif, raw_developing_software);
      print_tag(exif.exif, image_editing_software);
      print_tag(exif.exif, metadata_editing_software);

      for (int image_idx = 0; image_idx < exif.num_images; ++image_idx) {
        std::printf("Image #%d:\n", image_idx);
        print_image(exif.images[image_idx]);
      }
    } else {
      nexif::ParseError error = result.error();
      std::printf("Error code: %d\nMessage: %s\n", error.code, error.message);
      if (error.what) {
        std::printf("What: %s\n", error.what);
      }
      return 1;
    }
    double ms = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() * 1e-6;
    std::printf("Time elapsed: %f ms\n", ms);
  } else {
    printf("Usage: %s <filename>\n", argv[0]);
    return 1;
  }
  return 0;
}
