# The PC build of the same engine (WinQuake/, GLQuake's non-PS2 paths) on
# the host's OpenGL, off screen: the reference picture for libgl.
#
#   make -C pc [FRAMES=n] [ARGS="+map e1m1"]   $(OUT)/quake1-pc
#   make -C pc run                              runs it in build/pcsx2-test
#                                               (the game data there) on
#                                               Mesa's llvmpipe: quake1-pc.tga
#
# FRAMES, ARGS: as the PS2 build's (../Makefile), the same command line.

HERE    := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PS2MAX  ?= $(abspath $(HERE)/../../..)
OUT     ?= $(PS2MAX)/build/apps/quake1-pc
W       := $(HERE)/../WinQuake

CC      ?= cc
CFLAGS  := -O2 -g -fno-pie -std=gnu89 -w -MMD -fcommon -fno-strict-aliasing -DGLQUAKE -DNDEBUG \
           -I$(W) -I$(HERE)/include -I$(OUT)
LDLIBS  := -lEGL -lOpenGL -lm

ENGINE  := cl_demo cl_input cl_main cl_parse cl_tent chase cmd common console crc cvar \
           gl_draw gl_mesh gl_model gl_refrag gl_rlight gl_rmain gl_rmisc gl_rsurf \
           gl_screen gl_test gl_warp host host_cmd keys menu mathlib net_loop net_main \
           net_vcr net_none pr_cmds pr_edict pr_exec r_part sbar sv_main sv_phys sv_move \
           sv_user zone view wad world cd_null snd_null
PCSRC   := sys_pc vid_pc
OBJS    := $(ENGINE:%=$(OUT)/q1/%.o) $(PCSRC:%=$(OUT)/pc/%.o)

all: $(OUT)/quake1-pc

$(OUT)/q1/%.o: $(W)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(OUT)/pc/%.o: $(HERE)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -std=gnu99 -c $< -o $@

RUNCFG := $(OUT)/runcfg.h
$(shell mkdir -p $(OUT); printf '#define PS2_FRAMES %s\n#define PS2_ARGS "%s"\n' \
  '$(or $(FRAMES),0)' '$(ARGS)' > $(RUNCFG).new; \
  cmp -s $(RUNCFG).new $(RUNCFG) || mv $(RUNCFG).new $(RUNCFG); rm -f $(RUNCFG).new)

$(OUT)/quake1-pc: $(OBJS)
	$(CC) -no-pie -o $@ $(OBJS) $(LDLIBS)

RUNDIR := $(PS2MAX)/build/pcsx2-test
run: $(OUT)/quake1-pc
	cd $(RUNDIR) && LIBGL_ALWAYS_SOFTWARE=1 $(OUT)/quake1-pc && mv quake1.tga quake1-pc.tga

-include $(OBJS:.o=.d)
.PHONY: all run
