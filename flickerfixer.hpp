/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: flickerfixer.hpp,v 1.3 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Flicker fixer postprocessor class
 **********************************************************************************/

#ifndef FLICKERFIXER_HPP
#define FLICKERFIXER_HPP

/// Includes
#include "types.hpp"
#include "postprocessor.hpp"
#include "vbiaction.hpp"
///

/// Class FlickerFixer
// Flickerfixer: Combines two frames into one
class FlickerFixer : public PostProcessor, private VBIAction {
  //
  // Pointer to the previous frame.
  UBYTE *PreviousFrame;  
  // And the row within the previous frame.
  UBYTE *PreviousRow;
  //
  // Activity on a frame change. Not much happens here except that we reset the
  // internal frame buffers for vertical display post processing.
  virtual void VBI(class Timer *,bool,bool);
public:
  FlickerFixer(class Machine *mach,const struct ColorEntry *colormap);
  ~FlickerFixer(void);
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
