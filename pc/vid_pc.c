/* vid_pc.c: the video of the PC build (pc/): vid_ps2.c's GL state on the
   host's OpenGL, drawn off screen (EGL pbuffer: no window, no display;
   LIBGL_ALWAYS_SOFTWARE=1 for Mesa's llvmpipe) in the PS2's PAL screen,
   640 x 512, 2D and 3D. No input. */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "quakedef.h"

#define WARP_WIDTH 320
#define WARP_HEIGHT 200
#define PC_W 640
#define PC_H 512

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned char d_15to8table[65536];

cvar_t gl_ztrick = {"gl_ztrick", "0"};

int texture_mode = GL_LINEAR;
int texture_extension_number = 1;
float gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean is8bit = false;
qboolean isPermedia = false;
qboolean gl_mtexable = false;
lpMTexFUNC qglMTexCoord2fSGIS;
lpSelTexFUNC qglSelectTextureSGIS;

static float vid_gamma = 1.0;

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height) {}
void D_EndDirectRect(int x, int y, int width, int height) {}

void VID_Shutdown(void) {}
void VID_ShiftPalette(unsigned char *p) {}

/* as vid_ps2.c */
void VID_SetPalette(unsigned char *palette) {
	byte *pal = palette;
	unsigned *table = d_8to24table;
	for (int i = 0; i < 256; i++, pal += 3)
		*table++ = (255u << 24) + (pal[0] << 0) + (pal[1] << 8) + (pal[2] << 16);
	d_8to24table[255] &= 0xffffff; /* 255 is transparent */
}

qboolean VID_Is8bit(void) { return false; }

/* as vid_ps2.c */
void GL_Init(void) {
	gl_vendor = (const char *)glGetString(GL_VENDOR);
	Con_Printf("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (const char *)glGetString(GL_RENDERER);
	Con_Printf("GL_RENDERER: %s\n", gl_renderer);
	gl_version = (const char *)glGetString(GL_VERSION);
	Con_Printf("GL_VERSION: %s\n", gl_version);
	gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

	glClearColor(1, 0, 0, 0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void GL_BeginRendering(int *x, int *y, int *width, int *height) {
	*x = *y = 0;
	*width = PC_W, *height = PC_H;
	/* libgl clears colour and Z at each frame's start */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GL_EndRendering(void) {
	glFlush();
	Sbar_Changed(); /* as vid_ps2.c: the status bar each frame */
}

/* as vid_ps2.c */
static void Check_Gamma(unsigned char *pal) {
	int i;
	if ((i = COM_CheckParm("-gamma")) == 0)
		vid_gamma = 0.7f;
	else
		vid_gamma = Q_atof(com_argv[i + 1]);
	for (i = 0; i < 768; i++) {
		float f = powf((pal[i] + 1) / 256.0f, vid_gamma);
		float inf = f * 255 + 0.5f;
		pal[i] = inf < 0 ? 0 : inf > 255 ? 255 : (unsigned char)inf;
	}
}

static void egl_init(void) {
	PFNEGLGETPLATFORMDISPLAYEXTPROC get_display =
	    (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	EGLDisplay d = get_display ? get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL)
	                           : EGL_NO_DISPLAY;
	if (d == EGL_NO_DISPLAY || !eglInitialize(d, NULL, NULL))
		Sys_Error("EGL: no display");
	static const EGLint cattr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8,
	                               EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
	                               EGL_DEPTH_SIZE, 24, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	                               EGL_NONE};
	EGLConfig cfg;
	EGLint n;
	if (!eglChooseConfig(d, cattr, &cfg, 1, &n) || n < 1)
		Sys_Error("EGL: no config");
	static const EGLint sattr[] = {EGL_WIDTH, PC_W, EGL_HEIGHT, PC_H, EGL_NONE};
	EGLSurface s = eglCreatePbufferSurface(d, cfg, sattr);
	eglBindAPI(EGL_OPENGL_API);
	EGLContext c = eglCreateContext(d, cfg, EGL_NO_CONTEXT, NULL);
	if (s == EGL_NO_SURFACE || c == EGL_NO_CONTEXT || !eglMakeCurrent(d, s, s, c))
		Sys_Error("EGL: no context");
}

void VID_Init(unsigned char *palette) {
	Cvar_RegisterVariable(&gl_ztrick);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	vid.numpages = 2;
	vid.conwidth = vid.width = PC_W;
	vid.conheight = vid.height = PC_H;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0f / 240.0f);
	vid.recalc_refdef = 1;

	egl_init();
	GL_Init();
	Check_Gamma(palette);
	VID_SetPalette(palette);
	Con_SafePrintf("Video mode %dx%d (PC, off screen).\n", PC_W, PC_H);
}

/* the last frame as a TGA (24-bit, bottom-up, as libgl's mxgl_screenshot) */
int pc_screenshot(const char *path) {
	static byte px[PC_W * PC_H * 3];
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, PC_W, PC_H, GL_BGR, GL_UNSIGNED_BYTE, px);
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;
	byte h[18] = {0};
	h[2] = 2;
	h[12] = PC_W & 255, h[13] = PC_W >> 8, h[14] = PC_H & 255, h[15] = PC_H >> 8, h[16] = 24;
	fwrite(h, 1, 18, f);
	fwrite(px, 1, sizeof px, f);
	fclose(f);
	return 0;
}

/* ---- input: none ---------------------------------------------------------- */

void IN_Init(void) {}
void IN_Shutdown(void) {}
void Sys_SendKeyEvents(void) {}
void IN_Commands(void) {}
void IN_Move(usercmd_t *cmd) {}
