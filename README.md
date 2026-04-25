# 🖼️ NeonEXIF

Free and open-source, lightweight and fast, EXIF parser and writer.
Made with the [Handmade philosophy](https://handmade.network/manifesto).

## 🎯 Goals

 1. Sub-millisecond metadata parsing from digital photo (RAW and JPEG) files, from standard Tiff Exif tags and various MakerNote IFDs.
 2. Normalize MakerNote information into standardized Exif tags where possible. Provide unaltered vendor-specific MakerNotes as well.
 3. Write *basic information* in standard Tiff Exif tags.
 4. Write Exif info to JPEG files.


Currently the parsing-support status looks like this:

| File Type                          | Parsing   | MakerNote   |
| :--------------------------------- | :-------- | :---------- |
| .NEF, .ARW, .CR2, .DNG (TIFF)      | ✅        |             |
| .NEF (Nikon)                       | ✅        | 🟧          |
| .RAF (Fujifilm)                    | ✅        | ❌          |
| .MRW (Standard, Minolta)           | 🟧        | ❌          |
| .MRW (Non-standard, Minolta)       | ❌        | ❌          |
| .MDC (Minolta)                     | ❌        | ❌          |
| .X3F (Exif-containing FOVb, Sigma) | 🟧🐢      | ❌          |
| .RAW (Kodak)                       | ❌        | ❌          |
| .CRW (Canon CIFF)                  | ❌        | ❌          |
| .JPG (JPEG)                        | ✅        |             | 
| .WEBP (WebP)                       | ❌        |             | 
| .PNG (Portable Network Graphics)   | ❌        |             | 

Legend: ✅ Supported; 🟧 Partially Supported; ❌ Unsupported; 🐢 Slow.

## ⚖️ License

MIT License.

## 🤝 Contribute

Contributions are welcome! Open a PR and we can work on merging your efforts!

Possible contributions:

 - PNG parsing.
 - WebP parsing.
