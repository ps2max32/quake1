/* sys_pc.c: the system layer of the PC build (pc/): the same engine
   (WinQuake/, GLQuake's non-PS2 paths) on the host's OpenGL, as the
   reference for libgl: the same command line as sys_ps2.c, FRAMES and ARGS
   the same way, the picture of the last frame as quake1.tga. Sound, CD
   audio and the network are id's null drivers. Time is the frames': each
   frame 1/PC_FPS of a second, so a run gives the same picture each time. */

#include <malloc.h>
#include <stdint.h>

#include "quakedef.h"

#include "runcfg.h"

#ifndef PC_FPS
#define PC_FPS 50 /* PAL with beam racing: a frame a field */
#endif

int pc_screenshot(const char *path);

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
	long pos = ftell(f);
	fseek(f, 0, SEEK_END);
	long end = ftell(f);
	fseek(f, pos, SEEK_SET);
	return (int)end;
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
	return (int)fread(dest, 1, count, sys_handles[handle]);
}
int Sys_FileWrite(int handle, void *data, int count) {
	return (int)fwrite(data, 1, count, sys_handles[handle]);
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
	printf("Sys_Error: %s\n", text);
	exit(1);
}

void Sys_Printf(char *fmt, ...) {
	va_list ap;
	char text[2048];
	va_start(ap, fmt);
	vsnprintf(text, sizeof text, fmt, ap);
	va_end(ap);
	for (char *p = text; *p; p++)
		*p &= 0x7f;
	fputs(text, stdout);
}

void Sys_Quit(void) {
	printf("Sys_Quit\n");
	exit(0);
}

static int frames_run;
double Sys_FloatTime(void) { return (double)frames_run / PC_FPS; }

char *Sys_ConsoleInput(void) { return NULL; }
void Sys_Sleep(void) {}
void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void) {}

/* ---- main --------------------------------------------------------------------- */

int main(int argc, char **argv) {
	static quakeparms_t parms;
	/* sys_ps2.c's command line, and the cvars the PS2 build has as its
	   defaults (gl_rmain.c: gl_keeptjunctions) */
	static char line[512] = "quake -basedir . +gl_max_size 256 "
	                        "+gl_texturemode GL_LINEAR_MIPMAP_LINEAR +gl_flashblend 0 "
	                        "+gl_keeptjunctions 1 " PS2_ARGS;
	static char *args[64];
	int n = 0;
	for (char *p = strtok(line, " "); p && n < 63; p = strtok(NULL, " "))
		args[n++] = p;

	COM_InitArgv(n, args);
	parms.memsize = 16 * 1024 * 1024;
	int j = COM_CheckParm("-mem");
	if (j && j + 1 < com_argc)
		parms.memsize = (int)(Q_atof(com_argv[j + 1]) * 1024 * 1024);
	/* QuakeC's strings are 32-bit offsets from pr_strings (in the hunk) to
	   strings anywhere in the engine's memory: the hunk from the brk heap,
	   right after .bss (the program linked without PIE), not from an mmap
	   far away; then all of them are within 2 GiB of each other */
	mallopt(M_MMAP_THRESHOLD, 512 * 1024 * 1024);
	parms.membase = malloc(parms.memsize);
	if (!parms.membase)
		Sys_Error("no memory for the hunk");
	parms.basedir = ".";
	parms.argc = com_argc;
	parms.argv = com_argv;
	printf("Quake (GLQuake) on the PC: hunk %d MiB\n", parms.memsize >> 20);
	Host_Init(&parms);

	int last = -1;
	for (;;) {
		frames_run++;
		Host_Frame(1.0f / PC_FPS);
		if (host_framecount == last)
			continue;
		last = host_framecount;
		if (last % 60 == 0)
			printf("pc: frame %d\n", last);
		if (PS2_FRAMES && last + 1 >= PS2_FRAMES) {
			int r = pc_screenshot("quake1.tga");
			printf("pc: %d frames, picture quake1.tga: %d\n", last + 1, r);
			exit(0);
		}
	}
	return 0;
}

qboolean isDedicated;
