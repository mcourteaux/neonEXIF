#include "neonexif/neonexif.hpp"

static nexif::ExifData generate_sample_exif_data()
{
  nexif::ExifData data;
  auto str_neonexif = data.store_string_data("NeonEXIF");
  auto str_martijn = data.store_string_data("Martijn Courteaux");
  auto str_neonraw = data.store_string_data("NeonRAW");
  auto str_silvernode = data.store_string_data("SilverNode");

  data.software = data.store_string_data("Firmware123.89");
  data.processing_software = str_neonexif;
  data.artist = str_martijn;
  data.copyright = data.store_string_data("© Zero Effort 2025");
  data.make = data.store_string_data("Nikon", 5);
  data.model = data.store_string_data("D750", 4);
  data.date_time = nexif::DateTime(2025, 8, 26, 10, 00, 00);
  data.exif.date_time_original = nexif::DateTime(2025, 7, 18, 12, 10, 22);
  data.exif.exposure_time = nexif::rational64u{1, 400};
  data.exif.iso = 1600;
  data.exif.f_number = nexif::rational64u{28, 10};             // f/2.8
  data.apex_aperture_value = nexif::rational64s{43, 10};       // EV=-4.3
  data.apex_shutter_speed_value = nexif::rational64s{24, 10};  // EV=2.4
  data.exif.metadata_editing_software = str_neonexif;
  data.exif.raw_developing_software = str_neonraw;
  data.exif.image_editing_software = str_silvernode;
  return data;
}
