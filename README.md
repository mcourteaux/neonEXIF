# 🖼️ NeonEXIF

Free and open-source, lightweight and fast, EXIF parser and writer.
Made with the [Handmade philosophy](https://handmade.network/manifesto).

## 🎯 Goals

 1. Sub-millisecond metadata parsing from digital photo (RAW and JPEG) files, from standard Tiff Exif tags and various MakerNote IFDs.
 2. Normalize MakerNote information into standardized Exif tags where possible.
 3. Write *basic information* in standard Tiff Exif tags.
 4. Write Exif info to JPEG files.


Currently the parsing-support status looks like this:

| File Type                          | Parsing   | MakerNote   |
| :--------------------------------- | :-------- | :---------- |
| .JPG (JPEG)                        | ✅        |             | 
| .WEBP (WebP)                       | ❌        |             | 
| .PNG (Portable Network Graphics)   | ❌        |             | 
| .NEF, .ARW, .CR2, .DNG (TIFF)      | ✅        |             |
| .RAF (Fujifilm)                    | ✅        | ❌          |
| .MRW (Standard, Minolta)           | 🟧        | ❌          |
| .MRW (Non-standard, Minolta)       | ❌        | ❌          |
| .MDC (Minolta)                     | ❌        | ❌          |
| .X3F (Exif-containing FOVb, Sigma) | 🟧🐢      | ❌          |
| .NEF (Nikon)                       | ❌        | ❌          |
| .RAW (Kodak)                       | ❌        | ❌          |
| .CRW (Canon CIFF)                  | ❌        | ❌          |

Legend: ✅ Supported; 🟧 Partially Supported; ❌ Unsupported; 🐢 Slow.

Support for maker notes:
 - [ ] Fujifilm makernotes
 - [ ] Nikon makernotes
 ...

## ⚖️ License

MIT License.

## 🤝 Contribute

Contributions are welcome! Open a PR and we can work on merging your efforts!

Possible contributions:

 - PNG parsing.
 - WebP parsing.
