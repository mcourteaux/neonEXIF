cmake_minimum_required(VERSION 3.12)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(NeonEXIF
  LANGUAGES CXX C
  VERSION 0.1
  DESCRIPTION "Free Open-Source EXIF Reader and Writer"
)

set(CMAKE_CXX_STANDARD 23)

add_library(neonexif SHARED
  "src/neonexif.cpp"
  "src/mappedfile.c"
  "src/tiff.cpp"
)
target_include_directories(neonexif PUBLIC "include/")
target_include_directories(neonexif PRIVATE "src/")

enable_testing()
add_subdirectory("test/")
