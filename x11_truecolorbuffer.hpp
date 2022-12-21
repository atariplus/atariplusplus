/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: x11_truecolorbuffer.hpp,v 1.13 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Conversions from ANTIC/GTIA output to X11 draw commands
 **********************************************************************************/

#ifndef X11_TRUECOLORBUFFER_HPP
#define X11_TRUECOLORBUFFER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "chip.hpp"
#include "colorentry.hpp"
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

/// Class X11_TrueColorBuffer
// This class keeps the memory for the X11 interface
// and computes draw-commands from display changes.
// This implementation uses a true-color 32 bit buffer
// and is therefore able to mix colors more precisely
// than the mapped implementation.
class X11_TrueColorBuffer : public X11_DisplayBuffer {
  //
public:
  // Size of the point/rectangle buffer in entries.
 // Latest revisions of the i830M drivers slow down noticably
  // with smaller buffer sizes.
  static const int RenderBufferSize INIT(128);
  static const int ScanBuffNum      INIT(128);
  //
private:
  //
  // The way how we pack colors into 24 bits.
  typedef AtariDisplay::PackedRGB PackedRGB;
  //
  // Primary and secondary buffer
  PackedRGB       *active;      // screen we are currently rendering into
  PackedRGB       *last;        // last screen we used for rendering
  PackedRGB       *row;         // row we currently fill into
  PackedRGB       *lastrow;     // pointer in the last active buffer
  UBYTE           *changeflags; // a flagged array that indicates modified lines.
  UBYTE           *curflag;     // the current modification flag.
  // Indexed / colormapped buffers for old-fashioned access
  UBYTE           *idxactive;
  UBYTE           *idxrow;
  //
  // The currently active colormap
  const struct ColorEntry *colormap;
  //
  // The following is set to true to enforce that the next refresh is a full
  // refresh
  bool            enforcefullrefresh;
  //
  // The following boolean gets set in case the indexed map gets dirty
  // by the GUI engine
  bool            indexdirty;
  //
  // Index of the next scan block buffer to allocate
  // for a fresh color.
  int             nextscanblock;
  //
  LONG            bufferlines;  // vertical size of the buffer
  //
  // This keeps a collection of rectangles or lines, sorted by color.
  // The ScanBuffer pointer keeps the array completely.
  struct ScanBlock {
    // The rectangles we render to the screen
    XRectangle   *rectangles;
    //
    // The Display we render to
    Display      *display;
    //
    // The drawable we render into
    Drawable      target;
    //
    // The graphic context for this line (a copy of it)
    GC            context;
    //
    // The number of valid entries in the above
    int           entries;
    //
    // My color as packed R,G,B value
    PackedRGB     color;
    //
    // Pen allocated from X
    unsigned long xpen;
    //
    // The basic size of one rectangle in here in X-pixels
    int           width,height;
    //
    // The next boolean indicates whether this
    // buffer allocated the X resources herein (GC, xpen)
    bool          alloc;
    //
    // Constructor and destructor
    ScanBlock(Display *dsp,Drawable d,int w,int h)
      : rectangles((new XRectangle[RenderBufferSize])),
	display(dsp), target(d), 
	entries(0), width(w), height(h), alloc(false)
    { 
    }
    //
    ~ScanBlock(void)
    {
      // This does *not* release the X resources,
      // though. The caller must do this since we 
      // don't buffer the display/colormap pointers here.
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
  // Release the resources for the scan buffer 
  // without flushing it and without releasing the
  // administration structure.
  void ReleaseScanBlock(struct ScanBlock *buf);
  //
  // Find the scanbuffer to a given color or create it if it doesn't
  // exist. Might return NULL if no suitable block can be found.
  struct ScanBlock *FindBlock(PackedRGB color);
  //
  // Allocate an RGB pen from the X system. Returns true in case we
  // got it.
  bool XAllocPen(unsigned long &pen,PackedRGB color);
  //
  // Release a pen for the X11 display system.
  void XFreePen(unsigned long pen);
  //
public:
  // Methods required for constructing and destrucing us
  X11_TrueColorBuffer(class Machine *mach, class XFront *front);
  ~X11_TrueColorBuffer(void);
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
#endif // of if HAVE_LIBX11
#endif


  
  
