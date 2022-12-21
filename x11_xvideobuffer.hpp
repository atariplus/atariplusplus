/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_xvideobuffer.hpp,v 1.3 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

#ifndef X11_XVIDEOBUFFER_HPP
#define X11_XVIDEOBUFFER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "chip.hpp"
#include "colorentry.hpp"
#include "stdio.hpp"
#include "screendump.hpp"
#ifndef X_DISPLAY_MISSING
#if HAVE_X11_EXTENSIONS_XVLIB_H && HAVE_XVLISTIMAGEFORMATS && HAVE_XVQUERYADAPTORS && HAVE_XVQUERYEXTENSION && HAS_SYS_SHM_H && HAVE_SHMAT && HAVE_SHMCTL && HAVE_SHMDT && HAS_SYS_IPC_H
#define X_USE_XV
#include "x11_displaybuffer.hpp"
#include <X11/Xlib.h>
///

/// Forward references
class ArgParser;
class Monitor;
class XFront;
///

/// Class X11_XVideoBuffer
// This class keeps the memory for the X11 interface
// and computes draw-commands from display changes.
// This version uses the X11 XVideo extension to draw directly
// to the memory buffer without requiring an indirect pixbuf
// rendering, hopefully allowing faster rendering.
class X11_XVideoBuffer : public X11_DisplayBuffer {
  //
  // The way how we pack colors into 24 bits.
  typedef AtariDisplay::PackedRGB PackedRGB;
  //
  // Formats we support.
  enum VideoFormats {
    YUY2 = 0x32595559,
    UYVY = 0x59565955
  };
  //
  // The video buffer primitive. This is a pImpl.
  struct XVideoShMem *VideoMem;
  //
  bool indexdirty;
  bool enforcefullrefresh;
  //
  // Primary and secondary buffer
  PackedRGB          *active;      // screen we are currently rendering into
  PackedRGB          *last;        // last screen we used for rendering
  PackedRGB          *row;         // row we currently fill into
  PackedRGB          *lastrow;     // pointer in the last active buffer
  // Indexed / colormapped buffers for old-fashioned access
  UBYTE              *idxactive;
  UBYTE              *idxrow;
  //
public:
  // Methods required for constructing and destrucing us
  X11_XVideoBuffer(class Machine *mach, class XFront *front);
  ~X11_XVideoBuffer(void);
  //
  // Connect to the X11 window system
  virtual void ConnectToX(Display *display,Screen *screen,Window window,Colormap cmap,
			  LONG leftedge,LONG topedge,LONG width,LONG height,
			  LONG pxwidth,LONG pxheight,bool indirect);
  //
  // Unlink from the X11 window system again
  virtual void DetachFromX(void);
  //
  // Make a screen dump as .ppm file
  virtual void DumpScreen(FILE *file,ScreenDump::GfxFormat format);
  //
  // Rebuild the screen. If differential is set, then only an incremental
  // update to the last screen is made. Otherwise, a full screen
  // rebuild is made.
  virtual void RebuildScreen(bool differential=false);
  //
  // Return the currently active buffer and swap buffers.
  virtual UBYTE *NextBuffer(void);
  //
  // Return the currently active screen buffer
  virtual UBYTE *ActiveBuffer(void);
  //
  // In case of a window exposure event, try to rebuild the window
  // contents. This works only if we use an indirect drawing method
  // using a pixmap. Otherwise, we ignore the event as we refresh
  // the screen next time anyhow.
  virtual void HandleExposure(void);
  //
  // Methods imported from the chip class
  //
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // 
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Return the next output buffer for the next scanline to generate.
  // This either returns a temporary buffer to place the data in, or
  // a row in the final output buffer, depending on how the display generation
  // process works.
  virtual UBYTE *NextScanLine(void)
  {
    return idxrow;
  }
  virtual PackedRGB *NextRGBScanLine(void)
  {
    return row;
  }
  //
  // Push the output buffer back into the display process. This signals that the
  // currently generated display line is ready.
  virtual void PushLine(UBYTE *,int);
  // Ditto for true color.
  virtual void PushRGBLine(PackedRGB *,int); 
  //
  // Signal the requirement for refresh in the indicated region. This is only
  // used by the GUI engine as the emulator core uses PushLine for that.
  // Not required here... X11 will find out itself.
  virtual void SignalRect(LONG leftedge,LONG topedge,LONG width,LONG height);
  //  
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void);
};
///

///
#endif
#endif // of if HAVE_LIBX11
#endif
