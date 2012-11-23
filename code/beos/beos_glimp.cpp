/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*
** GLW_IMP.C
**
** This file contains ALL Linux specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
** GLimp_SetGamma
**
*/

#include <termios.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "beos_local.h" // bk001130
#include "beos_glw.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <DirectWindow.h>
#include <GLView.h>
#include "beos_glwindow.h"
#include <Screen.h>

QWindow	*sWindow;

typedef enum {
  RSERR_OK,
  RSERR_INVALID_FULLSCREEN,
  RSERR_INVALID_MODE,
  RSERR_UNKNOWN
} rserr_t;

glwstate_t glw_state;

static int scrnum;

static qboolean mouse_avail;
static qboolean mouse_active = qfalse;
//static int mwx, mwy;
//static int mx = 0, my = 0;

// Time mouse was reset, we ignore the first 50ms of the mouse to allow settling of events
static int mouseResetTime = 0;
#define MOUSE_RESET_DELAY 50

static cvar_t *in_mouse;
static cvar_t *in_dgamouse; // user pref for dga mouse
cvar_t *in_subframe;
cvar_t *in_nograb; // this is strictly for developers

// bk001130 - from cvs1.17 (mkv), but not static
cvar_t   *in_joystick      = NULL;
cvar_t   *in_joystickDebug = NULL;
cvar_t   *joy_threshold    = NULL;

cvar_t  *r_previousglDriver;

qboolean vidmode_ext = qfalse;
static int vidmode_MajorVersion = 0, vidmode_MinorVersion = 0; // major and minor of XF86VidExtensions

static int win_x, win_y;

static int num_vidmodes;
static qboolean vidmode_active = qfalse;

static int mouse_accel_numerator;
static int mouse_accel_denominator;
static int mouse_threshold;    

/*
* Find the first occurrence of find in s.
*/
// bk001130 - from cvs1.17 (mkv), const
// bk001130 - made first argument const
static const char *Q_stristr( const char *s, const char *find) {
	register char c, sc;
	register size_t len;

	if ((c = *find++) != 0) {
		if (c >= 'a' && c <= 'z') {
			c -= ('a' - 'A');
		}
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return NULL;
				if (sc >= 'a' && sc <= 'z') {
					sc -= ('a' - 'A');
				}
			} while (sc != c);
		} while (Q_stricmpn(s, find, len) != 0);
		s--;
	}
	return s;
}

/*****************************************************************************
** KEYBOARD
** NOTE TTimo the keyboard handling is done with KeySyms
**   that means relying on the keyboard mapping provided by X
**   in-game it would probably be better to use KeyCode (i.e. hardware key codes)
**   you would still need the KeySyms in some cases, such as for the console and all entry textboxes
**     (cause there's nothing worse than a qwerty mapping on a french keyboard)
**
** you can turn on some debugging and verbose of the keyboard code with #define KBD_DBG
******************************************************************************/

//#define KBD_DBG

// NOTE TTimo for the tty console input, we didn't rely on those .. 
//   it's not very surprising actually cause they are not used otherwise
void KBD_Init(void){
	Com_Printf("KBD_Init()\n");
}

void KBD_Close(void){
	Com_Printf("KBD_Close()\n");
}

void IN_ActivateMouse(void) {
	if (!mouse_active) {
		mouse_active = qtrue;
	}
}

void IN_DeactivateMouse( void ) {
	if (mouse_active) {
		mouse_active = qfalse;
	}
}
/*****************************************************************************/

/*
** GLimp_SetGamma
**
** This routine should only be called if glConfig.deviceSupportsGamma is TRUE
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256]) {
	// NOTE TTimo we get the gamma value from cvar, because we can't work with the s_gammatable
	// the API wasn't changed to avoid breaking other OSes
	float g = Cvar_Get("r_gamma", "1.0", 0)->value;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void ) {
	IN_DeactivateMouse();
	memset(&glConfig, 0, sizeof(glConfig));
	memset(&glState, 0, sizeof(glState));
	sWindow->Lock();
	sWindow->Quit();
	QGL_Shutdown();
}

/*
** GLimp_LogComment
*/
extern "C" void GLimp_LogComment(char *comment) {
	if (glw_state.log_fp) {
		fprintf( glw_state.log_fp, "%s", comment );
	}
}

/*
** GLW_SetMode
*/
int GLW_SetMode(const char *drivername, int mode, qboolean fullscreen) {
	/*int attrib[] = {
		GLX_RGBA,         // 0
		GLX_RED_SIZE, 4,      // 1, 2
		GLX_GREEN_SIZE, 4,      // 3, 4
		GLX_BLUE_SIZE, 4,     // 5, 6
		GLX_DOUBLEBUFFER,     // 7
		GLX_DEPTH_SIZE, 1,      // 8, 9
		GLX_STENCIL_SIZE, 1,    // 10, 11
		None
	};
	*/
	// these match in the array
	#define ATTR_RED_IDX 2
	#define ATTR_GREEN_IDX 4
	#define ATTR_BLUE_IDX 6
	#define ATTR_DEPTH_IDX 9
	#define ATTR_STENCIL_IDX 11
	//Window root;
	//XVisualInfo *visinfo;
	//XSetWindowAttributes attr;
	//XSizeHints sizehints;
	unsigned long mask;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int dga_MajorVersion, dga_MinorVersion;
	int actualWidth, actualHeight;
	int i;
	const char*   glstring; // bk001130 - from cvs1.17 (mkv)
	ri.Printf( PRINT_ALL, "Initializing OpenGL display\n");
	ri.Printf (PRINT_ALL, "...setting mode %d:", mode );

	if (!R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode)) {
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	/*
	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Error couldn't open the X display\n");
		return RSERR_INVALID_MODE;
	}
	*/
	//scrnum = DefaultScreen(dpy);
	//root = RootWindow(dpy, scrnum);
	/*
	actualWidth = glConfig.vidWidth;
	actualHeight = glConfig.vidHeight;
	*/
	/*
	if (vidmode_ext) {
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);

		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen) {
		best_dist = 9999999;
		best_fit = -1;

		for (i = 0; i < num_vidmodes; i++) {
			if (glConfig.vidWidth > vidmodes[i]->hdisplay || glConfig.vidHeight > vidmodes[i]->vdisplay)
				continue;

			x = glConfig.vidWidth - vidmodes[i]->hdisplay;
			y = glConfig.vidHeight - vidmodes[i]->vdisplay;
			dist = (x * x) + (y * y);
			if (dist < best_dist) {
				best_dist = dist;
				best_fit = i;
			}
		}

		if (best_fit != -1) {
			actualWidth = vidmodes[best_fit]->hdisplay;
			actualHeight = vidmodes[best_fit]->vdisplay;

			// change to the mode
			XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
			vidmode_active = qtrue;

			// Move the viewport to top left
			XF86VidModeSetViewPort(dpy, scrnum, 0, 0);

			ri.Printf(PRINT_ALL, "XFree86-VidModeExtension Activated at %dx%d\n", actualWidth, actualHeight);

		} else {
			fullscreen = 0;
			ri.Printf(PRINT_ALL, "XFree86-VidModeExtension: No acceptable modes found\n");
		}
	} else {
		ri.Printf(PRINT_ALL, "XFree86-VidModeExtension:  Ignored on non-fullscreen/Voodoo\n");
	}
	}
*/
	if (!r_colorbits->value)
		colorbits = 24;
	else
		colorbits = (int)r_colorbits->value;

	if ( !Q_stricmp( r_glDriver->string, _3DFX_DRIVER_NAME ) )
		colorbits = 16;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = (int)r_depthbits->value;
	
	stencilbits = (int)r_stencilbits->value;

	for (i = 0; i < 16; i++) {
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i) {
			// one pass, reduce
			switch (i / 4) {
			case 2 :
				if (colorbits == 24)
					colorbits = 16;
				break;
			case 1 :
				if (depthbits == 24)
					depthbits = 16;
				else if (depthbits == 16)
					depthbits = 8;
			case 3 :
				if (stencilbits == 24)
					stencilbits = 16;
				else if (stencilbits == 16)
					stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3) {
			// reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2) {
		// reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1) {
			// reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		if (tcolorbits == 24) {
			//attrib[ATTR_RED_IDX] = 8;
			//attrib[ATTR_GREEN_IDX] = 8;
			//attrib[ATTR_BLUE_IDX] = 8;
		} else {
			// must be 16 bit
			//attrib[ATTR_RED_IDX] = 4;
			//attrib[ATTR_GREEN_IDX] = 4;
			//attrib[ATTR_BLUE_IDX] = 4;
		}

		//attrib[ATTR_DEPTH_IDX] = tdepthbits; // default to 24 depth
		//attrib[ATTR_STENCIL_IDX] = tstencilbits;

		//visinfo = qglXChooseVisual(dpy, scrnum, attrib);
		//if (!visinfo) {
			//continue;
		//}

		//ri.Printf( PRINT_ALL, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n", 
		//           attrib[ATTR_RED_IDX], attrib[ATTR_GREEN_IDX], attrib[ATTR_BLUE_IDX],
		//           attrib[ATTR_DEPTH_IDX], attrib[ATTR_STENCIL_IDX]);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	//if (!visinfo) {
		//ri.Printf( PRINT_ALL, "Couldn't get a visual\n" );
		//return RSERR_INVALID_MODE;
	//}

	/* window attributes */
	//attr.background_pixel = BlackPixel(dpy, scrnum);
	//attr.border_pixel = 0;
	//attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	//attr.event_mask = X_MASK;
	/*
	if (vidmode_active) {
		mask = CWBackPixel | CWColormap | CWSaveUnder | CWBackingStore | CWEventMask | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	} else
		mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow(dpy, root, 0, 0, actualWidth, actualHeight, 0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);

	XStoreName( dpy, win, WINDOW_CLASS_NAME );

	//GH: Don't let the window be resized
	sizehints.flags = PMinSize | PMaxSize;
	sizehints.min_width = sizehints.max_width = actualWidth;
	sizehints.min_height = sizehints.max_height = actualHeight;

	XSetWMNormalHints( dpy, win, &sizehints );

	XMapWindow(dpy, win);

	if (vidmode_active)
		XMoveWindow(dpy, win, 0, 0);

	XFlush(dpy);
	XSync(dpy,False); // bk001130 - from cvs1.17 (mkv)
	ctx = qglXCreateContext(dpy, visinfo, NULL, True);
	XSync(dpy,False); // bk001130 - from cvs1.17 (mkv)

	// GH: Free the visinfo after we're done with it 
	XFree(visinfo);

	qglXMakeCurrent(dpy, win, ctx);
*/


	BScreen screen;
	uint32 num_vidmodes;
	display_mode *vidmodes;
	
	int best_fit, best_dist, dist, x, y;
	screen.GetModeList(&vidmodes, &num_vidmodes);
	
	
	// Are we going fullscreen? If so, let's change video mode
	if (fullscreen) {
		best_dist = 9999999;
		best_fit = -1;
		for (i = 0; i < num_vidmodes; i++) {
			if (glConfig.vidWidth > vidmodes[i].virtual_width || glConfig.vidHeight > vidmodes[i].virtual_height)
				continue;
			x = glConfig.vidWidth - vidmodes[i].virtual_width;
			y = glConfig.vidHeight - vidmodes[i].virtual_height;
			dist = (x * x) + (y * y);
			if (dist < best_dist) {
				best_dist = dist;
				best_fit = i;
			}
		}
	}

	//Yes, we are changing mode
	if (best_fit != -1) {
		actualWidth = vidmodes[best_fit].virtual_width;
		actualHeight = vidmodes[best_fit].virtual_height;
		// change to the mode
		//XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
	//	screen.SetMode(&vidmodes[best_fit]);
		vidmode_active = qtrue;
		// Move the viewport to top left
		//XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
		ri.Printf(PRINT_ALL, "XFree86-VidModeExtension Activated at %dx%d\n", actualWidth, actualHeight);
	} else {
		fullscreen = qfalse;
		ri.Printf(PRINT_ALL, "XFree86-VidModeExtension: No acceptable modes found\n");
	}


	sWindow = new QWindow(BRect(30+1, 30+1, 30+glConfig.vidWidth, 30+glConfig.vidHeight), fullscreen);
	sWindow->fView->LockGL();
	
	// bk001130 - from cvs1.17 (mkv)
	glstring = (char*)qglGetString (GL_RENDERER);
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glstring );

	return RSERR_OK;
}

/*
** GLW_StartDriverAndSetMode
*/
// bk001204 - prototype needed
static qboolean GLW_StartDriverAndSetMode( const char *drivername, int mode, qboolean fullscreen ) {
	int /*rserr_tjens*/ err;
	// don't ever bother going into fullscreen with a voodoo card
#if 1	// JDC: I reenabled this
	if (Q_stristr( drivername, "Voodoo")) {
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}
#endif
	
	if (fullscreen && in_nograb->value)	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;		
	}

	err = GLW_SetMode(drivername, mode, fullscreen);

	switch (err) {
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
			return qfalse;
		default:
			break;
	}
	return qtrue;
}

/*
** GLW_InitExtensions
*/
static void GLW_InitExtensions(void) {
	if (!r_allowExtensions->integer) {
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n");
		return;
	}
	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n");

	// GL_S3_s3tc
	if(Q_stristr( glConfig.extensions_string, "GL_S3_s3tc")) {
		if (r_ext_compressed_textures->value) {
			glConfig.textureCompression = TC_S3TC;
			ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n");
		} else {
			glConfig.textureCompression = TC_NONE;
			ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n");
		}
	} else {
		glConfig.textureCompression = TC_NONE;
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n");
	}

	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( Q_stristr( glConfig.extensions_string, "EXT_texture_env_add")) {
		if ( r_ext_texture_env_add->integer ) {
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n");
		} else {
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n");
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n");
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if (Q_stristr(glConfig.extensions_string, "GL_ARB_multitexture")) {
		if (r_ext_multitexture->value) {

			get_image_symbol(glw_state.OpenGLLib, "glMultiTexCoord2fARB",		B_SYMBOL_TYPE_TEXT, (void**)&qglMultiTexCoord2fARB);
			get_image_symbol(glw_state.OpenGLLib, "glActiveTextureARB",			B_SYMBOL_TYPE_TEXT, (void**)&qglActiveTextureARB);
			get_image_symbol(glw_state.OpenGLLib, "glClientActiveTextureARB",	B_SYMBOL_TYPE_TEXT, (void**)&qglClientActiveTextureARB);

			if(qglActiveTextureARB) {
				qglGetIntegerv(GL_MAX_ACTIVE_TEXTURES_ARB, (GLint*)&glConfig.maxActiveTextures);
				if (glConfig.maxActiveTextures > 1) {
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n");
				} else {
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n");
				}
			}
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n");
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n");
	}

	// GL_EXT_compiled_vertex_array
	if (Q_stristr( glConfig.extensions_string, "GL_EXT_compiled_vertex_array")) { /*jens la till NO för att disable libGL NV*/
		if (r_ext_compiled_vertex_array->value) {
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			
			get_image_symbol(glw_state.OpenGLLib, "glLockArraysEXT", B_SYMBOL_TYPE_TEXT, (void**)&qglLockArraysEXT);
			get_image_symbol(glw_state.OpenGLLib, "glUnlockArraysEXT", B_SYMBOL_TYPE_TEXT, (void**)&qglUnlockArraysEXT);

			if (!qglLockArraysEXT || !qglUnlockArraysEXT) {
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}
}

static void GLW_InitGamma() {
	/* Minimum extension version required */
	#define GAMMA_MINMAJOR 2
	#define GAMMA_MINMINOR 0

	glConfig.deviceSupportsGamma = qfalse;

	if (vidmode_ext) {
		if (vidmode_MajorVersion < GAMMA_MINMAJOR || (vidmode_MajorVersion == GAMMA_MINMAJOR && vidmode_MinorVersion < GAMMA_MINMINOR)) {
			ri.Printf( PRINT_ALL, "XF86 Gamma extension not supported in this version\n");
			return;
		}
		//jens    XF86VidModeGetGamma(dpy, scrnum, &vidmode_InitialGamma);
		ri.Printf( PRINT_ALL, "XF86 Gamma extension initialized\n");
		glConfig.deviceSupportsGamma = qtrue;
	}
}

/*
** GLW_LoadOpenGL
**
** GLimp_win.c internal function that that attempts to load and use 
** a specific OpenGL DLL.
*/
static qboolean GLW_LoadOpenGL( const char *name ) {
	qboolean fullscreen;

	ri.Printf( PRINT_ALL, "...loading %s: ", name );

  // disable the 3Dfx splash screen and set gamma
  // we do this all the time, but it shouldn't hurt anything
  // on non-3Dfx stuff
	putenv("FX_GLIDE_NO_SPLASH=0");

	// Mesa VooDoo hacks
	putenv("MESA_GLX_FX=fullscreen\n");

	// load the QGL layer
	if (QGL_Init(name)) {
 		fullscreen = (qboolean)r_fullscreen->integer;

		// create the window and set up the context
		if (!GLW_StartDriverAndSetMode(name, r_mode->integer, (qboolean)fullscreen)) {
			printf("GLW_LoadOpenGL2\n");
			if (r_mode->integer != 3) {
				if (!GLW_StartDriverAndSetMode(name, 3, fullscreen)) {
					goto fail;
				}
			} else
				goto fail;
		}
	return qtrue;
	} else {
		ri.Printf( PRINT_ALL, "failed\n" );
	}

	fail:
	QGL_Shutdown();
	return qfalse;
}

/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  
*/
void GLimp_Init( void ) {
	qboolean attemptedlibGL = qfalse;
	qboolean attempted3Dfx = qfalse;
	qboolean success = qfalse;
	char  buf[1024];
	cvar_t *lastValidRenderer = ri.Cvar_Get( "r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE );
	r_previousglDriver = ri.Cvar_Get( "r_previousglDriver", "", CVAR_ROM );
	InitSig();

	// Hack here so that if the UI 
	if (*r_previousglDriver->string) {
		// The UI changed it on us, hack it back
		// This means the renderer can't be changed on the fly
		ri.Cvar_Set( "r_glDriver", r_previousglDriver->string );
	}

	// set up our custom error handler for X failures
	//jens  XSetErrorHandler(&qXErrorHandler);

	//
	// load and initialize the specific OpenGL driver
	//
	if (!GLW_LoadOpenGL(r_glDriver->string)) {
		if (!Q_stricmp( r_glDriver->string, OPENGL_DRIVER_NAME)) {
			attemptedlibGL = qtrue;
		} else if (!Q_stricmp( r_glDriver->string, _3DFX_DRIVER_NAME)) {
			attempted3Dfx = qtrue;
		}

		// try ICD before trying 3Dfx standalone driver
		if (!attemptedlibGL && !success) {
			attemptedlibGL = qtrue;
			if (GLW_LoadOpenGL(OPENGL_DRIVER_NAME)) {
				ri.Cvar_Set( "r_glDriver", OPENGL_DRIVER_NAME );
				r_glDriver->modified = qfalse;
				success = qtrue;
			}
		}
		if (!success)
			ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n" );

	}

	// Save it in case the UI stomps it
	ri.Cvar_Set("r_previousglDriver", r_glDriver->string);

	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (const char *)qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (const char *)qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	if (*glConfig.renderer_string && glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n')
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	Q_strncpyz( glConfig.version_string, (const char *)qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (const char *)qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	//
	// chipset specific configuration
	//
	strcpy(buf, glConfig.renderer_string );
	strlwr(buf);
	//
	// NOTE: if changing cvars, do it within this block.  This allows them
	// to be overridden when testing driver fixes, etc. but only sets
	// them to their default state when the hardware is first installed/run.
	//
	if (Q_stricmp(lastValidRenderer->string, glConfig.renderer_string)) {
		glConfig.hardwareType = GLHW_GENERIC;

		ri.Cvar_Set( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST" );

		// VOODOO GRAPHICS w/ 2MB
		if ( Q_stristr( buf, "voodoo graphics/1 tmu/2 mb")) {
			ri.Cvar_Set( "r_picmip", "2" );
			ri.Cvar_Get( "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
		} else {
			ri.Cvar_Set( "r_picmip", "1" );

			if ( Q_stristr( buf, "rage 128" ) || Q_stristr( buf, "rage128")) {
				ri.Cvar_Set( "r_finish", "0" );
			}
			// Savage3D and Savage4 should always have trilinear enabled
			else if ( Q_stristr( buf, "savage3d" ) || Q_stristr( buf, "s3 savage4" ) ) {
				ri.Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
			}
		}
	}

	//
	// this is where hardware specific workarounds that should be
	// detected/initialized every startup should go.
	//
	if ( Q_stristr(buf, "banshee" ) || Q_stristr(buf, "Voodoo_Graphics")) {
		glConfig.hardwareType = GLHW_3DFX_2D3D;
	} else if ( Q_stristr( buf, "rage pro") || Q_stristr(buf, "RagePro")) {
    glConfig.hardwareType = GLHW_RAGEPRO;
	} else if ( Q_stristr( buf, "permedia2")) {
    glConfig.hardwareType = GLHW_PERMEDIA2;
	} else if ( Q_stristr( buf, "riva 128")) {
    glConfig.hardwareType = GLHW_RIVA128;
	} else if ( Q_stristr( buf, "riva tnt ")) {
	}

	ri.Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );

	// initialize extensions
	GLW_InitExtensions();
	GLW_InitGamma();
	InitSig(); // not clear why this is at begin & end of function
	return;
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void) {
	// don't flip if drawing to front buffer
	if (stricmp(r_drawBuffer->string, "GL_FRONT" ) != 0) {
		sWindow->fView->UnlockGL();
		sWindow->fView->LockGL();
		sWindow->fView->SwapBuffers();
	}
	// check logging
	QGL_EnableLogging((qboolean)r_logFile->integer); // bk001205 - was ->value
}


void GLimp_RenderThreadWrapper( void *stub ) {}
qboolean GLimp_SpawnRenderThread( void (*function)( void ) ) {
	ri.Printf( PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n");
	return qfalse;
}
void *GLimp_RendererSleep( void ) {
	return NULL;
}
void GLimp_FrontEndSleep( void ) {}
void GLimp_WakeRenderer( void *data ) {}


/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

extern "C" void IN_Init(void) {
	Com_Printf ("\n------- Input Initialization -------\n");
	// mouse variables
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);
	in_dgamouse = Cvar_Get ("in_dgamouse", "1", CVAR_ARCHIVE);
	
	// turn on-off sub-frame timing of X events
	in_subframe = Cvar_Get ("in_subframe", "1", CVAR_ARCHIVE);
	
	// developer feature, allows to break without loosing mouse pointer
	in_nograb = Cvar_Get ("in_nograb", "0", 0);

	// bk001130 - from cvs.17 (mkv), joystick variables
	in_joystick = Cvar_Get ("in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH);
	// bk001130 - changed this to match win32
	in_joystickDebug = Cvar_Get ("in_debugjoystick", "0", CVAR_TEMP);
	joy_threshold = Cvar_Get ("joy_threshold", "0.15", CVAR_ARCHIVE); // FIXME: in_joythreshold

	if (in_mouse->value)
		mouse_avail = qtrue;
	else
		mouse_avail = qfalse;

	IN_StartupJoystick(); // bk001130 - from cvs1.17 (mkv)
	Com_Printf ("------------------------------------\n");
}

extern "C" void IN_Shutdown(void) {
	mouse_avail = qfalse;
}

extern "C" void IN_Frame (void) {
	// bk001130 - from cvs 1.17 (mkv)
	IN_JoyMove(); // FIXME: disable if on desktop?
	if (cls.keyCatchers & KEYCATCH_CONSOLE) {
		// temporarily deactivate if not in the game and
		// running on the desktop
		// voodoo always counts as full screen
		if (Cvar_VariableValue ("r_fullscreen") == 0 && strcmp( Cvar_VariableString("r_glDriver"), _3DFX_DRIVER_NAME)) {
			IN_DeactivateMouse ();
			return;
		}
	}
	IN_ActivateMouse();
}

void IN_Activate(void)
{
}

// bk001130 - cvs1.17 joystick code (mkv) was here, no linux_joystick.c

extern "C" void Sys_SendKeyEvents (void) {
	//Com_Printf("Sys_SendKeyEvents()\n");
	return;//Todo Jens
/*
	static	key_info keys_old;
	key_info keys_new;
	int key;
	int t = 0;
	if (!sWindow->IsActive())
		return;
	if(get_key_info(&keys_new) == B_OK)	{
		for(key = 0; key < 128; key++) {
			if(KEY_CHANGED(key)) {
				if(KEY_PRESSED(key)) {
					Sys_QueEvent(t, SE_KEY, scantokey[key], qtrue, 0, NULL );
				} else {
					Sys_QueEvent(t, SE_KEY, scantokey[key], qfalse, 0, NULL );
				}
			}
		}
		memcpy(&keys_old, &keys_new, sizeof(key_info));
	}
*/
}
