# 🖼️ NeonEXIF

Free and open-source, lightweight and fast, EXIF parser and writer.
Made with the [Handmade philosophy](https://handmade.network/manifesto).

## 🎯 Goals

 1. Sub-millisecond metadata parsing from digital photo (RAW and JPEG) files,
    from standard Tiff Exif tags and various MakerNote IFDs.
 2. Normalize MakerNote information into standardized Exif tags where possible.
    Provide unaltered vendor-specific MakerNotes as well.
 3. Write *basic information* in *standard* Tiff Exif tags.
 4. Write Exif info to JPEG files.


Currently the parsing-support status looks like this:

| File Type                          | Parsing   | MakerNote    |
| :--------------------------------- | :-------- | :----------- |
| .DNG (Adobe TIFF)                  | ✅        | N/A          |
| .NEF (Nikon TIFF)                  | ✅        | 🟧           |
| .CR2 (Canon TIFF)                  | ✅        | 🟧           |
| .ARW (Sony TIFF)                   | ✅        | ❌ (planned) |
| .RAF (Fujifilm TIFF)               | ✅        | ❌           |
| .MRW (Standard, Minolta)           | 🟧        | ❌           |
| .MRW (Non-standard, Minolta)       | ❌        | ❌           |
| .MDC (Minolta)                     | ❌        | ❌           |
| .X3F (Exif-containing FOVb, Sigma) | 🟧🐢      | ❌           |
| .RAW (Kodak)                       | ❌        | ❌           |
| .CRW (Canon CIFF)                  | ❌        | ❌           |
| .JPG (JPEG)                        | ✅        |              | 
| .WEBP (WebP)                       | ❌        |              | 
| .PNG (Portable Network Graphics)   | ❌        |              | 

Legend: ✅ Supported; 🟧 Partially Supported; ❌ Unsupported; 🐢 Slow.

## ⚖️ License

MIT License.

I chose the MIT license on purpose to make a viable alternative for all other
GPL-licensed software. I doubted between LGPL and MIT, but went for MIT. I
would however greatly appreciate if you make any additions or fixes to this
library, you contribute them back in a form of a PR or some minimal information
in an issue.

## 🤝 Contribute

Contributions are welcome! Open a PR and we can work on merging your efforts!

Possible contributions:

 - PNG parsing.
 - WebP parsing.
 - CIFF parsing.

Programming guidelines: no malloc/free during parsing.

## Star History

<a href="https://www.star-history.com/?repos=neonexif%2Fneonexif&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=neonexif/neonexif&type=date&theme=dark&legend=top-left&sealed_token=U43ur6aDWiuuoAfwE6kdXUjlFLp0iXUistEeqzNhjDffea99C2bCP8vmjyPue959iSeo8PCALwYch3MkZ0loWLi1LAGOX7yay0o7iTigLb-HFC47o3B0c83bFot3HI9E9ShZv_czSONvEtb9YE7chD5gnObZyqy1xmIn7ql7W_scL84SvCln_V_tsQ30" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=neonexif/neonexif&type=date&legend=top-left&sealed_token=U43ur6aDWiuuoAfwE6kdXUjlFLp0iXUistEeqzNhjDffea99C2bCP8vmjyPue959iSeo8PCALwYch3MkZ0loWLi1LAGOX7yay0o7iTigLb-HFC47o3B0c83bFot3HI9E9ShZv_czSONvEtb9YE7chD5gnObZyqy1xmIn7ql7W_scL84SvCln_V_tsQ30" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=neonexif/neonexif&type=date&legend=top-left&sealed_token=U43ur6aDWiuuoAfwE6kdXUjlFLp0iXUistEeqzNhjDffea99C2bCP8vmjyPue959iSeo8PCALwYch3MkZ0loWLi1LAGOX7yay0o7iTigLb-HFC47o3B0c83bFot3HI9E9ShZv_czSONvEtb9YE7chD5gnObZyqy1xmIn7ql7W_scL84SvCln_V_tsQ30" />
 </picture>
</a>
