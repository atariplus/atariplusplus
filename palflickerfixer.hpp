/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: palflickerfixer.hpp,v 1.2 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Flicker fixer plus PAL color bluring
 **********************************************************************************/

#ifndef PALFLICKERFIXER_HPP
#define PALFLICKERFIXER_HPP

/// Includes
#include "types.hpp"
#include "postprocessor.hpp"
#include "vbiaction.hpp"
///

/// Class PALFlickerFixer
// Flickerfixer and PAL blurer: Combination of the two above
class PALFlickerFixer : public PostProcessor, private VBIAction {
  //
  // Pointer to the previous line of the same frame.
  UBYTE *PreviousLine;
  // Pointer to the previous frame.
  UBYTE *PreviousFrame;  
  // And the row within the previous frame.
  UBYTE *PreviousRow;
  //
  // Activity on a frame change. Not much happens here except that we reset the
  // internal frame buffers for vertical display post processing.
  virtual void VBI(class Timer *,bool,bool);
public:
  PALFlickerFixer(class Machine *mach,const struct ColorEntry *colormap);
  ~PALFlickerFixer(void);
  //
  //
  // What this is good for: Post-process the line and push it into
  // the display.
  virtual void PushLine(UBYTE *in,int size);
  //
  // Reset the postprocessor.
  virtual void Reset(void);
};
///

///
#endif
