/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xfront.hpp,v 1.19 2013/12/20 21:14:42 thor Exp $
 **
 ** In this module: Interface definition for a generic X11 frontend
 **********************************************************************************/

#ifndef XFRONT_HPP
#define XFRONT_HPP

/// Includes
#include "types.h"
#include "machine.hpp"
#include "chip.hpp"
#include "display.hpp"
#ifndef X_DISPLAY_MISSING
#include <X11/Xlib.h>
///

/// Forwards
class ArgParser;
class Monitor;
class X11_DisplayBuffer;
///

/// Class XFront
// This class defines the generic interface for all X11 like front-ends
class XFront : public AtariDisplay {
  //  
  // The display buffer of this class
  class X11_DisplayBuffer *FrameBuffer;
  //
  //
protected:
  // Some X11 specific pointers: Screen, Window, Display, Colormap
  Display                 *display;
  Screen                  *screen;
  Window                   window;
  Colormap                 colormap;
  //
  // Get and/or build the frame buffer for this class.
  class X11_DisplayBuffer *FrameBufferOf(bool truecolor = false,bool video = false);
  //
  // Unload the frame buffer, build a new one. This has to be called
  // in case GTIA changes its mood concerning the truecolor/mapped
  // property.
  void UnloadFrameBuffer(void);
  //
public:
  XFront(class Machine *mach,int unit);
  virtual ~XFront(void);
  //
  // Get some display dimensions
  virtual void BufferDimensions(LONG &le,   LONG &te,
				LONG &width,LONG &height,LONG &modulo);
  //
  //  
  // Return the next output buffer for the next scanline to generate.
  // This either returns a temporary buffer to place the data in, or
  // a row in the final output buffer, depending on how the display generation
  // process works.
  virtual UBYTE *NextScanLine(void);
  // Same for true color displays.
  virtual PackedRGB *NextRGBScanLine(void);
  //
  // Push the output buffer back into the display process. This signals that the
  // currently generated display line is ready.
  virtual void PushLine(UBYTE *buffer,int size);
  // Push an RGB scaneline.
  virtual void PushRGBLine(PackedRGB *buffer,int size);
  //  
  // Signal the requirement for refresh in the indicated region. This is only
  // used by the GUI engine as the emulator core uses PushLine for that.
  // Not required here... X11 will find out itself.
  virtual void SignalRect(LONG leftedge,LONG topedge,LONG width,LONG height);  
  //
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void) = 0;
  //
  // Coldstart and warmstart methods for the chip class.
  virtual void ColdStart(void) = 0;
  virtual void WarmStart(void) = 0;
  //
  // Argument parser frontends
  virtual void ParseArgs(class ArgParser *args)  = 0;
  virtual void DisplayStatus(class Monitor *mon) = 0;
  //  
};
///

///
#endif // of if HAVE_LIBX11
#endif
