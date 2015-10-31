/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: postprocessor.hpp,v 1.2 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Display postprocessor base class
 **********************************************************************************/

#ifndef POSTPROCESSOR_HPP
#define POSTPROCESSOR_HPP

/// Includes
#include "types.hpp"
#include "display.hpp"
#include "colorentry.hpp"
///

/// Forwards
class Machine;
///

/// Class PostProcessor
// Display post processing engines for filtering and similar effects.
class PostProcessor {
protected:
  //
  // Packed RGB color value for true color output
  typedef AtariDisplay::PackedRGB PackedRGB;
  //
  // Pointer to the machine (unused)
  class Machine           *machine;
  // Pointer to the display we output data to
  class AtariDisplay      *display;  
  // Pointer to the current color map.
  const struct ColorEntry *ColorMap;
  //
  // Constructor: Not from here, use derived
  // classes.
  PostProcessor(class Machine *mach,const struct ColorEntry *colormap);
  //
public:    
  virtual ~PostProcessor(void) = 0;
  //
  // What this is good for: Post-process the line and push it into
  // the display.
  virtual void PushLine(UBYTE *in,int size) = 0;
  //
  // Reset the postprocessor.
  virtual void Reset(void) = 0;
};
///

///
#endif
