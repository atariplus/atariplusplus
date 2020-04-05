/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: basicrom.hpp,v 1.18 2020/03/28 13:10:01 thor Exp $
 **
 ** In this module: Administration/loading of the Basic ROM
 **********************************************************************************/

#ifndef BASICROM_HPP
#define BASICROM_HPP

/// Includes
#include "chip.hpp"
#include "memcontroller.hpp"
#include "patchprovider.hpp"
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
class BasicROM : public Chip, public PatchProvider, private Cart8K {
  //
  //
public:
  // Basic ROM types
  enum BasicType {
    Basic_Auto,         // whatever is available
    Basic_RevA,         // revision A
    Basic_RevB,         // revision B (buggy)
    Basic_RevC,         // the latest and last revision
    Basic_Builtin,      // Basic++
    Basic_Disabled      // no basic cart at all.
  };
  //
private:
  //
  // Offsets within Basic++ of math entry points.
  static const ADR      BasicOffsets[6];
  //
  // Basic preferences:
  //
  // The type of the basic that is used here.
  BasicType             basic_type;
  //
  // names of the Basic image
  char                 *basicapath;
  char                 *basicbpath;
  char                 *basiccpath;
  //
  // Install the math pack patch?
  bool                  mppatch;
  //
  // Load the selected ROM from disk
  void LoadROM(void);
  //
  // Load one or several pages from a file into the Basic ROM,
  // return an indicator of whether the basic could be loaded from the
  // given file name.
  void LoadFromFile(const char *path,const char *name);
  //
  // Check whether a given file is valid and contains a valid basic ROM
  // Throws on error.
  void CheckROMFile(const char *path);
  //
  // Check whether the Basic image can be found at the specified path. In case
  // it can, return true and set the string variable to the given
  // path. Return false otherwise.
  bool FindRomIn(char *&rompath,const char *suggested);
  //
  // Return the currently active Basic ROM type and return it.
  BasicType ROMType(void) const;
  //
  // Patch the built-in basic from the hexdump into the rom.
  void PatchFromDump(const unsigned char *dump,int pages);
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
    if (basic_type != Basic_Disabled)
      Cart8K::MapCart(mmu);
  }
  //
  // Service for the MMU: Check whether we really have the basic.
  // The initializer does not fail if we can't find it, we just warn.
  bool BasicLoaded(void) const
  {
    return (basic_type != Basic_Disabled);
  }
  //
  // Service for the math pack patch: Check whether this is the
  // build-in basic.
  bool isBuiltin(void) const
  {
    return (basic_type == Basic_Builtin);
  }
  //
  // Patch a byte of the ROM image.
  void PatchByte(ADR where,UBYTE value);
};
///

///
#endif
