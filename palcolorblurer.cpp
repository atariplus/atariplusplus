/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: palcolorblurer.cpp,v 1.2 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: PAL Display postprocessor
 **********************************************************************************/

/// Includes
#include "palcolorblurer.hpp"
#include "machine.hpp"
#include "colorentry.hpp"
#include "antic.hpp"
#include "stdlib.hpp"
///

/// PALColorBlurer::PALColorBlurer
// Setup the color blurer post processor class
PALColorBlurer::PALColorBlurer(class Machine *mach,const struct ColorEntry *colormap)
  : PostProcessor(mach,colormap), VBIAction(mach),
    PreviousLine(new UBYTE[Antic::DisplayModulo])
{
}
///

/// PALColorBlurer::~PALColorBlurer
// Dispose the pal color blurer post processor.
PALColorBlurer::~PALColorBlurer(void)
{
  delete[] PreviousLine;
}
///

/// PALColorBlurer::VBI
// VBI activity: Reset the previous line.
void PALColorBlurer::VBI(class Timer *,bool,bool)
{
  memset(PreviousLine,0,Antic::DisplayModulo);
}
///

/// PALColorBlurer::Reset
// Reset activity of the post-processor:
// Reset the blurer line.
void PALColorBlurer::Reset(void)
{
  memset(PreviousLine,0,Antic::DisplayModulo);
}
///

/// PALColorBlurer::PushLine
// Post process a single line, push it into the
// RGB output buffer and from there into the
// display buffer.
void PALColorBlurer::PushLine(UBYTE *in,int size)
{
  PackedRGB *out = display->NextRGBScanLine(); // get the next scanline for output
  
  if (out) {
    // Only if we have true-color output.
    UBYTE *in1     = in;
    UBYTE *in2     = PreviousLine;
    PackedRGB *rgb = out;
    int i          = size;
    // Blur the output line and this line: This happens if both lines
    // share the same intensity.
    do {
      if ((*in1 ^ *in2) & 0x0f) {
	// Intensity differs. Use only the new line.
	*rgb = ColorMap[*in1].XPackColor();
      } else {
	// Otherwise combine the colors.
	*rgb = ColorMap[*in1].XMixColor(ColorMap[*in2]);
      }
      rgb++;
      in1++;
      in2++;
    } while(--i);
    // Copy data to previous line
    memcpy(PreviousLine,in,size);
    display->PushRGBLine(out,size);
  } else {
    display->PushLine(in,size);
  }
}
///
