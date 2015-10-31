/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_displaybuffer.cpp,v 1.45 2015/10/18 16:48:16 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

/// Includes
#include "types.h"
#include "monitor.hpp"
#include "argparser.hpp"
#include "x11_displaybuffer.hpp"
#include "xfront.hpp"
#include "gtia.hpp"
#include "antic.hpp"
#include "screendump.hpp"
#include "exceptions.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "new.hpp"
#ifndef X_DISPLAY_MISSING
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int X11_DisplayBuffer::RenderBufferSize = 16;
const int X11_DisplayBuffer::ScanBuffNum      = 256;
#endif
///

/// X11_DisplayBuffer::X11_DisplayBuffer
X11_DisplayBuffer::X11_DisplayBuffer(class Machine *mach, class XFront *front)
  : Chip(mach,"X11DisplayBuffer"),
    xfront(front), colormap(NULL), 
    display(0), screen(0), window(0), cmap(0), pixmap(0),
    pixmapcontext(0),
    PixMapIndirect(false), mapped(false)
{
}
///

/// X11_DisplayBuffer::~X11_DisplayBuffer
X11_DisplayBuffer::~X11_DisplayBuffer(void)
{
  // Unlink us from the X system here.
  CloseX();  
}
///

/// X11_DisplayBuffer::CloseX
// Unlink the display buffer from the X system
void X11_DisplayBuffer::CloseX(void)
{
  // Check whether we have a pixmap. If so, dispose it.
  if (pixmap) {
    XFreePixmap(display,pixmap);
    pixmap = 0;
  }
  if (pixmapcontext) {
    XFreeGC(display,pixmapcontext);
    pixmapcontext = 0;
  }
  //
  // Now forget the display
  display = NULL;
  window  = 0;
  mapped  = false;
}
///

/// X11_DisplayBuffer::SetupX
// Connect to the X11 window system
void X11_DisplayBuffer::SetupX(Display *d,Screen *s,Window win,Colormap cm,
			       LONG le,LONG te,LONG w,LONG h,
			       LONG pxwidth,LONG pxheight,bool indirect)
{  
  UWORD m,i;
  //
#if CHECK_LEVEL > 2
  if (pixmap || pixmapcontext) {
    Throw(ObjectExists,"X11_DisplayBuffer::ConnectToX",
	  "The display buffer is already connected to the X system");
  }
#endif
  // Get window, colormap, screen, display
  colormap       = machine->GTIA()->ActiveColorMap();
  display        = d;
  screen         = s;
  window         = win;
  cmap           = cm;
  PixelWidth     = pxwidth;
  PixelHeight    = pxheight;
  LeftEdge       = le;
  TopEdge        = te;
  Width          = w;
  Height         = h;
  PixMapIndirect = indirect;
  //
  // Get dimensions from antic: We need it for the modulo
  machine->Antic()->DisplayDimensions(m,i); // throw i away
  modulo = m;
  //
  // Check whether the width is reasonable.
  if (Width > modulo) {
    Throw(OutOfRange,"X11_DisplayBuffer::ConnectToX",
	  "The requested width is wider than the display generated "
	  "by the Atari hardware.");
  }  
  //
  if (PixMapIndirect) {
    int depth;
    depth  = XDefaultDepthOfScreen(screen);
    pixmap = XCreatePixmap(display,window,Width*PixelWidth,Height*PixelHeight,depth);
    if (pixmap == 0) {
      PixMapIndirect = false;
    }
  }
}
///

/// X11_DisplayBuffer::MousePosition
// For GUI frontends within this buffer: Get the position and the status
// of the mouse within this buffer measured in internal coordinates.
void X11_DisplayBuffer::MousePosition(LONG &x,LONG &y,bool &button)
{    
  int rootx,rooty;
  int winx,winy;
  unsigned int mask;
  Window activeroot = 0,activechild = 0;
  //  
  // Ok, query the mouse state from the X server now
  if (XQueryPointer(display,window,&activeroot,&activechild,
		    &rootx,&rooty,&winx,&winy,
		    &mask)) {
    // Only handle if the active screen is our screen.
    // First, check the button state. If buttons 1 or 2
    // are pressed, take this as a button down.
    if (mask & (Button1Mask | Button2Mask)) {
      button = true;
    } else {
      button = false;
    }
    //
    // Do not check for out of range. Hopefully, the GUI will do this for us. Hmm...
    x = winx / PixelWidth;
    y = winy / PixelHeight;
  }
}
///

/// X11_DisplayBuffer::SetMousePosition
// Set the mouse at the indicated position.
void X11_DisplayBuffer::SetMousePosition(LONG x,LONG y)
{
  XWarpPointer(display,window,window,0,0,Width * PixelWidth,Height * PixelHeight,x * PixelWidth,y * PixelHeight);
}
///

/// X11_DisplayBuffer::ParseArgs
void X11_DisplayBuffer::ParseArgs(class ArgParser *)
{
}
///

///
#endif // of if HAVE_LIBX11
///
