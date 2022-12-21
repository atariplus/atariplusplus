/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_mappedbuffer.hpp,v 1.9 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

#ifndef X11_MAPPEDBUFFER_HPP
#define X11_MAPPEDBUFFER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "chip.hpp"
#include "gtia.hpp"
#include "stdio.hpp"
#include "screendump.hpp"
#ifndef X_DISPLAY_MISSING
#include "x11_displaybuffer.hpp"
#include <X11/Xlib.h>
///

/// Forward references
class ArgParser;
class Monitor;
class XFront;
///

/// Class X11_MappedBuffer
// This class keeps the memory for the X11 interface
// and computes draw-commands from display changes
// This implementation of the display buffer uses an
// old-fashioned color map for its job, it does not
// offer any kind of true-color remapping.
class X11_MappedBuffer : public X11_DisplayBuffer {
  //
public:
  // Size of the point/rectangle buffer in entries.
  // Latest revisions of the i830M drivers slow down noticably
  // with smaller buffer sizes.
  static const int RenderBufferSize INIT(128);
  static const int ScanBuffNum      INIT(256);
  //
private:
  //
  // Primary and secondary buffer
  UBYTE           *active; // screen we are currently rendering into
  UBYTE           *last;   // last screen we used for rendering
  UBYTE           *row;    // row we currently fill into
  //
  // List of GraphicContexts for all the 256 colors.
  //
  GC              *graphiccontexts;
  //
  // Allocated pens of the above colormap
  struct Pen {
    unsigned long pen;   // the pen we got from X
    bool          alloc; // set to true as soon as we got it.
    //
    Pen(void)
      : alloc(false)
    { }
  }              *pens; // contains the pixel colors for X11
  //
  // The following is set to true to enforce that the next refresh is a full
  // refresh
  bool            enforcefullrefresh;
  //
  // This keeps a collection of rectangles or lines, sorted by color.
  // The ScanBuffer pointer keeps the array completely.
  struct ScanBlock {
    // The rectangles we render to the screen
    XRectangle  *rectangles;
    //
    // The Display we render to
    Display     *display;
    //
    // The drawable we render into
    Drawable     target;
    //
    // The graphic context for this line (a copy of it)
    GC           context;
    //
    // The number of valid entries in the above
    int          entries;
    //
    // My color = the index in the ScanBuffer array
    int          color;
    //
    // The basic size of one rectangle in here in X-pixels
    int          width,height;
    //
    // Constructor and destructor
    ScanBlock(Display *dsp,Drawable d,GC gc,int w,int h,int c)
      : rectangles((new XRectangle[RenderBufferSize])),
	display(dsp), target(d), context(gc),
	entries(0), color(c), width(w), height(h)
    { 
    }
    //
    ~ScanBlock(void)
    {
      delete[] rectangles;
    }
    //
    // Add a pixel to the ScanBuffer, possibly draw the contents if
    // we are too full. x and y are the coordinates (in the atari screen)
    // w is the number of pixels, h is the height in atari lines.
    void AddPixel(int x,int y,int w, int h);
    //
    // Flush the block and allow reusal: We need to specify:
    // A target display to draw on, a drawable we fill into and
    // an array of 256 graphic contexts that are setup for the
    // foreground/background combinations.
    void FlushBlock(void);
  }              **ScanBuffer;
  //
  // Find the scanbuffer to a given color or create it if it doesn't
  // exist.
  struct ScanBlock *FindBlock(int color)
  {
    if (ScanBuffer[color]) 
      return ScanBuffer[color];

    ScanBuffer[color] = new struct ScanBlock(display,(PixMapIndirect)?(pixmap):(window),
					     graphiccontexts[color],PixelWidth,PixelHeight,color);
    return ScanBuffer[color];
  }
  //
  // Request all colors from the colormap. 
  // Returns the number of missing colors.
  int AllocateColors(void);
  //
public:
  // Methods required for constructing and destrucing us
  X11_MappedBuffer(class Machine *mach, class XFront *front);
  ~X11_MappedBuffer(void);
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
    return row;
  }
  //
  // Push the output buffer back into the display process. This signals that the
  // currently generated display line is ready.
  virtual void PushLine(UBYTE *,int)
  {
    row += modulo;
  }
  //  
  // Signal that we start the display generation again from top. Hence, this implements
  // some kind of "vertical sync" signal for the display generation.
  virtual void ResetVertical(void)
  {
    row = ActiveBuffer();
  }
};
///

///
#endif // of if HAVE_LIBX11
#endif


  
  
