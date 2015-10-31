/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: renderport.hpp,v 1.7 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of graphic primitivies
 **********************************************************************************/

#ifndef RENDERPORT_HPP
#define RENDERPORT_HPP

/// Includes
#include "types.h"
#include "types.hpp"
///

/// Forward References
class AtariDisplay;
class Machine;
///

/// RenderPort
// This structure describes the buffer we render into. It also
// allows some basic graphic operations that might be useful.
class RenderPort {
protected:
  UBYTE          *Buffer;     // Were we render into
  class AtariDisplay *Screen; // The class responsible for making the result visible.
  class Machine  *Machine;    // link to the complete emulator base class
  LONG            Width;      // width of the buffer
  LONG            Height;     // height of the buffer
  LONG            Modulo;     // pitch of the buffer
  UBYTE           Pen;        // the current pen we draw with.
  const UBYTE    *Font;       // Atari type font we use for rendering.
  LONG            X,Y;        // graphic cursor position
  LONG            Xo,Yo;      // origin of this renderport relative to the parent
  //
  // Get the address of a pixel
  UBYTE          *At(LONG x,LONG y);
  // Sort rectangle coordinates
  static void     SortPair(LONG &x1,LONG &y1,LONG &x2,LONG &y2);
public:
  RenderPort(void);
  ~RenderPort(void);
  // Construct a render port by clipping it out of an existing port.
  // We cannot safely unlink this port from the parent, though, and hence
  // must ensure that we use this kind of clipping only if we can guarantee
  // that the parent remains valid and is never unlinked.
  RenderPort(const class RenderPort *parent,LONG le,LONG te,LONG w,LONG h);
  //
  // Link the render port to the frontend, i.e. the machine. If the machine
  // is NULL, we disconnect from it. Before linkage, we cannot draw into
  // this port.
  void Link(class Machine *mach);
  //
  // Select the pen we draw with
  void SetPen(UBYTE pen);
  // Draw elementary graphics in the render port: Rectangles
  void FillRectangle(LONG xmin,LONG ymin,LONG xmax,LONG ymax);
  // Fill the entire port with one color
  void FillRaster(void);
  // Draw a vertical line in the port
  void DrawVertical(LONG height);
  // Draw a horizontal line in the port
  void DrawHorizontal(LONG width);
  // Render a text in the port
  void Text(const char *text,bool inverse=false);
  // Draw a frame in the port.
  void DrawFrame(LONG xmin,LONG ymin,LONG xmax,LONG ymax);
  //
  // Return the height of the port
  LONG HeightOf(void) const
  {
    return Height;
  }
  // Return the width of the port
  LONG WidthOf(void) const
  {
    return Width;
  }
  //
  // Read the current cursor position
  void ReadPosition(LONG &x,LONG &y)
  {
    x=X,y=Y;
  }
  // Set the current render position
  void Position(LONG x,LONG y)
  {
    X=x,Y=y;
  }
  //
  // Refresh the contents of the display by rendering it to the visual
  // frontend.
  void Refresh(void); 
  //
  // Similar to the rectangle fill, but this also sets the color and
  // expects dimensions rather than edge points.
  void CleanBox(LONG le,LONG te,LONG w,LONG h,UBYTE color);
  // Draw a raised/recessed frame, given the same coordinate system
  // than above.
  void Draw3DFrame(LONG le,LONG te,LONG w,LONG h,bool recessed,
		   UBYTE light=0x0a,UBYTE dark=0x02);
  // Draw a text clipped to a certain box in a given color.
  void TextClip(LONG le,LONG te,LONG w,LONG h,const char *text,UBYTE color);
  // Draw a text left-aligned into a certain box in a given color.
  void TextClipLefty(LONG le,LONG te,LONG w,LONG h,const char *text,UBYTE color);
};
///

///
#endif
