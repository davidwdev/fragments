
**Palette Generator**

A command line tool that takes one or more input image(s) and generates a unified palette of a requested size (must be a power of 2) using the 'median cut' algorithm. If any transparent pixels are detected, these are mapped to a special (magenta) index 0 value.

The stb_image library is used to load images.

The palette is written to disk in the .hex format. A simple format - newline separated 6 digit hex values in ASCII.

Usage:

```
 palgen.exe [-?] [-count=#] [-transp] <image>[...] -o <palette>

  -?                This help.
  -count=#          Set the palette size (power of 2). [Default=256]
  -transp           Always make index 0 transparent.

  <image>           Source image(s), wildcards supported.

  -o <palette>      Filename of output palette.

```

---

Example 1:

> palgen -o leaf.hex -count=4 -transp leaf.png

Source Image:

![leaf](example/leaf.png?raw=true "Leaf Photo")

Generated Palette:

![palette](example/leaf.hex.png?raw=true "Leaf Palette")

---

Example 2:

> palgen -o mountain.hex -count=16 mountain.png

Source Image:

![leaf](example/mountain.png?raw=true "Leaf Photo")

Generated Palette:

![palette](example/mountain.hex.png?raw=true "Leaf Palette")
