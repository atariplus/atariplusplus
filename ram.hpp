/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: ram.hpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of the RAM as a complete object with a single
 ** state.
 **********************************************************************************/

#ifndef RAM_HPP
#define RAM_HPP

/// Includes
#include "types.hpp"
#include "rampage.hpp"
#include "saveable.hpp"
#include "chip.hpp"
///

/// Forward references
class Machine;
class Monitor;
///

/// Class RAM
// The RAM class represents the total amount of RAM within the
// the machine and loads/saves its state as total.
class RAM : public Chip, public Saveable {
  //
  // the RAM itself as 256 pages = 64K of memory.
  class RamPage *Pages;
  //
  //
public:
  RAM(class Machine *mach);
  ~RAM(void);
  //
  // Implementation of the chip methods
  // Warmstart and ColdStart.
  virtual void WarmStart(void);
  virtual void ColdStart(void);
  //
  // Implementation of the state definition
  virtual void State(class SnapShot *snap);
  //
  // If we just want to think as the RAM as an array of pages, then
  // here is the conversion operator that does this. The MMU
  // build-up process likes to think this way:
  class RamPage *RamPages(void) const
  {
    return Pages;
  }
  //
  void ParseArgs(class ArgParser *args);
  //
  // Print the status of the RAM. Is there anything to display?
  void DisplayStatus(class Monitor *mon);
  //
};
///

///
#endif

