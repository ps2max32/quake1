# Quake on maxsdk (PS2)

id's GLQuake (`WinQuake/`, GPL) built for the PS2 with `libs/maxsdk` (clang, R5900) by
`Makefile`. The port is in `ps2/`; the engine is id's, with one line changed
(`mathlib.c`: no `double sqrt` declaration).

```
make [FRAMES=n] [ARGS="+map e1m1"]     $(OUT)/quake1.elf (build/apps/maxsdk_quake1)
make run                               copies the retail ID1/PAK0, PAK1 (Q1DATA) to
                                       build/pcsx2-test/id1 and runs it in PCSX2
```

`FRAMES=n` ends the program after n drawn frames with a picture (`host:quake1.tga`) and
the test end marker; `ARGS` is added to the command line. Without `ARGS` the demos play.

| Part | How |
|---|---|
| Renderer | GLQuake's on `libs/maxsdk_gl` (OpenGL 1.1 subset on the GS); its lightmaps are `GL_LUMINANCE` with blend `ZERO, ONE_MINUS_SRC_COLOR`, which maxsdk_gl draws as `Cd * As` |
| Video | `ps2/vid_ps2.c`: the TV's mode, NTSC 640 x 448 or PAL 640 x 512 (the console's), or 1080i (below); `gl_max_size 128`, trilinear; the status bar every frame |
| Files | stdio on the IOP file server (`host:`), `ps2/ps2_file.c`; `-basedir host:` |
| Input | pad 0: d-pad arrows, left stick moves, right stick turns, R1 attack (CTRL), X enter/jump, START menu, SELECT scores; O, square, triangle, R2, L1, L2 are AUX1..6 (bind them) |
| Memory | 32 MiB: hunk 12 MiB, stack 1 MiB |
| Sound | id's mixer (`snd_dma.c`, `snd_mix.c`) at 22050 Hz 16-bit stereo into a ring, sent to maxsdk's PCM stream (`mx/snd.h`, audsrv) by `ps2/snd_ps2.c` as far as the mixer has painted |
| Math | float literals (`-cl-single-precision-constant`) and float math.h (`ps2/ps2_math.h`): the FPU has no doubles |

Not done: CD audio (id's null driver), network play (loopback only).

## 1080i

Options, Video Options switches between the TV's mode and 1080i (`vid_mode 0` / `1`, not
kept in the config: a TV without 1080i would show nothing at the next start). In 1080i the
3D is drawn at 1920 x 1080 (two 540-line fields, drawn in three strips ahead of the beam
every field, maxsdk_gl's `MXGL_1080I`); the 2D (console, status bar, menus) is laid out in
640 x 360. Its memory (2 MiB) is taken at the start (`mxgl_reserve_1080i`).

On the PS2 (2026-10-31, the 1080i-only build before the switch): the demo loop at 59 to 61
fps, the `start` map at 60; no pass late. World polygons are drawn with `mxgl_DrawPolygon`
(`WinQuake/gl_rsurf.c`), alias models with `mxgl_MultiDraw`, the lightmaps buffer is a
byte a texel (GL_LUMINANCE only), and the palette goes to maxsdk_gl (`mxgl_Palette`:
8-bit textures). `make PROF=1` profiles the second half of a `FRAMES` run
(`libs/maxsdk/tools/prof.py <elf> <log>`).
