
**Palette Fog Generator**

A command line tool that takes an input palette in .hex format, generates multiple 'steps' of the palette with increasing amounts of fog applied, then writes the whole lot out to a new .hex format file. You have the option to make the final step equal to the fog colour, but you may not need that so it's made into an option.

.hex palettes are a simple format - newline separated 6 digit hex values in ASCII. I use aseprite to load/edit/save them.

Usage:

```
 fogpal.exe [-?] -col=RRGGBB [-final] -steps=# [-split] [-remap|-remap-lab] -i <palette> <output>

  -?                This help.
  -col=RRGGBB       The fog colour.
  -final            Make the last line equal to the fog colour.
  -steps=#          Set the number of fog levels to generate.
  -split            Write each fog level to a separate file.
  -remap            Map fog outputs back to original palette.
  -remap-lab        Use Lab color space for remapping.

  -i <file>         Filename of input palette.

  <output>          Filename of output palette.
```

Example:

> fogpal -col=808080 -steps=12 -final -i ega.hex ega_fog.hex


Palette:

![EGA colour palette](example/ega.png?raw=true "Palette")

Palette with Fog:

![EGA colour palette with fog](example/ega_fog.png?raw=true "Palette + Fog")

---

## Support Development

All support is greatly appreciated and encourages me to develop more open source projects!

➤ ☕ Buy me a Coffee: https://ko-fi.com/davidwdev

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/B0B458231)

➤ |<sup>●</sup> Back me on︎ Patreon: https://www.patreon.com/davidwdev

[![Patreon](../patreon.svg?raw=true)](https://www.patreon.com/davidwdev)

