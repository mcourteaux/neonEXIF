cmake_minimum_required(VERSION 3.21)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(NeonEXIF
  LANGUAGES CXX
  VERSION 0.1
  DESCRIPTION "Free Open-Source EXIF Reader and Writer"
)

set(CMAKE_CXX_STANDARD 23)

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

add_library(neonexif STATIC
  "src/neonexif.cpp"
  "src/mappedfile.cpp"
  "src/tiff.cpp"
  "src/nikon.cpp"
)
target_include_directories(neonexif PUBLIC "include/")
target_include_directories(neonexif PRIVATE "src/")

enable_testing()
add_subdirectory("test/")
