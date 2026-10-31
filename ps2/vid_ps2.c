/* vid_ps2.c: the video and input of GLQuake on maxsdk: the screen is
   maxsdk_gl's (libs/maxsdk_gl, 640 x 448), the input pad 0. */

#include <mx/mx.h>
#include <mx/pad.h>
#include <mxgl.h>

#include "quakedef.h"

#define WARP_WIDTH 320
#define WARP_HEIGHT 200

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned char d_15to8table[65536];

cvar_t vid_mode = {"vid_mode", "0", false};
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

/* d_8to24table only: d_15to8table is for 8-bit textures, which are not here */
void VID_SetPalette(unsigned char *palette) {
	byte *pal = palette;
	unsigned *table = d_8to24table;
	for (int i = 0; i < 256; i++, pal += 3)
		*table++ = (255u << 24) + (pal[0] << 0) + (pal[1] << 8) + (pal[2] << 16);
	d_8to24table[255] &= 0xffffff; /* 255 is transparent */
}

qboolean VID_Is8bit(void) { return false; }

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
	*width = MXGL_WIDTH;
	*height = MXGL_HEIGHT;
}


void GL_EndRendering(void) {
	mxgl_end_frame();
	mxgl_frame_done();
	/* the status bar each frame (gl_vidnt.c's -fullsbar): the frame is
	   cleared, and the view covers the bar when it is full size */
	Sbar_Changed();
}

static void Check_Gamma(unsigned char *pal) {
	int i;
	if ((i = COM_CheckParm("-gamma")) == 0)
		vid_gamma = 0.7f; /* GLQuake's default on non-3dfx hardware */
	else
		vid_gamma = Q_atof(com_argv[i + 1]);
	for (i = 0; i < 768; i++) {
		float f = powf((pal[i] + 1) / 256.0f, vid_gamma);
		float inf = f * 255 + 0.5f;
		pal[i] = inf < 0 ? 0 : inf > 255 ? 255 : (unsigned char)inf;
	}
}

void VID_Init(unsigned char *palette) {
	Cvar_RegisterVariable(&vid_mode);
	Cvar_RegisterVariable(&gl_ztrick);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
#ifdef MXGL_HIRES
	/* the 2D (console, status bar, menus) in 640 x 360 units, the size it
	   has at 640 x 448; the 3D in all 1920 x 1080 (GL_BeginRendering) */
	vid.conwidth = vid.width = 640;
	vid.conheight = vid.height = 360;
#else
	vid.conwidth = vid.width = MXGL_WIDTH;
	vid.conheight = vid.height = MXGL_HEIGHT;
#endif
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0f / 240.0f);
	vid.numpages = 2;

	mxgl_init();
	GL_Init();
	Check_Gamma(palette);
	VID_SetPalette(palette);
	Con_SafePrintf("Video mode %dx%d initialized.\n", MXGL_WIDTH, MXGL_HEIGHT);
	vid.recalc_refdef = 1;
}

/* ---- input: pad 0 ------------------------------------------------------- */

static int pad_ok;
static const struct {
	u32 bit;
	int key;
} pad_keys[] = {
	{MX_PAD_UP, K_UPARROW},     {MX_PAD_DOWN, K_DOWNARROW}, {MX_PAD_LEFT, K_LEFTARROW},
	{MX_PAD_RIGHT, K_RIGHTARROW}, {MX_PAD_CROSS, K_ENTER},  {MX_PAD_CIRCLE, K_AUX1},
	{MX_PAD_SQUARE, K_AUX2},    {MX_PAD_TRIANGLE, K_AUX3},  {MX_PAD_R1, K_CTRL},
	{MX_PAD_R2, K_AUX4},        {MX_PAD_L1, K_AUX5},        {MX_PAD_L2, K_AUX6},
	{MX_PAD_START, K_ESCAPE},   {MX_PAD_SELECT, K_TAB},
};
static float stick[4]; /* right x, y, left x, y: -1 .. 1 */

void IN_Init(void) {
	pad_ok = mx_pad_init() >= 0;
	Con_Printf("pad: %s\n", pad_ok ? "ok" : "no driver");
}

void IN_Shutdown(void) {}

void Sys_SendKeyEvents(void) {
	if (!pad_ok)
		return;
	static u32 last;
	u32 held = mx_pad_buttons(0);
	for (unsigned i = 0; i < sizeof pad_keys / sizeof pad_keys[0]; i++) {
		u32 b = pad_keys[i].bit;
		if ((held ^ last) & b)
			Key_Event(pad_keys[i].key, (held & b) != 0);
	}
	last = held;
	u8 st[4];
	if (mx_pad_sticks(0, st))
		for (int k = 0; k < 4; k++) {
			int v = (int)st[k] - 128;
			v = v > -32 && v < 32 ? 0 : v > 0 ? v - 32 : v + 32;
			stick[k] = v / 96.0f;
		}
}

void IN_Commands(void) {}

/* the left stick moves, the right one turns (180 degrees a second at full
   turn) */
void IN_Move(usercmd_t *cmd) {
	if (key_dest != key_game)
		return;
	cmd->forwardmove -= stick[3] * cl_forwardspeed.value;
	cmd->sidemove += stick[2] * cl_sidespeed.value;
	cl.viewangles[YAW] -= stick[0] * 180.0f * host_frametime;
	cl.viewangles[PITCH] += stick[1] * 135.0f * host_frametime;
	if (stick[1])
		V_StopPitchDrift();
	if (cl.viewangles[PITCH] > 80)
		cl.viewangles[PITCH] = 80;
	if (cl.viewangles[PITCH] < -70)
		cl.viewangles[PITCH] = -70;
}
