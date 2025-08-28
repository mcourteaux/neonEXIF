#include "neonexif/neonexif.hpp"

static nexif::ExifData generate_sample_exif_data()
{
  nexif::ExifData data;
  data.software = nexif::CharData(data.store_string_data("Firmware123.89", 14), 14);
  data.processing_software = nexif::CharData(data.store_string_data("NeonEXIF", 8), 8);
  data.artist = nexif::CharData(data.store_string_data("Martijn Courteaux", 17), 17);
  data.copyright = nexif::CharData(data.store_string_data("© Zero Effort 2025", 19), 19);
  data.make = nexif::CharData(data.store_string_data("Nikon", 5), 5);
  data.model = nexif::CharData(data.store_string_data("D750", 4), 4);
  data.date_time = nexif::CharData(data.store_string_data("2025:08:26 10:00:00", 19), 19);
  data.date_time_original = nexif::CharData(data.store_string_data("2025:07:18 12:10:22", 19), 19);
  data.exif.exposure_time = nexif::rational64u{1, 400};
  data.exif.iso = 1600;
  data.exif.f_number = nexif::rational64u{28, 10}; // f/2.8
  data.exif.aperture_value = nexif::rational64u{28, 10}; // f/2.8
  return data;
}
