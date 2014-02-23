/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xfront.cpp,v 1.18 2013/12/20 21:14:42 thor Exp $
 **
 ** In this module: Interface definition for a generic X11 frontend
 **********************************************************************************/

/// Includes
#include "xfront.hpp"
#include "x11_displaybuffer.hpp"
#include "x11_mappedbuffer.hpp"
#include "x11_truecolorbuffer.hpp"
#include "x11_xvideobuffer.hpp"
#include "gtia.hpp"
#include "machine.hpp"
#include "new.hpp"
#ifndef X_DISPLAY_MISSING
///

/// XFront::XFront
// The constructor of the XFront class
XFront::XFront(class Machine *mach,int unit)
  : AtariDisplay(mach,unit), FrameBuffer(NULL),
    display(NULL), screen(NULL), window(0), colormap(0)
{
}
///

/// XFront::~XFront
// The destructor of this class
XFront::~XFront(void)
{
  delete FrameBuffer;
}
///

/// XFront::FrameBufferOf
// Get and/or build the frame buffer for this class.
class X11_DisplayBuffer *XFront::FrameBufferOf(bool truecolor,bool xv)
{
  if (FrameBuffer == NULL) {
    // Pick the optimal (fastest) possible buffer
    // for our needs.
    if (xv) {
#ifdef X_USE_XV
      FrameBuffer = new class X11_XVideoBuffer(machine,this);
#else
      FrameBuffer = new class X11_TrueColorBuffer(machine,this);
#endif
    } else if (truecolor) {
      FrameBuffer = new class X11_TrueColorBuffer(machine,this);
    } else {
      FrameBuffer = new class X11_MappedBuffer(machine,this);
    }
  }
  return FrameBuffer;
}
///

/// XFront::BufferDimensions
void XFront::BufferDimensions(LONG &le,LONG &te,LONG &width,LONG &height,LONG &modulo)
{
  FrameBufferOf()->BufferDimensions(le,te,width,height,modulo);
}
///

/// XFront::NextScanLine
// Return the next output buffer for the next scanline to generate.
// This either returns a temporary buffer to place the data in, or
// a row in the final output buffer, depending on how the display generation
// process works.
UBYTE *XFront::NextScanLine(void)
{
  return FrameBuffer->NextScanLine();
}
///

/// XFront::NextRGBScanLine
AtariDisplay::PackedRGB *XFront::NextRGBScanLine(void)
{
  return FrameBuffer->NextRGBScanLine();
}
///

/// XFront::PushLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void XFront::PushLine(UBYTE *buffer,int size)
{
  FrameBuffer->PushLine(buffer,size);
}
///

/// XFront::PushRGBLine
// Push the output buffer back into the display process. This signals that the
// currently generated display line is ready.
void XFront::PushRGBLine(AtariDisplay::PackedRGB *buffer,int size)
{
  FrameBuffer->PushRGBLine(buffer,size);
}
///

/// XFront::SignalRect
// Signal the requirement for refresh in the indicated region. This is only
// used by the GUI engine as the emulator core uses PushLine for that.
// Not required here... X11 will find out itself.
void XFront::SignalRect(LONG leftedge,LONG topedge,LONG width,LONG height)
{
  FrameBuffer->SignalRect(leftedge,topedge,width,height);
}
///

/// XFront::UnloadFrameBuffer
// Unload the frame buffer, build a new one. This has to be called
// in case GTIA changes its mood concerning the truecolor/mapped
// property.
void XFront::UnloadFrameBuffer(void)
{
  delete FrameBuffer;
  FrameBuffer = NULL;
}
///

///
#endif // of if HAVE_LIBX11
///
