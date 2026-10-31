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
| Video | `ps2/vid_ps2.c`: 640 x 448, `gl_max_size 128`, trilinear; the status bar every frame |
| Files | stdio on the IOP file server (`host:`), `ps2/ps2_file.c`; `-basedir host:` |
| Input | pad 0: d-pad arrows, left stick moves, right stick turns, R1 attack (CTRL), X enter/jump, START menu, SELECT scores; O, square, triangle, R2, L1, L2 are AUX1..6 (bind them) |
| Memory | 32 MiB: hunk 12 MiB, stack 1 MiB |
| Math | float literals (`-cl-single-precision-constant`) and float math.h (`ps2/ps2_math.h`): the FPU has no doubles |

Not done: sound and CD audio (id's null drivers), network play (loopback only).

## Branch `hires`: 1080i

This branch builds the 1920 x 1080 interlaced version (`$(OUT)` is
`build/apps/maxsdk_quake1-hires`): maxsdk_gl with `MXGL_HIRES=1`, on the maxsdk worktree of
branch `hires` (`libs/maxsdk-hires`, built into `build/libs/maxsdk-hires`). The 3D is drawn
at 1920 x 1080 (two 540-line fields, drawn in three strips ahead of the beam every field);
the 2D (console, status bar, menus) is laid out in 640 x 360.
