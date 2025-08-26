#include "neonexif.hpp"
#include "sample_exif_data.hpp"

int main(int argc, char **argv)
{
  nexif::ExifData data = generate_sample_exif_data();

  std::vector<uint8_t> binary = nexif::generate_exif_jpeg_binary_data(data);
  constexpr size_t line_size = 16;
  for (size_t l = 0; l < (binary.size() + line_size - 1) / line_size; ++l) {
    size_t n = std::min(binary.size() - l * line_size, size_t(line_size));
    for (int i = 0; i < n; ++i) {
      printf(" %02x", binary[l * line_size + i]);
    }
    for (int i = n; i < line_size; ++i) {
      printf("   ");
    }
    printf(" | ");

    for (int i = 0; i < n; ++i) {
      uint8_t v = binary[l * line_size + i];
      if (std::isprint(v)) {
        printf("%c", v);
      } else {
        printf("\033[2m·\033[0m");
      }
    }
    printf("\n");
  }
}
