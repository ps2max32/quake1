/* sys_ps2.c: the system layer of GLQuake on maxsdk: main, time, files
   (stdio on host: through ps2_file.c), errors. Sound, CD audio and the
   network are id's null drivers (snd_null.c, cd_null.c, net_none.c). */

#include <mx/mx.h>
#include <mx/prof.h>
#include <mx/vm.h>
#include <mxgl.h>

#include "quakedef.h"

void mx_exit(int status) __attribute__((noreturn));

/* frames to run before the program ends by itself (0: never) with a
   picture, and more of the command line; set by the Makefile */
#include "runcfg.h"

static void ps2_end(int status) {
	mx_printf("@@PS2TEST_END@@ status=%d\n", status);
	mx_exit(status);
}

/* ---- files ------------------------------------------------------------------ */

#define MAX_HANDLES 10
static FILE *sys_handles[MAX_HANDLES];

static int findhandle(void) {
	for (int i = 1; i < MAX_HANDLES; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error("out of handles");
	return -1;
}

static int filelength(FILE *f) {
	int pos = ftell(f);
	fseek(f, 0, SEEK_END);
	int end = ftell(f);
	fseek(f, pos, SEEK_SET);
	return end;
}

int Sys_FileOpenRead(char *path, int *hndl) {
	int i = findhandle();
	FILE *f = fopen(path, "rb");
	if (!f) {
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	return filelength(f);
}

int Sys_FileOpenWrite(char *path) {
	int i = findhandle();
	FILE *f = fopen(path, "wb");
	if (!f)
		Sys_Error("Error opening %s", path);
	sys_handles[i] = f;
	return i;
}

void Sys_FileClose(int handle) {
	fclose(sys_handles[handle]);
	sys_handles[handle] = NULL;
}
void Sys_FileSeek(int handle, int position) { fseek(sys_handles[handle], position, SEEK_SET); }
int Sys_FileRead(int handle, void *dest, int count) {
	return fread(dest, 1, count, sys_handles[handle]);
}
int Sys_FileWrite(int handle, void *data, int count) {
	return fwrite(data, 1, count, sys_handles[handle]);
}

int Sys_FileTime(char *path) {
	FILE *f = fopen(path, "rb");
	if (f) {
		fclose(f);
		return 1;
	}
	return -1;
}

void Sys_mkdir(char *path) {}

/* ---- the system -------------------------------------------------------------- */

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length) {}

void Sys_Error(char *error, ...) {
	va_list ap;
	char text[1024];
	va_start(ap, error);
	vsnprintf(text, sizeof text, error, ap);
	va_end(ap);
	mx_printf("Sys_Error: %s\n", text);
	ps2_end(1);
}

void Sys_Printf(char *fmt, ...) {
	va_list ap;
	char text[2048];
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	/* Quake's high-bit characters (coloured text) as plain ones */
	for (char *p = text; *p; p++)
		*p &= 0x7f;
	mx_printf("%s", text);
}

void Sys_Quit(void) {
	mx_printf("Sys_Quit\n");
	ps2_end(0);
}

static u64 cycles(void) {
	static u32 last, high;
	u32 c = mx_count();
	if (c < last)
		high++;
	last = c;
	return (u64)high << 32 | c;
}

double Sys_FloatTime(void) {
	static u64 base;
	u64 c = cycles();
	if (!base)
		base = c;
	return (double)(c - base) / MX_EE_HZ;
}

char *Sys_ConsoleInput(void) { return NULL; }
void Sys_Sleep(void) {}
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}

/* ---- main --------------------------------------------------------------------- */

/* maxsdk's virtual memory (<mx/vm.h>): malloc (the hunk) in up to 128 MiB,
   paged to a swap file next to the game's data; what the GS reads by DMA
   (libgl: arenas, textures) is pinned memory, frames taken as needed.
   MX_MEMORY=phys: no VM, everything in the 32 MiB */
#if PS2_VM
const struct mx_vm_config mx_vm_config = {
	.swap = "host:quake1.swp",
};
#endif

int main(int argc, char **argv) {
	static quakeparms_t parms;
	/* the command line: GLQuake's settings for the PS2, then PS2_ARGS */
	/* the textures at full size (up to 256: libgl leaves their top levels
	   out while GS memory refills, MXGL_TEX_AUTO in vid_ps2.c), trilinear;
	   dynamic lights in the lightmaps, not as glow balls */
	static char line[512] = "quake -basedir host: +gl_max_size 256 "
	                        "+gl_texturemode GL_LINEAR_MIPMAP_LINEAR +gl_flashblend 0 " PS2_ARGS;
	static char *args[64];
	int n = 0;
	for (char *p = strtok(line, " "); p && n < 63; p = strtok(NULL, " "))
		args[n++] = p;

	/* the hunk: 16 MiB as GLQuake on Linux, or -mem <MiB> */
	COM_InitArgv(n, args);
	parms.memsize = 16 * 1024 * 1024;
	int j = COM_CheckParm("-mem");
	if (j && j + 1 < com_argc)
		parms.memsize = (int)(Q_atof(com_argv[j + 1]) * 1024 * 1024);
	parms.membase = malloc(parms.memsize);
	if (!parms.membase)
		Sys_Error("no memory for the hunk");
	parms.basedir = "host:";
	parms.argc = com_argc;
	parms.argv = com_argv;
	mx_printf("Quake (GLQuake) on maxsdk: hunk %d MiB, virtual memory %s\n", parms.memsize >> 20,
	          mx_vm_active() ? "on" : "off");
	Host_Init(&parms);

	/* frames: those Host_Frame ran (host_framecount; it skips the calls
	   that come sooner than host_maxfps allows) */
	double oldtime = Sys_FloatTime();
	int last = -1;
	for (;;) {
		double newtime = Sys_FloatTime();
		Host_Frame(newtime - oldtime);
		oldtime = newtime;
		if (host_framecount == last)
			continue;
		last = host_framecount;
		if (last % 60 == 0)
			mx_printf("ps2: frame %d at %d ms\n", last, (int)(newtime * 1000));
#if PS2_PROF
		/* the profile of the second half of the run */
		if (PS2_FRAMES && last == PS2_FRAMES / 2)
			mx_prof_start(2000);
#endif
		if (PS2_FRAMES && last + 1 >= PS2_FRAMES) {
#if PS2_PROF
			mx_prof_stop();
			mx_prof_report(4000);
#endif
			int r = mxgl_screenshot("host:quake1.tga");
			mx_printf("ps2: %d frames, picture host:quake1.tga: %d\n", last + 1, r);
			ps2_end(0);
		}
	}
	return 0;
}

/* ---- engine and libc odds ----------------------------------------------------- */

qboolean isDedicated;

/* console.c's log (-condebug) only: no POSIX files here */
int open(const char *path, int flags, ...) { return -1; }
int close(int fd) { return -1; }
int unlink(const char *path) { return -1; }
