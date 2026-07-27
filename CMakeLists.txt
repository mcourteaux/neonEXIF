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

message(STATUS "Cmake System version: ${CMAKE_SYSTEM_VERSION}")
set(NEONEXIF_HAVE_FROM_CHARS_FLOAT ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_SYSTEM_VERSION VERSION_LESS "25.0")
    message(STATUS "Detected old macOS: no from_chars available")
    set(NEONEXIF_HAVE_FROM_CHARS_FLOAT OFF)
  endif()
endif()

add_library(neonexif STATIC
  "src/neonexif.cpp"
  "src/mappedfile.cpp"
  "src/tiff.cpp"
  "src/lens_name_parser.cpp"
  "src/nikon.cpp"
  "src/canon.cpp"
  "src/from_chars.cpp"
)
target_include_directories(neonexif PUBLIC "include/")
target_include_directories(neonexif PRIVATE "src/")

if (NOT NEONEXIF_HAVE_FROM_CHARS_FLOAT)
  message(STATUS "FINDMEEE")
  set_source_files_properties(src/from_chars.cpp
    PROPERTIES COMPILE_DEFINITIONS "NEXIF_NO_FROM_CHARS_FLOAT")
endif()

enable_testing()
add_subdirectory("test/")
