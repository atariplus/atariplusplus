/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_displaybuffer.hpp,v 1.35 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

#ifndef X11_DISPLAYBUFFER_HPP
#define X11_DISPLAYBUFFER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "chip.hpp"
#include "colorentry.hpp"
#include "stdio.hpp"
#include "screendump.hpp"
#include "display.hpp"
#ifndef X_DISPLAY_MISSING
#include <X11/Xlib.h>
///

/// Forward references
class ArgParser;
class Monitor;
class XFront;
///

/// Class X11_DisplayBuffer
// This class keeps the memory for the X11 interface
// and computes draw-commands from display changes
class X11_DisplayBuffer : public Chip {
  //
protected:
  //  
  // Pointer to the front-end that uses us.
  class XFront                  *xfront;
  //
  // The colormap defines how each atari color has to be mapped
  // to an RGB value.
  const struct ColorEntry       *colormap;
  // 
  // Display, screen and window we render to
  Display                       *display;
  Screen                        *screen;
  Window                         window;  
  // The colormap of this window should we have allocated it.
  Colormap                       cmap;
  //
  // in case we use an intermediate pixmap, it is here.  
  Pixmap                         pixmap;
  //  
  //
  GC                             pixmapcontext; 
  //
  // Preferences
  //
  // use a pixmap for rendering
  bool                           PixMapIndirect;
  //
  // Modulo, width and height of the display buffer
  int                            modulo;
  //
  // Size multiplier for the window rendering size
  LONG                           PixelWidth,PixelHeight;
  //
  // Modulo, width and height of the display buffer
  LONG                           LeftEdge,TopEdge,Width,Height;
  //
  // True in case the window is mapped.
  bool                           mapped;
  //  
  //
  // Connect to the X11 window system
  void SetupX(Display *display,Screen *screen,Window window,Colormap cmap,
	      LONG leftedge,LONG topedge,LONG width,LONG height,
	      LONG pxwidth,LONG pxheight,bool indirect);
  //
  // Unlink from the X11 window system again
  void CloseX(void);
  //
public:
  // Methods required for constructing and destrucing us
  X11_DisplayBuffer(class Machine *mach, class XFront *front);
  ~X11_DisplayBuffer(void);
  //  
  // Connect to the X11 window system
  virtual void ConnectToX(Display *display,Screen *screen,Window window,Colormap cmap,
			  LONG leftedge,LONG topedge,LONG width,LONG height,
			  LONG pxwidth,LONG pxheight,bool indirect) = 0;
  //
  // Unlink from the X11 window system again
  virtual void DetachFromX(void) = 0;
  //
  // Make a screen dump as .ppm file
  virtual void DumpScreen(FILE *file,ScreenDump::GfxFormat format) = 0;
  //
  // Rebuild the screen. If differential is set, then only an incremental
  // update to the last screen is made. Otherwise, a full screen
  // rebuild is made.
  virtual void RebuildScreen(bool differential=false) = 0;
  //
  // Return the dimensions of the buffer array
  void BufferDimensions(LONG &leftedge,LONG &topedge,LONG &width,LONG &height,LONG &mod)
  {
    leftedge = LeftEdge;
    topedge  = TopEdge;
    width    = Width;
    height   = Height;
    mod      = modulo;
  }
  //
  // Return the currently active buffer and swap buffers.
  virtual UBYTE *NextBuffer(void) = 0;
  //
  // Return the currently active screen buffer
  virtual UBYTE *ActiveBuffer(void) = 0;
  //
  // In case of a window exposure event, try to rebuild the window
  // contents. This works only if we use an indirect drawing method
  // using a pixmap. Otherwise, we ignore the event as we refresh
  // the screen next time anyhow.
  virtual void HandleExposure(void) = 0;
  //
  // Methods imported from the chip class
  //
  virtual void ColdStart(void) = 0;
  virtual void WarmStart(void) = 0;
  //
  virtual void DisplayStatus(class Monitor *mon) = 0;
  //
  // The following is implemented as a dummy method
  // because we are not allowed to parse off any arguments.
  virtual void ParseArgs(class ArgParser *args);
  //
  // For GUI frontends within this buffer: Get the position and the status
  // of the mouse within this buffer measured in internal coordinates.
  void MousePosition(LONG &x,LONG &y,bool &button);  
  // 
  // Set the mouse at the indicated position.
  void SetMousePosition(LONG x,LONG y);
  //
  // Return the next output buffer for the next scanline to generate.
  // This either returns a temporary buffer to place the data in, or
  // a row in the final output buffer, depending on how the display generation
  // process works.
  virtual UBYTE *NextScanLine(void) = 0;
  //
  // The same for RGB/True color lines.
  virtual AtariDisplay::PackedRGB *NextRGBScanLine(void)
  {
    return NULL;
  }
  //
  // Push the output buffer back into the display process. This signals that the
  // currently generated display line is ready.
  virtual void PushLine(UBYTE *,int) = 0;
  //  
  virtual void PushRGBLine(AtariDisplay::PackedRGB *,int)
  {
  } 
  //
  // Signal the requirement for refresh in the indicated region. This is only
  // used by the GUI engine as the emulator core uses PushLine for that.
  // Not required here... X11 will find out itself.
  virtual void SignalRect(LONG,LONG,LONG,LONG)
  {
  }
  //
  //
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void) = 0;
};
///

///
#endif // of if HAVE_LIBX11
#endif


  
  
