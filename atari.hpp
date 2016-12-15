/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: atari.hpp,v 1.9 2015/12/11 16:27:35 thor Exp $
 **
 ** In this module: Main loop of the emulator
 **********************************************************************************/

#ifndef ATARI_HPP
#define ATARI_HPP

/// Includes
#include "types.hpp"
#include "exceptions.hpp"
#include "chip.hpp"
///

/// Forwards
class Machine;
class Timer;
class AtariDisplay;
class Monitor;
class ChoiceRequester;
///

/// Class Atari
// This class implements the main emulator loop of the process.
class Atari : public Chip {
  //
  // This timer keeps the vertical blank interrupt precise
  class Timer           *VBITimer;
  //
  // The pointer to the display class that generates the image from
  // the intermediate antic buffer
  class AtariDisplay    *Display;
  // The pointer to Antic, which builds the display.
  class Antic           *Antic;
  // A requester that asks for shut-down
  class ChoiceRequester *YesNoRequester;
  //
  // The nominal display refresh frequency is setup by the PAL/NTSC
  // flag. If this is true, we are running in NTSC mode.
  bool                   NTSC;
  //
  // The maximal number of frames we may miss to speedup the emulation
  // frame rate. 
  LONG                   MaxMiss;
  //
  // If the following is set, then a custom refresh rate is selected.
  bool                   CustomRate;
  //
  // The desired refresh rate. Is one for full refresh, two to display
  // every other frame. This is in milliseconds per frame.
  LONG                   RefreshRate;
  //
  // Compute the refresh rate in microseconds.
  LONG RefreshDelay(void);
  //
public:
  //
  Atari(class Machine *mach);
  ~Atari(void);
  //
  // Implementation of the interfaces for the chip class
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  virtual void DisplayStatus(class Monitor *mon);
  virtual void ParseArgs(class ArgParser *args);
  //
  // This is the main loop of the emulator
  void EmulationLoop(void);
  //
  // Scale the given frequency to the current base frequency.
  int ScaleFrequency(int freq);
  //
  // Return true in case this is NTSC.
  bool isNTSC(void) const
  {
    return NTSC;
  }
};
///

///
#endif
