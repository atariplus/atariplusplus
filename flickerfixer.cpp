/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: flickerfixer.cpp,v 1.2 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Flicker fixer postprocessor class
 **********************************************************************************/

/// Includes
#include "flickerfixer.hpp"
#include "machine.hpp"
#include "antic.hpp"
///

/// FlickerFixer::FlickerFixer
// Setup the flicker fixer post processor class
FlickerFixer::FlickerFixer(class Machine *mach,const struct ColorEntry *colormap)
  : PostProcessor(mach,colormap), VBIAction(mach),
    PreviousFrame(new UBYTE[Antic::DisplayModulo * Antic::DisplayHeight]),
    PreviousRow(PreviousFrame)
{
}
///

/// FlickerFixer::~FlickerFixer
// Dispose the flicker fixer post processor.
FlickerFixer::~FlickerFixer(void)
{
  delete[] PreviousFrame;
}
///

/// FlickerFixer::VBI
// VBI activity: Reset the row counter.
void FlickerFixer::VBI(class Timer *,bool,bool)
{
  PreviousRow = PreviousFrame;
}
///

/// FlickerFixer::Reset
// Reset activity of the post-processor:
// Reset the blurer line.
void FlickerFixer::Reset(void)
{
  PreviousRow = PreviousFrame;
  memset(PreviousFrame,0,Antic::DisplayModulo * Antic::DisplayHeight);
}
///

/// FlickerFixer::PushLine
// Post process a single line, push it into the
// RGB output buffer and from there into the
// display buffer.
void FlickerFixer::PushLine(UBYTE *in,int size)
{
  PackedRGB *out = display->NextRGBScanLine(); // get the next scanline for output
  
  if (out) {
    // Only if we have true-color output.
    UBYTE *in1     = in;
    UBYTE *in2     = PreviousRow;
    PackedRGB *rgb = out;
    int i          = size;
    // Blur the output line and this line
    do {
      *rgb = ColorMap[*in1].XMixColor(ColorMap[*in2]);
      rgb++;
      in1++;
      in2++;
    } while(--i);
    //
    // Advance the row activity.
    memcpy(PreviousRow,in,size);
    PreviousRow += Antic::DisplayModulo;    
    display->PushRGBLine(out,size);
  } else {
    display->PushLine(in,size);
  }
}
///
