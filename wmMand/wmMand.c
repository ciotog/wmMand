/*
 *	 wmMand-1.3.3 (C) 1999-2011 Mike Henderson (mghenderson@lanl.gov)
 *
 *	- Mandelbrot explorer
 *
 *
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program (see the file COPYING); if not, write to the
 *	Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 *		Boston, MA	02111-1307, USA
 *
 *
 * ToDo:
 *		- Activate Julia-set map button. Currently it bugs.
 *		- Colors on 8-bit displays are not yet working properly.
 *		- Tooltip giving information about the current view
 *		- Add support for non-square large images
 *		- Antialiasing for the large image
 *
 *
 * Version 1.0	- initial release, Feb. 15, 1999.
 * Version 1.1	- update by Stonehead <pspiertz@sci.kun.nl>
 *		See Debian changelog, Apr. 13, 2002.
 * Version 1.2	- update by ciotog <wmmand@ciotog.net>
 *		Feb 2005
 * Version 1.2.1 - update by ciotog <wmmand@ciotog.net>
 *		Mar 2006
 * Version 1.3 - update by ciotog <wmmand@ciotog.net>
 *		Apr 2006
 * Version 1.3.1 - update by ciotog <wmmand@ciotog.net>
 *		Sept 2006
 * Version 1.3.2 - update by ciotog <wmmand@ciotog.net>
 *		Mar 2007
 * Version 1.3.3 - update by ciotog <wmmand@ciotog.net>
 * 		Apr 2011
 */

/*
 * includes
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/X.h>
#include <err.h>

#define XK_LATIN1 1
#define XK_MISCELLANY 1
#include <X11/keysymdef.h>

#include <X11/xpm.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../wmgeneral/wmgeneral.h"
#include "Rainbow1.h"
#include "Rainbow2.h"
#include "PurpleWhite.h"
#include "BlueYellowRed.h"
#include "wmMand_master.xpm"
#include "wmMand_mask.xbm"

typedef struct {
	Display *display;
	int screen;
	Visual *visual;
	int depth;
	Colormap cmap;
	int format;
	int bitmap_pad;
	int Color[256];
	unsigned char RRR[256];
	unsigned char GGG[256];
	unsigned char BBB[256];
} DisplayInfo;

/* Determine the image viewer used to view the large image */
typedef enum {
	LARGEVIEWER_IM, LARGEVIEWER_XV
} LargeViewer;

#define WMMAND_VERSION "1.3.3"

void ParseCMDLine(int argc, char *argv[]);
void ButtonPressEvent(XButtonEvent *xev, DisplayInfo *info);
int KeyPressEvent(XKeyEvent *xev, DisplayInfo *info);
void ZoomEvent(XButtonEvent *lastEvent);
void ComputeImage(double X, double Y, int width, int height, double range, unsigned char *image);
void ComputeJulia(double X, double Y, int width, int height, double range, unsigned char *image);
void ViewLargeImage(double X, double Y, int width, int height, double range, DisplayInfo *info);
void LaunchFractalViewer(double X, double Y, int width, int height, double range);
void SetColorTable(DisplayInfo *info, int tableNumber);

const int numPalettes = 4;
const int nIterList = 8;
const int iterList[] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
unsigned int iterations = 2;
const int smallImageSize = 56;
const int halfSmallImageSize = 28;
unsigned int largeImageSize = 540;
unsigned int autoZoomDelay = 10;
/* Delay between refreshes (in seconds, microseconds) */
struct timespec delay = { 0, 10000000 };

int buttonsUp = False;
int buttonBarEvent = False;
int fractalType = 1;
long double range = 2.0;
long double zoom = 1.2;
long double centerX = 0.0;
long double centerY = 0.0;
int colorTable;
unsigned int maxIterations;
LargeViewer largeViewProg = LARGEVIEWER_IM;

int WriteGIF();


/*
 *	 main
 */
int main(int argc, char *argv[]) {

	XImage *xim;
	XEvent event, lastEvent;
	int i, j;
	unsigned int timer = 0;
	unsigned int nextUpdate = autoZoomDelay;
	int cursorHidden = False;
	int focused = False;
	int forceUpdate = True;
	unsigned char *image;
	DisplayInfo info;
	Cursor blankCursor;
	Pixmap blankPixmap;
	const char blankPixmapData = 0x00;
	XColor blackColor;

	maxIterations = iterList[iterations]; /* default is 256 iterations */

	/* Parse any command line arguments. */
	ParseCMDLine(argc, argv);

	/* Open window */
	openXwindow(argc, argv, wmMand_master_xpm, wmMand_mask_bits, wmMand_mask_width, wmMand_mask_height);

	/* Get Display parameters */
	info.display = display;
	info.screen = DefaultScreen(display);
	info.visual = DefaultVisual(display, info.screen);
	info.depth = DefaultDepth(display, info.screen);
	info.cmap = DefaultColormap(display, 0);

	/* Initialize Color Table */
	colorTable = 3;
	SetColorTable(&info, colorTable);

	xim = XCreateImage(info.display, info.visual, info.depth, info.format, 0, (char *)0, smallImageSize, smallImageSize, info.bitmap_pad, 0);
	xim->data = (char *)malloc(xim->bytes_per_line * smallImageSize);

	/* create blank cursor for hiding when using keyboard */
	blackColor.pixel = 0;	blackColor.red = 0;
	blackColor.blue = 0; blackColor.green = 0;
	blankPixmap = XCreateBitmapFromData(display, iconwin, &blankPixmapData, 1, 1);
	blankCursor = XCreatePixmapCursor(display, blankPixmap, blankPixmap, &blackColor, &blackColor, 0, 0);

	/* Loop until we die */
	while ( True ) {

		/* keep track of time */
		++timer;

		/* autozoom! */
		if ( ( timer + autoZoomDelay > nextUpdate ) && ( lastEvent.type == ButtonPress || lastEvent.type == FocusIn )
				&& buttonBarEvent == False ) {
			nextUpdate += autoZoomDelay;
			ZoomEvent(&lastEvent.xbutton);
			forceUpdate = True;
		}

		if ( lastEvent.type == KeyRelease && ( timer + autoZoomDelay > nextUpdate ) ) {
			if ( cursorHidden ) { /* unhide cursor */
				XUndefineCursor(display, event.xkey.window);
				cursorHidden = False;
			}
		}

		/* Process any pending X events */
		while( XPending(display) ) {
			XNextEvent(display, &event);
			lastEvent = event;

			switch( event.type ) {
				case Expose:
						RedrawWindow();
						break;
				case ButtonPress:
				        printf("wmMand: button pressed\n");
						if ( cursorHidden ) { /* unhide cursor */
							XUndefineCursor(display, event.xbutton.window);
							cursorHidden = False;
						}
						/* Attempt to confine the pointer */
						XGrabPointer(display, event.xbutton.window, True, 0, GrabModeAsync, GrabModeAsync, event.xbutton.window, None, CurrentTime);
						ButtonPressEvent(&event.xbutton, &info);
						timer = 0;
						nextUpdate = timer + 2 * autoZoomDelay; /* pause before autozoom */
						if ( !focused ) {
//						    XGrabKeyboard(display, event.xbutton.window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
							XSetInputFocus(display, event.xcrossing.window, RevertToPointerRoot, CurrentTime);
							focused = True;
						}
						forceUpdate = True;
						break;
				case ButtonRelease:
						XUngrabPointer(display, CurrentTime);
						break;
				case KeyPress:
						if ( !cursorHidden ) { /* hide cursor */
							XUndefineCursor(display, event.xkey.window);
							XDefineCursor(display, event.xkey.window, blankCursor);
							cursorHidden = True;
						}
						if ( !KeyPressEvent(&event.xkey, &info) ) {
							printf("wmMand: unhandled keypress, passing to root\n");
							XSendEvent(display, Root, True, KeyPressMask, &event);
						}
						forceUpdate = True;
						break;
				case EnterNotify:
						printf("wmMand: pointer entered window\n");
						if (info.depth == 8) XInstallColormap(display, info.cmap);
						break;
				case FocusOut:
						printf("wmMand: focus out\n");
						focused = False;
						break;
				case LeaveNotify:
						printf("wmMand: pointer left window\n");

						if ( cursorHidden ) { /* unhide cursor */
							XUndefineCursor(display, event.xcrossing.window);
							cursorHidden = False;
						}
						if ( info.depth == 8 ) XUninstallColormap(display, info.cmap);
						break;
			}
		}

		if ( forceUpdate ) {
			/* allocate temp memory for Image */
			image = (unsigned char *) malloc(sizeof(unsigned char) * smallImageSize * smallImageSize);
			
			if ( image == NULL ) {
				fprintf(stderr, "wmMand: Unable to allocate memory for small image\n");
				exit(-1);
			}

			/* create image */
			if ( fractalType == 1 )
				ComputeImage(centerX, centerY, smallImageSize, smallImageSize, range, image);
			else
				ComputeJulia(centerX, centerY, smallImageSize, smallImageSize, range, image);

			/* Clear window. */
			copyXPMArea(70, 70, smallImageSize, smallImageSize, 4, 4);

			/* Paste up image. */
			for ( i = 0; i < smallImageSize; ++i ) {
				for ( j = 0; j < smallImageSize; ++j ) {
					XPutPixel(xim, i, j,	info.Color[*(image + j * smallImageSize + i)]);
					XFlush(display);
				}
			}

			XPutImage(display, wmgen.pixmap, NormalGC, xim, 0, 0, 4, 4, smallImageSize, smallImageSize);

			/* Paste up buttons if buttonsUp is toggled. */
			if ( buttonsUp ) {

				/* paste button bar */
				copyXPMArea(4, 69, smallImageSize, 10, 4, 50);

				/* paste maxiterations button */
				switch( maxIterations ) {
					case 64:
						copyXPMArea(75, 69, 9, 6, 19, 52);
						break;
					case 128:
						copyXPMArea(71, 77, 13, 6, 17, 52);
						break;
					case 256:
						copyXPMArea(70, 85, 14, 6, 17, 52);
						break;
					case 512:
						copyXPMArea(71, 93, 13, 6, 17, 52);
						break;
					case 1024:
						copyXPMArea(67, 101, 17, 6, 15, 52);
						break;
					case 2048:
						copyXPMArea(87, 69, 18, 6, 14, 52);
						break;
					case 4096:
						copyXPMArea(87, 77, 18, 6, 14, 52);
						break;
					case 8192:
						copyXPMArea(87, 85, 18, 6, 14, 52);
						break;
				}
			}

			RedrawWindow(); /* Make changes visible */
			free(image); /* Free Image Memory */
		}

		forceUpdate = 0; /* Clear forceUpdate Flag */
		waitpid(-1, NULL, WNOHANG); /* clean up any children */
		nanosleep(&delay, NULL); /* Wait for next update */
	}

	/* Quit program	(this code is never reached..) */
	free(xim->data);
	XDestroyImage(xim);
	XFreeCursor(display,blankCursor);
}


/*
 *	 ParseCMDLine()
 */
void ParseCMDLine(int argc, char *argv[]) {
	int i, nIter;

	for ( i = 1; i < argc; i++ ) {

		if ( !strcmp(argv[i], "--zoom") || !strcmp(argv[i], "-z") ) {
			/* set zoom level (no checks for reasonableness) */
			zoom = atof(argv[++i]);
		} else if ( !strcmp(argv[i], "--iterations") || !strcmp(argv[i], "-i") ) {
			/* set initial max iterations. If invalid value given, use default */
			nIter = atof(argv[++i]);
			switch ( nIter ) {
				case 64:
						iterations = 0; 
						break;
				case 128:
						iterations = 1;
						break;
				case 256:
						iterations = 2;
						break;
				case 512:
						iterations = 3;
						break;
				case 1024:
						iterations = 4;
						break;
				case 2048:
						iterations = 5;
						break;
				case 4096:
						iterations = 5;
						break;
				case 8192:
						iterations = 6;
						break;
			}
			maxIterations = iterList[iterations];
		} else if ( !strcmp(argv[i], "--largesize") || !strcmp(argv[i], "-l") ) {
			/* set size of large image */
			largeImageSize = atof(argv[++i]);
		} else if ( strcmp(argv[i], "-display") && ( !strcmp(argv[i], "--delayzoom") || !strcmp(argv[i], "-d") ) ) {
			/* set autozoom delay */
			autoZoomDelay = atof(argv[++i]);
		} else if ( !strcmp(argv[i], "--xv") || !strcmp(argv[i], "-x") ) {
			/* set display program to xv */
			largeViewProg = LARGEVIEWER_XV;
		} else {
			/* print usage string
			 * note that \E[1m turns on bold type, \E[m turns it off
			 */
			printf("\nwmMand version %s\n", WMMAND_VERSION);
			printf("\nUsage: wmMand [OPTION] ...\n\n");
			printf("Options:\n");
			printf("\t\E[1m-z, --zoom <factor>\E[m\n\t\tset zoom factor (default is %.1Lf)\n", zoom);
			printf("\t\E[1m-i, --iterations <number>\E[m\n\t\tset initial max iterations (64, 128, 256 (default), 512, 1024 or 2048)\n");
			printf("\t\E[1m-l, --largesize <number>\E[m\n\t\tset size of large image (square) (default is %d)\n", largeImageSize);
			printf("\t\E[1m-d, --delayzoom <number>\E[m\n\t\tset delay for autozooming with mouse (default %d, larger values give longer delay). Dependent on CPU cycle availability\n", autoZoomDelay);
			printf("\t\E[1m-x, --xv\E[m\n\t\tuse xv to display large image instead of ImageMagic's display program\n");
			printf("\t\E[1m-h, --help\E[m\n\t\tdisplay help screen\n");
			exit(1);
		}
	}
}


/*
 *	This routine handles key presses.
 *  Return value is 'True' if the key press was handled, 'False' otherwise
 */
int KeyPressEvent(XKeyEvent *xev, DisplayInfo *info) {

	if ( ( xev->keycode == XKeysymToKeycode(display, XK_z) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_Z) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_plus) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Add) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_5) ) ) {
		/* z, + or 5 key on numberpad: zoom in */
		range *= 1.0 / zoom;
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_o) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_O) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_minus) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Subtract) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_0) ) ) {
		/* o, - key or 0 on numberpad: zoom out */
		range *= zoom;
	}

	/* Arrow keys shift the center point with the zoom level as the shift factor */
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Up) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Up) ) ) {
 		/* up arrow or 8 on numberpad: shift up */
		centerY -= ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Down) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Down) ) ) {
 		/* down arrow or 2 on numberpad: shift down */
		centerY += ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Left) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Left) ) ) {
 		/* left arrow or 4 on numberpad: shift left */
		centerX -= ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Right) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Right) ) ) {
 		/* right arrow or 6 on numberpad: shift right */
		centerX += ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Home) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Home) ) ) {
 		/* Home or 7 on numberpad: shift up and left */
		centerY -= ( zoom - 1.0 ) * range;
		centerX -= ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Page_Up) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Page_Up) ) ) {
 		/* PgUp or 9 on numberpad: shift up and right */
		centerY -= ( zoom - 1.0 ) * range;
		centerX += ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Page_Down) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_Page_Down) ) ) {
 		/* PgDn or 3 on numberpad: shift down and right */
		centerY += ( zoom - 1.0 ) * range;
		centerX += ( zoom - 1.0 ) * range;
 	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_End) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_KP_End) ) ) {
 		/* End or 1 on numberpad: shift down and left */
		centerY += ( zoom - 1.0 ) * range;
		centerX -= ( zoom - 1.0 ) * range;
	}

	/* General keystrokes follow */

	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_c) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_C) ) ) {
		/* c key: change color table */
		colorTable = ( colorTable + 1 ) % numPalettes;
		SetColorTable(info, colorTable);
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_i) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_I) ) ) {
		/* i key: change iterations level */
		iterations = ( iterations + 1 ) % nIterList;
		maxIterations = iterList[iterations];
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_r) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_R) ) ) {
		/* r key: reset view to default */
		centerX = 0.0;
		centerY = 0.0;
		range = 2.0;
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_b))
			|| ( xev->keycode == XKeysymToKeycode(display, XK_B) ) ) {
		/* b key: toggle button bar */
		buttonsUp = !buttonsUp;
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_v) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_V) ) ) {
		/* v key: view big image with ImageMagic or other viewer */
		ViewLargeImage(centerX, centerY, largeImageSize, largeImageSize, range, info);
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_Escape) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_q) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_Q) ) ) {
		/* Esc or q key: try to release mouse grab, in case it's not releasing otherwise */
		XUngrabPointer(display, CurrentTime);
	}
	else if ( ( xev->keycode == XKeysymToKeycode(display, XK_x) )
			|| ( xev->keycode == XKeysymToKeycode(display, XK_X) ) ) {
		/* x key: launch fractal program XaoS */
		LaunchFractalViewer(centerX, centerY, largeImageSize, largeImageSize, range);
	}
	
	else {
		/* keypress wasn't handled */
		return False;
	}

	/* keypress was handled */
	return True;
}


/*
 *	This routine handles button presses.
 *
 * - Left Mouse single click: make cursor position the new image center an zooms in
 * - Right Mouse single click: make cursor position the new image center an zooms out
 * - Middle Mouse single click: toggle the button bar
 */
void ButtonPressEvent(XButtonEvent *xev, DisplayInfo *info) {

	if ( buttonsUp && ( ( xev->x > 3 ) && ( xev->x < 60 ) && ( xev->y > 49 ) && ( xev->y < 60 ) ) ) {
		/* button bar event */
		buttonBarEvent = True;

		if ( xev->x < 13 ) {
			/* C button: change color table */
			if ( xev->button == Button1 )
				colorTable = (colorTable + 1) % numPalettes;
			else if ( xev->button == Button3 )
				colorTable = (colorTable + numPalettes - 1) % numPalettes;
			SetColorTable(info, colorTable);
		} else if ( xev->x < 34 ) {
			/* number button: change number of iterations */
			if ( xev->button == Button1 )
				iterations = (iterations + 1) % nIterList;
			else if ( xev->button == Button3 )
				iterations = (iterations + nIterList - 1) % nIterList;
			maxIterations = iterList[iterations];
		} else if ( xev->x < 42 ) {
			/* M button: not yet defined */
		} else if ( xev->x < 50 ) {
			/* R button: reset */
			centerX = 0.0;
			centerY = 0.0;
			range = 2.0;
		} else {
			/* V button: view large image */
			ViewLargeImage(centerX, centerY, largeImageSize, largeImageSize, range, info);
		}
		return;
	} else if ( ( xev->x > 3 ) && ( xev->x < 60 ) && ( xev->y > 3 ) && ( xev->y < 60 ) ) {
		buttonBarEvent = False;
	} else {
		/* Click was on border */
		return;
	}

	if ( xev->button == Button2 ) {
		/* middle mouse button pressed: show/hide buttons */
		buttonsUp = !buttonsUp;
	} else if ( !buttonBarEvent ) {
		/* Right or left button pressed or scrollwheel, zoom in or out */
		ZoomEvent(xev);
	}
}


/*
 *	This routine handles zooming with the mouse
 *
 * - Left Mouse button zooms in
 * - Right Mouse button zooms out
 * - Middle Mouse button does nothing
 */
void ZoomEvent(XButtonEvent *lastEvent) {

	int	x, y;
	double X, Y;

	/* dummy variables for the XQueryPointer call */
	Window rr, cr;
	int rx, ry;
	unsigned int mr;

	XQueryPointer(display, iconwin, &rr, &cr, &rx, &ry, &x, &y, &mr);

	/* offset from 4px border */
	x -= 4;
	y -= 4;

	if ( lastEvent->button == Button1 ) {
		/*
		 * left mouse button pressed: zoom in
		 *	 compute the physical (X, Y) values of the point defined by the image (x, y)
		 *	 recenter on the point half way from center to click
		 */
		X = centerX + range * ((double) x / halfSmallImageSize - 1);
		Y = centerY + range * ((double) y / halfSmallImageSize - 1);
		range *= 1.0 / zoom;
		X -= range * ((double) x / halfSmallImageSize - 1);
		Y -= range * ((double) y / halfSmallImageSize - 1);
		centerX = X;
		centerY = Y;
	}
	else if ( lastEvent->button == Button3 ) {
		/* right mouse button pressed: zoom out
		 *	 compute the physical (X, Y) values of the point defined by the image (x, y)
		 *	 recenter on point a little more than half way from center to click
		 */
		X = centerX + 1.1 * range * ( (double) x / halfSmallImageSize - 1 );
		Y = centerY + 1.1 * range * ( (double) y / halfSmallImageSize - 1 );
		range *= zoom;
		X -= range * ( (double) x / halfSmallImageSize - 1 );
		Y -= range * ( (double) y / halfSmallImageSize - 1 );
		centerX = X;
		centerY = Y;
	}
	else if ( lastEvent->button == Button4 ) {
		/* scroll wheel up: zoom in */
		range *= 1.0 / zoom;
	}
	else if ( lastEvent->button == Button5 ) {
		/* scroll wheel down: zoom out */
		range*= zoom;
	}
}


/*
 *	 This routine computes the Mandelbrot fractal Image center on (X, Y)
 *	 of the current Image. 
 */
void ComputeImage(double X, double Y, int width, int height, double range, unsigned char *image) {

	int *intImage;
	int i, j, done, n, nMin, nMax;
	int nCount, count, *hist, cutoff;
	long double f, re, im;
	long double a, b, d, a2, b2;
	long double nRange, uval, halfWidth, halfHeight;

	/* Initialize hist[] */
	hist = (int *) malloc(sizeof(int) * maxIterations);

	if ( hist == NULL ) {
		fprintf(stderr, "wmMand: unable to allocate memory for hist\n");
		return;
	}

/* TODO: use memset */
	for ( i = 0; i < maxIterations; ++i ) hist[i] = 0;

	/* allocate memory for intImage */
	intImage = (int *) malloc(sizeof(int) * width * height);
 
	if ( intImage == NULL ) {
		fprintf(stderr, "wmMand: unable to allocate memory for image\n");
		free(hist);
		return;
	}
 
	/* compute the (integer) map first */
	nMin = 9999, nMax = -9999, nCount = 0;
	halfWidth = width / 2.0;
	halfHeight = height / 2.0;
	f = range / halfWidth;

	for ( i = 0; i < width; ++i ) {
		re = f * ( (long double) i - halfWidth ) + X;

		for ( j = 0; j < height; ++j ) {
			im = f * ( (long double) j - halfHeight ) + Y;
			n = 0; a = b = 0.0;
			a2 = 0.0; b2 = 0.0;

			while ( ( n < maxIterations ) && ( ( a2 + b2 ) < 4.0 ) ) {
				d = a;
				a = a2 - b2 + re;
				b = 2.0 * d * b + im;
				a2 = a * a; b2 = b * b;
				++n;
			}

			if ( n > nMax ) nMax = n;
			if ( n < nMin ) nMin = n;

			if ( n >= maxIterations ) {
				*( intImage + height*j + i ) = 0;
			} else{
				*( intImage + height*j + i ) = n;
				++nCount;
			}

			++hist[*( intImage + height * j + i )];
		}
	}

	/*	 Figure out what nMax should be. The problem is that you might only get
	 *	 one or two points in the upper half of the given range. A quick and dirty
	 *	 method is to find out at what n the f(n) starts to just become outliers.
	 *	 define outliers as mostly zeros. e.g. say more than 50% zero in a stretch
	 *	 that is 10 contiguous values long. Or you could count backwards until you
	 *	 get to the 99.5% level and call that the cutoff. 
	 */
	i = nMax - 1, done = False, count = 0, cutoff = 0;

	while( (i > 0) && (done == False) ) {
		count += hist[i];
		if ( 40 * count >= nCount ) {
			done = True;
			cutoff = i;
		}
		--i;
	}
 
	/* 
	 *	 Then map the integer map (intImage) into a byte map (i.e. with 256 colours) 
	 */
	uval = 0, nRange = (double) (cutoff - nMin + 1);

	for ( i = 0; i < width; ++i ) {

		for ( j = 0; j < height; ++j ) {
			n = *( intImage + height * j + i );

			if ( n == 0 ) {
				uval = 0;
			}
			else if ( n >= cutoff ) {
				uval = 255;
			}
			else if ( n != 0 ) {
				uval = (unsigned char) ( (double) (n - nMin + 1) / nRange * 255 );

				/* make sure roundoff doesn't put uval at 0 when n is not 0 */
				if ( uval == 0 ) uval = 1;
			}

			*( image + height * j + i ) = uval;
		}
	}

	free(hist);
	free(intImage);
}


/*
 *	 This routine computes the Julia fractal Image center on (X, Y)
 *	 of the current Image. Ripped from xmand.c
 */
void ComputeJulia(double X, double Y, int width, int height, double range, unsigned char *image) {

int *intImage;
int i, j, done, n, nMin, nMax;
int nCount, count, *hist, cutoff;
long double f, re, im;
long double a, b, d, a2, b2;
long double nRange, uval, halfWidth, halfHeight;
/* CX, CY refer to the c parameter needed in the Julia set iterations. */
long double CX, CY;

	/* Initialize hist[] */
	hist = (int *) malloc(sizeof(int) * maxIterations);

	if ( hist == NULL ) {
		fprintf(stderr, "wmMand: unable to allocate memory for hist\n");
		return;
	}

/* TODO: change to memset */
	for ( i = 0; i < maxIterations; ++i ) hist[i] = 0;

	/* allocate memory for intImage */
	intImage = (int *) malloc(sizeof(int) * width * height);

	if ( intImage == NULL ) {
		fprintf(stderr, "wmMand: unable to allocate memory for image\n");
		free(hist);
		return;
	}
 
	/* compute the (integer) map first */
	nMin = 9999, nMax = -9999, nCount = 0;
	halfWidth = width / 2.0;
	halfHeight = height / 2.0;

	CX = range * ( (double) X - halfWidth ) / halfWidth;
	CY = range * ( (double) Y - halfWidth ) / halfWidth;

	f = range / halfWidth;
	for ( i = 0; i < width; ++i ) {

		re = f * ( (double) i - halfWidth ) + X;
		for ( j = 0; j < height; ++j ) {

			im = f * ( (double) j - halfHeight ) + Y;
			a = re;
			b = im;

			n = 0;
			a2 = a * a;
			b2 = b * b;

			while ( ( n < maxIterations ) && ( ( a2 + b2 ) < 4.0 ) ) {
				d = a;
				a = a2 - b2 + CX;
				b = 2.0 * d * b + CY;

				a2 = a * a; b2 = b * b;
				++n;
			}

			if ( n > nMax ) nMax = n;
			if ( n < nMin ) nMin = n;
			if ( n >= maxIterations ) {
				*( intImage + height*j + i ) = 0;
			} else {
				*( intImage + height*j + i ) = n;
				++nCount;
			}
			++hist[*( intImage + height*j + i )];
		}
	}

	/*	 Figure out what nMax should be. The problem is that you might only get
	 *	 one or two points in the upper half of the given range. A quick and dirty
	 *	 method is to find out at what n the f(n) starts to just become outliers.
	 *	 define outliers as mostly zeros. e.g. say more than 50% zero in a stretch
	 *	 that is 10 contiguous values long. Or you could count backwards until you
	 *	 get to the 99.5% level and call that the cutoff. 
	 */
	i = nMax - 1, done = False, count = 0, cutoff = 0;
	while( ( i > 0 ) && ( done == False ) ) {
		count += hist[i];
		if ( 40 * count > nCount ) {
			done = True;
			cutoff = i;
		}
		--i;
	}
 
	/* 
	 *	 Then map the integer map (intImage) into a byte map (i.e. with 256 colours) 
	 */
	uval = 0;
	nRange = (long double) ( cutoff - nMin + 1 );
	for ( i = 0; i < width; ++i ) {
		for ( j = 0; j < height; ++j ) {
			n = *( intImage + height * j + i );
			if ( n == 0 ) {
				uval = 0;
			}
			else if ( n >= cutoff ) {
				uval = 255;
			}
			else if ( n != 0 ) {

				/* 
				 *	make sure roundoff doesnt put index to 0. Otherwise it'll
				 *	look like its in the set! 
				 */
				uval = (unsigned char) ( (long double) ( n - nMin + 1.0 ) / nRange * 255 );
				*( image + height * j + i ) = ( uval == 0 ) ? 1 : uval;
			}
		}
	}
	free(hist);
	free(intImage);
}


/*
 * Creates a large version of the small window image and opens it in an image viewer
 */
void ViewLargeImage(double X, double Y, int width, int height, double range, DisplayInfo *info) {

	unsigned char *image;
	FILE *fp_gif;
	char *fp_command;
	pid_t fp_pid;

	/* allocate memory for image data */
	image = (unsigned char *) malloc(sizeof(unsigned char) * height * width);

	if ( image == NULL ) {
		fprintf(stderr, "wmMand: unable to allocate memory for large image\n");
		return;
	}

	/* allocate memory for the command string */
	fp_command = (char *) malloc(sizeof(char)*80);
	
	if ( fp_command == NULL ) {
		fprintf(stderr, "wmMand: unable to allocate memory for viewer command string\n");
		free(image);
		return;
	}
	
	/* create large fractal */
	if ( fractalType == 1 )
		ComputeImage(X, Y, height, width, range, image);
	else
		ComputeJulia(X, Y, height, width, range, image);

	/* Build view command 
	 * assume ImageMagic's display program, if nothing else specified */
	if ( largeViewProg == LARGEVIEWER_XV )
		sprintf(fp_command, "xv -name wmMand_Re%3.3E_Im%3.3E_Ra%3.3E.gif -", X, Y, range);
	else
		sprintf(fp_command, "display -title wmMand_Re%3.3E_Im%3.3E_Ra%3.3E.gif -", X, Y, range);

	printf("wmMand: large image launched with command: %s\n", fp_command);

	if ( (fp_pid = fork()) == -1 )
		fprintf(stderr, "wmMand: fork error\n");
	else {
		if ( fp_pid == (pid_t) 0 ) { // Child process, which just handles the large viewer
			if ( (fp_gif = popen(fp_command, "w")) < 0 ) {
				fprintf(stderr, "wmMand: error opening large viewer program\n");
				exit(EXIT_FAILURE);
			} else {
				WriteGIF(fp_gif, image, 0, width, height, info->RRR, info->GGG, info->BBB, 256, 0, "");
			}
			fclose(fp_gif);
			exit(EXIT_SUCCESS);
		}
	}

	/* free Image memory and command string */
	free(fp_command);
	free(image);
}


/* 
 * Launches XaoS (if installed)
 */
void LaunchFractalViewer(double centerX, double centerY, int width, int height, double range) {
	char *fp_command[11];
	pid_t fp_pid;
	int i, j;

	/* 
	 * allocate memory for the command and attribute strings
	 * the longest possible are the floating point values at 13 characters,
	 * eg. "-1.196006e+01"
	 */
	for ( i = 0; i < 10; ++i ) {
		fp_command[i] = (char *) malloc(sizeof(char)*25);
		
		if ( fp_command[i] == NULL ) {
			fprintf(stderr, "wmMand: Unable to allocate memory for XaoS command string\n");
			for ( j = 0; j < i; ++j )
				free(fp_command[j]);
			return;
		}
		
	}

	/* attribute list must end with a NULL-terminated string */
	fp_command[10] = (char *) NULL;

	/*
	 * build fractal program command
	 * XaoS takes 4 floating values for the -view parameter:
	 * centerX, centerY, rangeX, rangeY
	 * A precision of 15 should be good enough except at the most extreme zoom levels
	 */
	sprintf(fp_command[0], "xaos");
	sprintf(fp_command[1], "-size");
	sprintf(fp_command[2], "%dx%d", width, height);
	sprintf(fp_command[3], "-view");
	sprintf(fp_command[4], "%.15e", centerX);
	sprintf(fp_command[5], "%.15e", centerY);
	sprintf(fp_command[6], "%.15e", 2 * range);
	sprintf(fp_command[7], "%.15e", 2 * range);
	sprintf(fp_command[8], "-maxiter");
	sprintf(fp_command[9], "%d", iterList[iterations]);

	printf("wmMand: fractal viewer command:");
	for ( i = 0; i < 10; ++i )
		printf(" %s", fp_command[i]);
	printf("\n");

	if ( ( fp_pid = fork() ) == -1 )
		fprintf(stderr, "wmMand: fork error\n");
	else {
		if ( fp_pid == (pid_t) 0 ) { // Child process, which just handles the fractal viewer
			if ( execvp(fp_command[0], fp_command) < 0 ) {
				fprintf(stderr, "wmMand: unable to launch Xaos\n");
				exit(EXIT_FAILURE);
			}
			printf("wmMand: This code is never reached\n");
			exit(EXIT_SUCCESS);
		}
	}

	/* free Image memory and command string */
	for ( i = 0; i < 10; ++i )
		free(fp_command[i]);
}


/*
 * This routine sets or resets the color table
 */
void SetColorTable(DisplayInfo *info, int tableNumber) {

	XColor xColor, xColors[256];
	int i;

	switch ( tableNumber ) {
		case 0:
			for ( i = 0; i < 256; ++i ) {
				info->RRR[i] = Rainbow2_Red[i];
				info->GGG[i] = Rainbow2_Grn[i];
				info->BBB[i] = Rainbow2_Blu[i];
			}
			break;
		case 1:
			for ( i = 0; i < 256; ++i ) {
				info->RRR[i] = Rainbow1_Red[i];
				info->GGG[i] = Rainbow1_Grn[i];
				info->BBB[i] = Rainbow1_Blu[i];
			}
			break;
		case 2:
			for ( i = 0; i < 256; ++i ) {
				info->RRR[i] = PurpleWhite_Red[i];
				info->GGG[i] = PurpleWhite_Grn[i];
				info->BBB[i] = PurpleWhite_Blu[i];
			}
			break;
		case 3:
			for ( i = 0; i < 256; ++i ) {
				info->RRR[i] = BlueYellowRed_Red[i];
				info->GGG[i] = BlueYellowRed_Grn[i];
				info->BBB[i] = BlueYellowRed_Blu[i];
			}
			break;
	}

	/*
	 *	 Create an XImage with NULL data. Then allocate space for data. 
	 */
	info->format = ZPixmap;

	if ( info->depth == 8 ) {

		info->bitmap_pad = 8;
		/*
		 *	 Set a private colormap
		 */
		info->cmap = XCreateColormap(info->display, RootWindow(info->display, info->screen), info->visual, AllocAll);

		for ( i = 0; i < 256; ++i ) {
			info->Color[i] = i;
			xColors[i].pixel = i;
			xColors[i].red   = (unsigned short) info->RRR[i] << 8;
			xColors[i].green = (unsigned short) info->GGG[i] << 8;
			xColors[i].blue  = (unsigned short) info->BBB[i] << 8;
			xColors[i].flags = DoRed | DoGreen | DoBlue;
		}

		XStoreColors(info->display, info->cmap, xColors, 256);
		XSetWindowColormap(info->display, win, info->cmap);
	}
	else if ( info->depth > 8 ) {

		/* Allocate Colors */
		for ( i = 0; i < 256; ++i ) {
			xColor.red   = (unsigned short) info->RRR[i] << 8;
			xColor.green = (unsigned short) info->GGG[i] << 8;
			xColor.blue  = (unsigned short) info->BBB[i] << 8;
			xColor.flags = DoRed | DoGreen | DoBlue;
			XAllocColor(info->display, info->cmap, &xColor);
			info->Color[i] = xColor.pixel;
		}

		info->bitmap_pad = 32;
	}
	else {
		fprintf(stderr, "wmMand: Needs at least 8-bit display\n");
		exit(-1);
	}
}
