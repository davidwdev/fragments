
**Palette Fog Generator**

A command line tool that takes an input palette in .hex format, generates multiple 'steps' of the palette with increasing amounts of fog applied, then writes the whole lot out to a new .hex format file. You have the option to make the final step equal to the fog colour, but you may not need that so it's made into an option.

.hex palettes are a simple format - newline separated 6 digit hex values in ASCII. I use aseprite to load/edit/save them.

Usage:

```
 fogpal.exe [-?] [-final] -col=RRGGBB -steps=# -i <palette> <output>

  -?                This help.
  -final            Make the last line equal to the fog colour.
  -col=RRGGBB       The fog colour. The last level will equal this.
  -steps=#          Set the number of fog levels to generate.

  -i <file>         Filename of input palette.

  <output>          Filename of output palette.
```

Example:

> fogpal -col=808080 -steps=12 -final -i ega.hex ega_fog.hex

Original:

![EGA colour palette](example/ega.png?raw=true "Palette")

Palette with Fog Applied:

![EGA colour palette with fog](example/ega_fog.png?raw=true "Palette + Fog")

