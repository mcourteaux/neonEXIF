#include "neonexif/neonexif.hpp"
#include "sample_exif_data.hpp"
#include <fstream>

void usage(const char *p)
{
  printf("Usage: %s <input.jpg> <output.jpg>\n", p);
}

int main(int argc, char **argv)
{
  if (argc != 3) {
    usage(argv[0]);
    return 1;
  }

  std::ifstream in(argv[1], std::ios::binary);
  std::vector<uint8_t> file{
    std::istreambuf_iterator<char>(in),
    std::istreambuf_iterator<char>()
  };
  in.close();

  nexif::enable_debug_print = true;
  nexif::ExifData exif_data = generate_sample_exif_data();
  std::vector<uint8_t> exif_binary = nexif::generate_exif_jpeg_binary_data(exif_data);

  file.insert(file.begin() + 2, exif_binary.begin(), exif_binary.end());
  std::ofstream out(argv[2], std::ios::binary);
  out.write((const char*)file.data(), file.size());
  out.close();
}
