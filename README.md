# NeonEXIF

Free and open-source EXIF parser and writer.

## Goal

The goal is twofold, and is far from achieved:
 1. load *basic information* from digital photo (RAW and JPEG) files, from standard Tiff Exif tags and various MakerNote IFDs.
 2. write *basic information* in standard Tiff Exif tags.


Currently the parsing-support status looks like this:
 - [x] JPEG files
 - [x] Tiff files (.NEF, .ARW, .CR2, ...)
 - [x] DNG files
 - [x] Fujifilm RAF files
 - [x] Standard MRW files
 - [ ] Non-standard *.MRW files
 - [ ] Minolta *.MDC files
 - [x] EXIF-containing Sigma FOVb files
 - [ ] Custom Nikon *.NEF files
 - [ ] Custom Kodak *.RAW files
 - [ ] Canon CIFF files

Support for maker notes:
 - [ ] Fujifilm makernotes
 - [ ] Nikon makernotes
 ...

## License

MIT License.

## Contribute

Contributions are welcome! Open a PR and we can work on merging your efforts!
