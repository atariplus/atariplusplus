/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: palflickerfixer.cpp,v 1.2 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Flicker fixer plus PAL color bluring
 **********************************************************************************/

/// Includes
#include "palflickerfixer.hpp"
#include "machine.hpp"
#include "antic.hpp"
///

/// PALFlickerFixer::PALFlickerFixer
// Setup the flicker fixer post processor class
PALFlickerFixer::PALFlickerFixer(class Machine *mach,const struct ColorEntry *colormap)
  : PostProcessor(mach,colormap), VBIAction(mach),
    PreviousLine(new UBYTE[Antic::DisplayModulo]),
    PreviousFrame(new UBYTE[Antic::DisplayModulo * Antic::DisplayHeight]),
    PreviousRow(PreviousFrame)
{
}
///

/// PALFlickerFixer::~PALFlickerFixer
// Dispose the flicker fixer post processor.
PALFlickerFixer::~PALFlickerFixer(void)
{
  delete[] PreviousFrame;
  delete[] PreviousLine;
}
///

/// PALFlickerFixer::VBI
// VBI activity: Reset the row counter.
void PALFlickerFixer::VBI(class Timer *,bool,bool)
{
  PreviousRow = PreviousFrame;  
  memset(PreviousLine,0,Antic::DisplayModulo);
}
///

/// PALFlickerFixer::Reset
// Reset activity of the post-processor:
// Reset the blurer line.
void PALFlickerFixer::Reset(void)
{
  PreviousRow = PreviousFrame;
  memset(PreviousFrame,0,Antic::DisplayModulo * Antic::DisplayHeight);  
  memset(PreviousLine,0,Antic::DisplayModulo);
}
///

/// PALFlickerFixer::PushLine
// Post process a single line, push it into the
// RGB output buffer and from there into the
// display buffer.
void PALFlickerFixer::PushLine(UBYTE *in,int size)
{
  PackedRGB *out = display->NextRGBScanLine(); // get the next scanline for output
  
  if (out) {
    // Only if we have true-color output.
    UBYTE *in1     = in;
    UBYTE *in2     = PreviousRow;
    UBYTE *in3     = PreviousLine;
    PackedRGB *rgb = out;
    int i          = size;
    // Blur the output line and this line
    do {      
      if ((*in1 ^ *in3) & 0x0f) {
	// Intensity differs. Use only the new line.
	*rgb = ColorMap[*in1].XMixColor(ColorMap[*in2]);
      } else {
	// Otherwise combine the colors.
	*rgb = ColorMap[*in1].XMixColor(ColorMap[*in3],ColorMap[*in2]);
      }
      rgb++;
      in1++;
      in2++;
      in3++;
    } while(--i);
    //
    // Advance the row activity.    
    memcpy(PreviousRow,in,size);
    PreviousRow += Antic::DisplayModulo;    
    // Copy data to previous line
    memcpy(PreviousLine,in,size);    
    display->PushRGBLine(out,size);
  } else {
    display->PushLine(in,size);
  }
}
///
