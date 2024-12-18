#include <cstdio>
#include <iostream>

#include "neonexif.hpp"

std::ostream &operator<<(std::ostream &o, const nexif::rational64u &r)
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
    return o << d.data();
  }
  return o << "(None)";
}

std::ostream &operator<<(std::ostream &o, nexif::Orientation ori)
{
  return o << to_str(ori);
}

#define print_tag(data, tag)                                                                       \
  {                                                                                                \
    std::printf(" -> \033[%dm%04x\033[0m " #tag, data.tag.is_set ? 32 : 31, data.tag.parsed_from); \
    if (data.tag.is_set) {                                                                         \
      std::cout << " = " << data.tag.value;                                                        \
    } else {                                                                                       \
      std::cout << " \033[2m(not set)\033[0m";                                                     \
    }                                                                                              \
    std::cout << "\n";                                                                             \
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
  if (argc == 2) {
    std::printf("Reading %s\n", argv[1]);
    auto result = nexif::read_exif(argv[1]);
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
        if (warn.reason) {
          std::printf("  ^>  Reason : %s\n", warn.reason);
        }
      }
      std::printf("File type: %s\n", nexif::to_str(exif.file_type));
      print_tag(exif, copyright);
      print_tag(exif, artist);
      print_tag(exif, make);
      print_tag(exif, model);
      print_tag(exif, software);

      print_tag(exif.exif, exposure_time);
      print_tag(exif.exif, f_number);
      print_tag(exif.exif, iso);
      print_tag(exif.exif, exposure_program);
      print_tag(exif.exif, focal_length);
      print_tag(exif.exif, aperture_value);

      print_tag(exif.exif, exif_version);

      for (int image_idx = 0; image_idx < exif.num_images; ++image_idx) {
        std::printf("Image #%d:\n", image_idx);
        print_image(exif.images[image_idx]);
      }
    } else {
      nexif::ParseError error = result.error();
      std::printf("Error code: %d\nMessage: %s\n", error.code, error.message);
      return 1;
    }
  } else {
    printf("Usage: %s <filename>\n", argv[0]);
    return 1;
  }
  return 0;
}
