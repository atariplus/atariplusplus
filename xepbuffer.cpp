/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xepbuffer.cpp,v 1.4 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: The character buffer of the XEP output
 **********************************************************************************/

/// Includes
#include "xepbuffer.hpp"
#include "machine.hpp"
#include "display.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "snapshot.hpp"
#include "charmap.hpp"
#include "string.hpp"
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int XEPBuffer::CharacterWidth  = 80;
const int XEPBuffer::CharacterHeight = 25;
#endif
///

/// XEPBuffer::XEPBuffer
XEPBuffer::XEPBuffer(class Machine *mach)
  : Chip(mach,"XEPBuffer"), Saveable(mach,"XEPBuffer"),
    Characters(NULL)
{  
  // Front and back color for the text in the
  // color box. We separate this into color and hue
  // to be a bit more user friendly.
  FrontHue       = 0x0a;
  FrontLuminance = 0x00;
  BackHue        = 0x0a;
  BackLuminance  = 0x0a;
}
///

/// XEPBuffer::~XEPBuffer
XEPBuffer::~XEPBuffer(void)
{
  delete[] Characters;
}
///

/// XEPBuffer::WarmStart
// Reset this class, initialize the buffer.
void XEPBuffer::WarmStart(void)
{  
  LONG le,te;
  //
  // Clear the contents of the buffer.
  memset(Characters,' ',CharacterWidth * CharacterHeight);
  //
  // Get the screen we render to.
  Screen     = machine->XEPDisplay();
  Screen->EnableDoubleBuffer(false);
  RawBuffer  = Screen->ActiveBuffer(); 
  Screen->BufferDimensions(le,te,Width,Height,Modulo);
  //
  // Adjust offset to that of the left top edge of the buffer.
  RawBuffer += le + te * Modulo; 
  //
  // Get the font to render the data.
  Font       = CharMap;
}
///

/// XEPBuffer::ColdStart
// Reset this class after a power-on
void XEPBuffer::ColdStart(void)
{
  if (Characters == NULL) {
    Characters = new UBYTE[CharacterWidth * CharacterHeight];
  }
  //
  // Now warmstart this class.
  WarmStart();
}
///

/// XEPBuffer::RefreshRegion
// Refresh the the rectangle to the output
// display given the coordinates of the rectangle
// in character coordinates.
void XEPBuffer::RefreshRegion(LONG x,LONG y,LONG w,LONG h)
{
  LONG rawleft,rawtop,rawwidth,rawheight,row;
  UBYTE *src,*dst,*target,c,mask,col,front,back;
  const UBYTE *font;
  bool inverse;
  int dx,dy;
  //
  rawleft   = (x << 3);
  rawtop    = (y << 3);
  rawwidth  = (w << 3);
  rawheight = (h << 3);
  src       = Characters + x       + y      * CharacterWidth;
  dst       = RawBuffer  + rawleft + rawtop * Modulo;
  front     = (FrontHue << 4) | FrontLuminance;
  back      = (BackHue  << 4) | BackLuminance;
  //
  while(h) {
    row = w;
    while(row) {
      c = *src;
      // Check for inverse video
      if (c & 0x80) {
	inverse = true;
	c      &= 0x7f;
      } else {
	inverse = false;
      }
      font   = (ToANTIC(c) << 3) + Font;
      target = dst;
      for(dy = 0;dy < 8;dy++) {
	for(dx = 0,mask = 0x80;dx < 8;dx++,mask >>= 1) {
	  if (inverse) {
	    if (*font & mask) {
	      col = back;
	    } else {
	      col = front;
	    }
	  } else {
	    if (*font & mask) {
	      col = front;
	    } else {
	      col = back;
	    }
	  }
	  *target++ = col;
	}
	font++,target += Modulo - 8;
      }
      src++,dst += 8,row--;
    }
    // Go to the next row/column
    src += CharacterWidth -  w;
    dst += (Modulo << 3)  - (w << 3);
    h--;
  }  
  //
  // Signal the display driver to refresh the indicated region.
  Screen->SignalRect(rawleft,rawtop,rawwidth,rawheight);
}
///

/// XEPBuffer::DisplayStatus
// Print the status of the XEP buffer on the monitor
void XEPBuffer::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("XEP Display Buffer status.\n"
		   "\tXEP Buffer is fine.\n");
}
///

/// XEPBuffer::ParseArgs
// Parse the user arguments for this class.
// Currently, there is none.
void XEPBuffer::ParseArgs(class ArgParser *)
{
}
///

/// XEPBuffer::State
// Save or load the internal state of this class.
void XEPBuffer::State(class SnapShot *snap)
{
  //
  // Write the contents of this buffer as part of the
  // state snapshot.
  snap->DefineChunk("DisplayContents","the contents of the XEP buffer in hex notation",
		    Characters,CharacterWidth * CharacterHeight);
}
///
