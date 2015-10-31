/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: renderport.cpp,v 1.15 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of graphic primitivies
 **********************************************************************************/

/// Includes
#include "renderport.hpp"
#include "machine.hpp"
#include "display.hpp"
#include "charmap.hpp"
#include "timer.hpp"
///

/// RenderPort::RenderPort
// Create the render port. As we require the machine and the display front for
// it, but this GUI frontend changes exactly this layout, we cannot require
// the machine for the construction of the port.
RenderPort::RenderPort(void)
  : Buffer(NULL), Screen(NULL), Machine(NULL),
    Width(0), Height(0), Modulo(0), Pen(0), Font(NULL), X(0), Y(0), Xo(0), Yo(0)
{ }
///

/// RenderPort::~RenderPort
// Nothing happens here.
RenderPort::~RenderPort(void)
{ }
///

/// RenderPort::RenderPort
// Construct a render port by clipping it out of an existing port.
RenderPort::RenderPort(const class RenderPort *parent,LONG le,LONG te,LONG w,LONG h)
  : Screen(parent->Screen), Machine(parent->Machine),
    Width(w), Height(h), Modulo(parent->Modulo), Pen(0), Font(CharMap), X(0), Y(0), 
    Xo(parent->Xo + le), Yo(parent->Yo + te)
{
  // Check whether the parent has a buffer. If so, we have one as well. 
  Buffer = parent->Buffer + le + te * Modulo;
}
///

/// RenderPort::Link
// Link the render port to the frontend, i.e. the machine. If the machine
// is NULL, we disconnect from it.
void RenderPort::Link(class Machine *mach)
{
  if (mach) {
    LONG le,te;
    Machine = mach;
    Screen  = mach->Display();
    // Get the display layout. 
    // Now install all the data:
    // We first need to fetch the buffer as this may just
    // enforce a re-calculation of the necessary data within the
    // frontend.
    Screen->EnableDoubleBuffer(false);
    Buffer  = Screen->ActiveBuffer();
    Screen->BufferDimensions(le,te,Width,Height,Modulo);
    Buffer += le + te * Modulo;
    Xo      = le;
    Yo      = te;
    Pen     = 0;
    Font    = CharMap;
  } else {    
    if (Screen && Machine) {
      // Be extra careful! If we enter here, we just might have
      // made a global change, thus making our "screen" pointer
      // useless. Better fetch a new one.
      Screen = Machine->Display();
      if (Screen)
	Screen->EnableDoubleBuffer(true);
    }
    Width  = 0;
    Height = 0;
    Modulo = 0;
    Buffer = NULL;
    Screen = NULL;
    Font   = NULL;
  }
}
///

/// RenderPort::Refresh
// Refresh the screen contents by mapping the render buffer
// to the visual frontend
void RenderPort::Refresh(void)
{  
  if (Machine) {
    LONG le,te;
    class Timer dummy;    
    // Update the display now by running a VBI cycle of the machine
    // in "paused" mode.  
    dummy.StartTimer(0,0);  
    //This is no longer required...
    //Screen->EnforceFullRefresh();
    Machine->VBI(&dummy,false,true);
    // Refetch the buffer, this might have been toggled
    Screen->BufferDimensions(le,te,Width,Height,Modulo);
    // Now install all the data:
    Buffer = Screen->ActiveBuffer() + le + te * Modulo;
  }
}
///

/// RenderPort::At
// Return the address of a pixel
UBYTE *RenderPort::At(LONG x,LONG y)
{
  // First clip the pixel. If it is outside the port, return NULL.
  if (x < 0 || x >= Width || y < 0 || y >= Height)
    return NULL;

  return Buffer + x + y * Modulo;
}
///

/// RenderPort::SortPair
// Sort rectangle coordinates
void RenderPort::SortPair(LONG &x1,LONG &y1,LONG &x2,LONG &y2)
{
  LONG tmp;

  if (x1 > x2) {
    tmp = x1;
    x1  = x2;
    x2  = tmp;
  }
  if (y1 > y2) {
    tmp = y1;
    y1  = y2;
    y2  = tmp;
  }
}
///

/// RenderPort::SetPen
// Select the drawing pen for futher operations
void RenderPort::SetPen(UBYTE pen)
{
  Pen = pen;
}
///

/// RenderPort::FillRectangle
// Set a rectangle to a solid color
void RenderPort::FillRectangle(LONG xmin,LONG ymin,LONG xmax,LONG ymax)
{
  UBYTE *p;
  
  SortPair(xmin,ymin,xmax,ymax);

  // Clip the edges such that we are inside the port
  if (xmin < 0)       xmin = 0;
  if (ymin < 0)       ymin = 0;
  if (xmax >= Width)  xmax = Width  - 1;
  if (ymax >= Height) ymax = Height - 1;
  //
  // Now compute the size.
  xmax -= xmin - 1;
  ymax -= ymin - 1;
  // Signal that this rectangle requires refresh  
  if (xmax > 0 && ymax > 0) {  
    LONG w,h;
    w     = xmax;
    h     = ymax;
    p     = At(xmin,ymin);
    if (p) do {
      memset(p,Pen,xmax);
      p += Modulo;
    } while(--ymax);
    Screen->SignalRect(xmin + Xo,ymin + Yo,w,h);
  }
}
///

/// RenderPort::FillRaster
// Fill all of the port with the active pen
void RenderPort::FillRaster(void)
{
  FillRectangle(0,0,Width-1,Height-1); // now, this was obvious, right?
}
///

/// RenderPort::DrawVertical
// Draw a vertical line
void RenderPort::DrawVertical(LONG height)
{
  UBYTE *p;
  LONG tmp;
  LONG ymin = Y,ymax = Y + height;
 
  if (height > 0) {
    ymax--;
  } else {
    ymax++;
  }
  Y = ymax;
  if (ymax < ymin) {
    tmp  = ymax;
    ymax = ymin;
    ymin = tmp;
  }
  // Now clip coordinates.
  if (ymin < 0)       ymin = 0;
  if (ymax >= Height) ymax = Height-1;
  // Compute the height of the line
  ymax -= ymin - 1;
  p = At(X,ymin);
  // Signal that this rectangle requires refresh
  if (p && ymax > 0) {
    LONG h = ymax;
    do {
      *p = Pen;
      p += Modulo;
    } while(--ymax);  
    Screen->SignalRect(X+Xo,ymin+Yo,1,h);
  }
}
///

/// RenderPort::DrawHorizontal
// Draw a horizontal line
void RenderPort::DrawHorizontal(LONG width)
{
  UBYTE *p;
  LONG tmp;
  LONG xmin = X,xmax = X + width;

  if (width > 0) {
    xmax--;
  } else {
    xmax++;
  }
  X = xmax;
  if (xmax < xmin) {
    tmp  = xmax;
    xmax = xmin;
    xmin = tmp;
  }
  // Now clip coordinates.
  if (xmin < 0)      xmin = 0;
  if (xmax >= Width) xmax = Width-1;
  // Compute the height of the line
  xmax -= xmin - 1;
  p = At(xmin,Y);  
  if (p && xmax > 0) {
    memset(p,Pen,xmax);
    // Signal that this rectangle requires refresh
    Screen->SignalRect(xmin+Xo,Y+Yo,xmax,1);
  }
}
///

/// RenderPort::DrawFrame
// Draw a frame around the given coordinates
void RenderPort::DrawFrame(LONG xmin,LONG ymin,LONG xmax,LONG ymax)
{
  LONG width,height;
  SortPair(xmin,ymin,xmax,ymax);
  // Now, this just uses the elementary line drawer.
  width  = xmax-xmin+1;
  height = ymax-ymin+1;
  Position(xmin,ymin);
  DrawHorizontal(width);
  DrawVertical(height);
  DrawHorizontal(-width);
  DrawVertical(-height);
}
///

/// RenderPort::Text
// Render a text at the given position
void RenderPort::Text(const char *text,bool inverse)
{
  UBYTE *p;
  LONG dx,dy;
  char c;
  LONG le,te,w,h;
  //
  le = X + Xo;
  te = Y + Yo;
  w  = LONG(strlen(text) << 3);
  h  = 8;
  
  while((c = *text++)) {
    const UBYTE *glyph;
    // First compute the address of the text to print within the port
    glyph = Font + ((ULONG)(ToANTIC(c)) << 3);
    // Get the pointer. This might be still outside of the port. Do not mind here.
    p     = Buffer + X + Y * Modulo;
    // Render the glyph
    for(dy = 0;dy < 8;dy++,p+=Modulo,glyph++) {
      // Check whether y is within range
      if (Y+dy >= 0 && Y+dy < Height) {
	UBYTE line,mask;
	// Get the mask from the current line of the glyph
	line = *glyph;
	mask = 0x80;
	for(dx = 0,mask = 0x80;dx < 8;dx++,mask >>= 1) {
	  if (((inverse == false) && (line & mask)     ) ||
	      ((inverse == true)  && (line & mask) == 0)) {
	    if (X + dx >= 0 && X + dx < Width) {
	      p[dx] = Pen;
	    }
	  }
	}
      }
    }
    X += 8; // One character
  }
  Y += 8; // One text line  
  //
  // Signal that this rectangle requires refresh
  if (le < 0) {
    w += le;
    le = 0;
  }
  if (te < 0) {
    h += te;
    te = 0;
  }
  Screen->SignalRect(le,te,w,h);
  //
}
///

/// RenderPort::CleanBox
// Similar to the rectangle fill, but this also sets the color and
// expects dimensions rather than edge points.
void RenderPort::CleanBox(LONG le,LONG te,LONG w,LONG h,UBYTE color)
{
  SetPen(color);
  FillRectangle(le,te,le+w-1,te+h-1);
}
///

/// RenderPort::Draw3DFrame
// Draw a raised/recessed frame, given the same coordinate system
// than above.
void RenderPort::Draw3DFrame(LONG le,LONG te,LONG w,LONG h,bool recessed,
				   UBYTE light,UBYTE dark)
{
  UBYTE toppen,botpen;

  if (recessed) {
    toppen = dark;
    botpen = light;
  } else {
    toppen = light;
    botpen = dark;
  }
  
  SetPen(toppen);
  Position(le,te);
  DrawHorizontal(w);
  SetPen(botpen);
  DrawVertical(h);
  DrawHorizontal(-w);
  SetPen(toppen);
  DrawVertical(-h);
}
///

/// RenderPort::TextClip
// Draw a text clipped to a certain box in a given color.
void RenderPort::TextClip(LONG le,LONG te,LONG w,LONG h,
				const char *text,UBYTE color)
{
  size_t cnt;
  size_t chr;
  char buffer[80];

  if (h < 8) // Not enough room otherwise!
    return; 

  // We know that each character is eight pixel wide, hence clipping is quite
  // simple.
  cnt = w >> 3;
  chr = strlen(text);
  //
  // If the text is longer than what we can print, abbreviate.
  if (chr > cnt) {
    if (cnt > 3) {
      snprintf(buffer,79,"%*.*s...",int(3-cnt),int(cnt-3),text);
    } else {
      snprintf(buffer,79,"%-3.*s",int(cnt),text);
    }
  } else {
    // Center within the buffer.
    le += LONG((cnt - chr) << 2);
    snprintf(buffer,79,"%s",text);
  }
  te += (h - 8) >> 1;
  SetPen(color);
  Position(le,te);
  Text(buffer);
}
///

/// RenderPort::TextClipLefty
// Draw a text clipped to a certain box in a given color, left-aligned
void RenderPort::TextClipLefty(LONG le,LONG te,LONG w,LONG h,
				     const char *text,UBYTE color)
{
  size_t cnt,chr;
  char buffer[80];

  if (h < 8) // Not enough room otherwise!
    return; 

  // We know that each character is eight pixel wide, hence clipping is quite
  // simple.
  cnt = w >> 3;
  chr = strlen(text);
  //
  // If the text is longer than what we can print, abbreviate.
  if (chr > cnt) {
    if (cnt > 3) {
      snprintf(buffer,79,"%*.*s...",int(3-cnt),int(cnt-3),text);
    } else {
      snprintf(buffer,79,"%-3.*s",int(cnt),text);
    }
  } else {
    snprintf(buffer,79,"%s",text);
  }
  te += (h - 8) >> 1;
  SetPen(color);
  Position(le,te);
  Text(buffer);
}
///
