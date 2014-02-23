/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: basicrom.hpp,v 1.13 2013-03-16 15:08:50 thor Exp $
 **
 ** In this module: Administration/loading of the Basic ROM
 **********************************************************************************/

#ifndef BASICROM_HPP
#define BASICROM_HPP

/// Includes
#include "chip.hpp"
#include "memcontroller.hpp"
#include "rompage.hpp"
#include "cart8k.hpp"
///

/// Forwards
class Monitor;
class ArgParser;
class MMU;
///

/// Class BasicROM
// This class implements the loading of the Basic ROM. This doesn't come
// unexpected, though.
class BasicROM : public Chip, private Cart8K {
  //
  // Boolean: This is set if the basic has been loaded.
  bool                  loaded;
  //
  //
  // Basic preferences:
  //
  // names of the Basic image
  char                 *basicpath;
  //
  // name of the current basic image
  char                 *currentpath;
  //
  // Request to load the basic
  bool                  usebasic;
  //
  void LoadFromFile(const char *path);
  //  
public:
  BasicROM(class Machine *mach);
  ~BasicROM(void);
  //
  // We include here the interface routines imported from the chip class.
  //
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // The argument parser: Pull off arguments specific to this class.
  virtual void ParseArgs(class ArgParser *args);
  virtual void DisplayStatus(class Monitor *mon);
  //
  //
  // Implementations of the MemController features
  virtual void Initialize(void);
  //
  // Map the basic given the MMU.
  void MapBasic(class MMU *mmu)
  {
    if (loaded && usebasic)
      Cart8K::MapCart(mmu);
  }
  //
  // Service for the MMU: Check whether we really have the basic.
  // The initializer does not fail if we can't find it, we just warn.
  bool BasicLoaded(void) const
  {
    return (loaded && usebasic);
  }
};
///

///
#endif
