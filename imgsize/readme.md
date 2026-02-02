
**Image Resize and Palettize**

A command line tool that resizes images and can optionally apply a palette during the process to create 8-bit outputs.

.hex palettes are a simple format - newline separated 6 digit hex values in ASCII. I use aseprite to load/edit/save them.

Usage:

```

 imgsize.exe [-?] -w <width> -h <height> -aspect [-pal <palette> [-dither]] [-nearest|-bilinear] <image>[...] [-o <image>]|[-outdir <folder>]

  -?                 This help.
  
  -w                 The output width in pixels, all images use this value.
  -h                 The output height.
  -aspect            Preserve aspect ratio if either width or height is omitted.
  
  -pal <palette>     Palette file to use (in .HEX format)
  -dither            Apply error-diffusion dithering to output.

  -nearest           Use nearest-neighbor sampling.
  -bilinear          Use bilinear filtering.

  <image>[...]       Source image(s), wildcards supported.

  -o <file>          Specify an output file. Not supported with multiple images.
  -outdir <folder>   Specify an output folder. Ignored if -o is used.
```

---

## Support Development

All support is greatly appreciated and encourages me to develop more open source projects!

➤ ☕ Buy me a Coffee: https://ko-fi.com/davidwdev

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/B0B458231)

➤ |<sup>●</sup> Back me on︎ Patreon: https://www.patreon.com/davidwdev

[![Patreon](../patreon.svg?raw=true)](https://www.patreon.com/davidwdev)

