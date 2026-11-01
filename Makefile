# Quake (id's GLQuake, WinQuake/) on maxsdk: the PS2 port.
#
#   make [OUT=<dir>] [FRAMES=n] [ARGS="+map e1m1"]   $(OUT)/quake1.elf
#   make run             quake1.elf in PCSX2 with the game data in
#                        build/pcsx2-test/id1 (Q1DATA: the retail ID1
#                        directory, copied there once)
#
# The engine draws through libs/libgl (an OpenGL 1.1 subset on the GS);
# ps2/ has the system, video, input and sound layers (sound: maxsdk's
# mx/snd.h stream). CD audio and the network are id's null drivers.

HERE     := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PS2MAX   ?= $(abspath $(HERE)/../..)
MAXSDK   ?= $(PS2MAX)/libs/maxsdk
MXGL     ?= $(PS2MAX)/libs/libgl
OUT      ?= $(PS2MAX)/build/apps/quake1
all: $(OUT)/quake1.elf
include $(MXGL)/mk/mxgl.mk
include $(MAXSDK)/mk/maxsdk.mk
# MX_MEMORY=vm (started by mxload.elf): Quake in VM, libgl in core
MX_CORE_OBJS = $(MXGL_OBJS)

PICOLIBC ?= $(PS2MAX)/build/libs/picolibc/install
LIBC_LIBS = $(MAXSDK_OUT)/libc/glue.c.o $(MAXSDK_OUT)/libmxc.a $(PICOLIBC)/lib/libc.a \
            $(MX_LIB) $(MAXSDK_OUT)/librt.a

W        := $(HERE)/WinQuake
P        := $(HERE)/ps2
# 1996 C (gnu89: implicit declarations); unsuffixed literals and the
# math.h functions are float (ps2/ps2_math.h): the FPU has no doubles
Q1CFLAGS  = $(filter-out -fno-common -ffreestanding -Wall -Wextra -Wno-unused-parameter -std=gnu11,$(CFLAGS)) \
            -std=gnu89 -w -MMD -fcommon -fno-strict-aliasing -ffast-math -fapprox-func \
            -cl-single-precision-constant -nostdlibinc -isystem $(PICOLIBC)/include \
            -DGLQUAKE -D__PS2__ -DNDEBUG -I$(W) -I$(P) -I$(OUT)/ps2 -I$(MXGL_INC) $(MXGL_DEFS) \
            -include $(P)/ps2_stdio.h -include $(P)/ps2_math.h

ENGINE   := cl_demo cl_input cl_main cl_parse cl_tent chase cmd common console crc cvar \
            gl_draw gl_mesh gl_model gl_refrag gl_rlight gl_rmain gl_rmisc gl_rsurf \
            gl_screen gl_test gl_warp host host_cmd keys menu mathlib net_loop net_main \
            net_vcr net_none pr_cmds pr_edict pr_exec r_part sbar sv_main sv_phys sv_move \
            sv_user zone view wad world cd_null snd_dma snd_mem snd_mix
PS2SRC   := sys_ps2 vid_ps2 ps2_file snd_ps2

OBJS     := $(patsubst %,$(OUT)/q1/%.o,$(ENGINE)) $(patsubst %,$(OUT)/ps2/%.o,$(PS2SRC))

$(OUT)/q1/%.o: $(W)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(Q1CFLAGS) -c $< -o $@
$(OUT)/ps2/%.o: $(P)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(Q1CFLAGS) -c $< -o $@

# FRAMES: end after that many frames with a picture (host:quake1.tga);
# ARGS: more of the command line. In $(OUT)/ps2/runcfg.h, rewritten only
# when they change.
RUNCFG := $(OUT)/ps2/runcfg.h
$(shell mkdir -p $(OUT)/ps2; printf '#define PS2_FRAMES %s\n#define PS2_ARGS "%s"\n#define PS2_PROF %s\n' \
  '$(or $(FRAMES),0)' '$(ARGS)' '$(or $(PROF),0)' > $(RUNCFG).new; \
  cmp -s $(RUNCFG).new $(RUNCFG) || mv $(RUNCFG).new $(RUNCFG); rm -f $(RUNCFG).new)

$(OUT)/quake1.elf: $(OBJS) $(MXGL_OBJS) $(HLSL_OBJS) $(MX_LIB) $(MX_CRT0) $(MX_LD) $(LIBC_LIBS)
	$(call link,$@,--defsym=__stack_size=0x100000 $(MX_CRT0) $(OBJS) $(MXGL_OBJS) $(HLSL_OBJS) $(LIBC_LIBS))

# ---- running --------------------------------------------------------------------
Q1DATA   ?= /mnt/truenas_data_exthdd/spellen/PC/Quake I/ID1
RUNDIR   := $(PS2MAX)/build/pcsx2-test
run: $(OUT)/quake1.elf
	@mkdir -p $(RUNDIR)/id1
	@for f in pak0 pak1; do [ -f $(RUNDIR)/id1/$$f.pak ] || \
	  cp -v "$(Q1DATA)/$$(echo $$f | tr a-z A-Z).PAK" $(RUNDIR)/id1/$$f.pak; done
	~/.claude/skills/ps2-pcsx2-test/run.sh $(OUT)/quake1.elf $(or $(TIMEOUT),600)

clean:
	rm -rf $(OUT)

.PHONY: all run clean

-include $(shell find $(OUT) -name '*.d' 2>/dev/null)
