# Quake on maxsdk (PS2)

id's GLQuake (`WinQuake/`, GPL) built for the PS2 with `libs/maxsdk` (clang, R5900) by
`Makefile`. The port is in `ps2/`; the engine is id's, with one line changed
(`mathlib.c`: no `double sqrt` declaration).

```
make [FRAMES=n] [ARGS="+map e1m1"]     $(OUT)/quake1.elf (build/apps/quake1)
make run                               copies the retail ID1/PAK0, PAK1 (Q1DATA) to
                                       build/pcsx2-test/id1 and runs it in PCSX2
make -C pc [FRAMES=n] [ARGS=...] run   the same engine on the PC's OpenGL (below)
```

`FRAMES=n` ends the program after n drawn frames with a picture (`host:quake1.tga`) and
the test end marker; `ARGS` is added to the command line. Without `ARGS` the demos play.
`FIXEDFPS=50` makes the game's time 1/50 s a frame instead of the clock's, so the picture
at frame n does not depend on the speed (lightstyles, sky, water, animations).
`ARGS="+timedemo demo1"` ends the program when the timedemo is done, its result the last
console line (`PROF=1`: with a profile of the timedemo).

The PC build (`pc/`) is the reference for libgl: the same `WinQuake/` sources (id's GL
paths, not the `__PS2__` ones), vid_ps2.c's GL state and command line, 640 x 512, drawn
off screen through EGL (Mesa's llvmpipe), time 1/50 s a frame. Its `quake1-pc.tga` and the
PS2 build's `quake1.tga` (with `FIXEDFPS=50`) at the same `FRAMES` and `ARGS` show the
same view; what differs is libgl's.

| Part | How |
|---|---|
| Renderer | GLQuake's on `libs/libgl` (OpenGL 1.1 subset on the GS); its lightmaps are `GL_LUMINANCE` with blend `ZERO, ONE_MINUS_SRC_COLOR`, which libgl draws as `Cd * As`; `gl_keeptjunctions 1` (id's 0 drops the points on the polygons' edges: the T-junctions left showed as red dots and stripes, the clear colour, along the edges) |
| Video | `ps2/vid_ps2.c`: the TV's mode, NTSC 640 x 448 or PAL 640 x 512 (the console's), or 1080i, with or without beam racing, 16 or 24-bit colour (below); `gl_max_size 256`, trilinear, the textures' top levels left out only while GS memory overflows or the swap file is read (`gl_ps2texquality -1`, libgl's `MXGL_TEX_AUTO`; 0, 1, 2: that many levels always; `gl_ps2texlimits 120,80,30`); dynamic lights in the lightmaps (`gl_flashblend 0`); the status bar every frame |
| Files | stdio on the IOP file server (`host:`), `ps2/ps2_file.c`; `-basedir host:` |
| Input | pad 0: d-pad arrows, left stick moves, right stick turns, R1 attack (CTRL), X enter/jump, START menu, L3 + R3 (both sticks pressed) the counters on/off, SELECT scores; O, square, triangle, R2, L1, L2 are AUX1..6 (bind them) |
| Memory | maxsdk's virtual memory (`mx_vm_config` in `ps2/sys_ps2.c`): `malloc` in up to 128 MiB, paged to `host:quake1.swp`; the hunk 16 MiB (`-mem <MiB>`); built into VM (`MX_MEMORY=vm`, the default: started by `mxload.elf`, as `make run` does), only maxsdk, libc and libgl in physical memory; libgl's pinned memory (what the GS reads by DMA) taken from the frames as needed, the textures' larger levels pageable; stack 1 MiB. In PCSX2 only on the EE interpreter (`PCSX2_EE=interp`: the recompiler raises no TLB misses); `MX_MEMORY=phys` builds it to run without VM, in 32 MiB |
| Sound | id's mixer (`snd_dma.c`, `snd_mix.c`) at 22050 Hz 16-bit stereo into a ring, sent to maxsdk's PCM stream (`mx/snd.h`, audsrv) by `ps2/snd_ps2.c` as far as the mixer has painted |
| Math | float literals (`-cl-single-precision-constant`) and float math.h (`ps2/ps2_math.h`): the FPU has no doubles |

Not done: CD audio (id's null driver), network play (loopback only).

## Video modes: 1080i, beam racing, 24-bit colour

Options, Video Options sets three things, each at once:

- Resolution: the TV's mode or 1080i (`vid_mode 0` / `1`, not kept in the config: a TV
  without 1080i would show nothing at the next start). In 1080i the 3D is drawn on all of
  the 1920 x 1080 screen, the 2D (console, status bar, menus) is laid out in 640 x 360.
- Beam racing (`vid_beam`, libgl's `MXGL_BEAM`): one buffer of a field's lines, drawn in
  strips just ahead of the beam every field. It leaves GS memory for twice the textures in
  the TV's mode (3.6 MiB instead of 1.8, PAL 1.5), and shows at most a frame a field (60,
  PAL 50). Without it, 1080i is 640 x 540 stretched to the screen (two buffers).
- Colour (`vid_rgb24`, `MXGL_RGB24`): 24-bit colour, with beam racing in the TV's mode
  (3.3 MiB for textures); 16-bit (dithered) otherwise.

Beam racing takes 2 MiB more of pinned memory while it is on. When the world's buffer object
does not fit (a build without virtual memory, `MX_MEMORY=phys`, in PCSX2's 32 MiB), the
world is drawn from its polygons' own vertices.

On the PS2 (2026-10-31, the 1080i-only build before the switch): the demo loop at 59 to 61
fps, the `start` map at 60; no pass late. World polygons are drawn with `mxgl_DrawPolygon`
(`WinQuake/gl_rsurf.c`), alias models with `mxgl_MultiDraw`, the lightmaps buffer is a
byte a texel (GL_LUMINANCE only), and the palette goes to libgl (`mxgl_Palette`:
8-bit textures). `make PROF=1` profiles the second half of a `FRAMES` run
(`libs/maxsdk/tools/prof.py <elf> <log>`).
