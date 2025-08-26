add_executable(read_file "read_file.cpp")
target_link_libraries(read_file PUBLIC
  neonexif
)

set(TEST_PHOTO_REPO "http://www.rawsamples.ch/raws/")
set(PHOTOS_DIR "${PROJECT_SOURCE_DIR}/test_files")
file(MAKE_DIRECTORY "${PHOTOS_DIR}")
set(RAW_FILES "")

macro(download brand path name)
  set(DEST_FILE "${PHOTOS_DIR}/${brand}/${name}")
  list(APPEND RAW_FILES "${DEST_FILE}")
  if (NOT EXISTS "${DEST_FILE}")
    file(MAKE_DIRECTORY "${PHOTOS_DIR}/${brand}")
    set(URL "${TEST_PHOTO_REPO}/${brand}/${path}/${name}")
    message(STATUS "Downloading ${URL} -> ${brand}/${name}...")
    file(DOWNLOAD "${URL}" "${DEST_FILE}")
  endif()
endmacro(download)

# Example path
# http://www.rawsamples.ch/raws/nikon/RAW_NIKON_D750.NEF
download(nikon "" "RAW_NIKON_D750.NEF")
download(nikon "" "RAW_NIKON_D3300.NEF")
download(nikon "" "RAW_NIKON_D3100.NEF")
download(nikon "d90" "RAW_NIKON_D90.NEF")
download(nikon "d40" "RAW_NIKON_D40_SRGB.NEF")

#download(canon "d60" "RAW_CANON_D60_ARGB.CRW")
download(canon "" "RAW_CANON_EOS_700D.CR2")
download(canon "" "RAW_CANON_EOS_5DS.CR2")

download(panasonic "" "RAW_PANASONIC_DMC_FZ1000.RW2")

foreach(RAW_FILE IN LISTS RAW_FILES)
  get_filename_component(BASENAME "${RAW_FILE}" NAME)
  message(STATUS "Add test: ${BASENAME}")
  add_test(NAME read_file_${BASENAME}
    COMMAND read_file ${RAW_FILE}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endforeach()

add_executable(generate_exif "generate_exif.cpp")
target_link_libraries(generate_exif PUBLIC neonexif)
add_test(NAME generate_exif COMMAND generate_exif)


add_executable(add_exif_to_jpeg "add_exif_to_jpeg.cpp")
target_link_libraries(add_exif_to_jpeg PUBLIC neonexif)
#add_test(NAME add_exif_to_jpeg COMMAND add_exif_to_jpeg)
