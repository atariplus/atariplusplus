/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: palcolorblurer.hpp,v 1.2 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: PAL Display postprocessor
 **********************************************************************************/

#ifndef PALCOLORBLURER_HPP
#define PALCOLORBLURER_HPP

/// Includes
#include "types.hpp"
#include "postprocessor.hpp"
#include "vbiaction.hpp"
///

/// PALColorBlurer
// PAL color bluring.
class PALColorBlurer : public PostProcessor, private VBIAction {
  //
  // Pointer to the previous line.
  UBYTE *PreviousLine;  
  //
  // Activity on a frame change. Not much happens here except that we reset the
  // internal frame buffers for vertical display post processing.
  virtual void VBI(class Timer *,bool,bool);
public:
  PALColorBlurer(class Machine *mach,const struct ColorEntry *colormap);
  ~PALColorBlurer(void);
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
