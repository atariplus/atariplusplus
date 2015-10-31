/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: amdchip.hpp,v 1.5 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Emulation of AMD FlashROM chips by Mark Keates
 **********************************************************************************/

#ifndef AMDCHIP_HPP
#define AMDCHIP_HPP

/// Includes
#include "chip.hpp"
#include "memcontroller.hpp"
#include "saveable.hpp"
///

/// Forwards
class CartFlash;
class FlashPage;
class MMU;
class ArgParser;
///

/// Class AmdChip
// This class emulates the internal wiring and behaviour of the AMD flash
// ROMs.
class AmdChip : public Chip, public MemController, public Saveable {
public:
  // Hardware emulated by this class
  enum ChipType
  {
	AmNone,
	Am010,
	Am020,
	Am040
  };
  //
private:
  //
  // Internal state machine
  enum ChipState
  {
	CmdRead,
	CmdSecond,
	CmdThird,
	CmdAutoSelect,
	CmdProgram,
	CmdErase1,
	CmdErase2,
	CmdErase,
	CmdLimbo
  };
  //
  // State of the chip and the chip type of the currently
  // inserted chip
  enum ChipState     cmdState;
  enum ChipType      type;
  //
  // Number of banks in this chip
  UBYTE              TotalBanks;
  UWORD		     TotalPages;
  //
  // The currently selected active bank.
  UBYTE              ActiveBank;
  //
  // Boolean indicator whether this cart has
  // been written to and requires saving back.
  bool               Modified;
  //
  // The following is true in case the user desires to activate
  // the mapping. Otherwise, no data is read.
  bool               Enabled;
  //
  // Several copies of this chip might be available. The following
  // is the unit number.
  UBYTE              Unit;
  //
  // Flag of the currently active bank.
  //
  class CartFlash   *Parent;
  class FlashPage  **Rom;
  //
  // Private Methods:
  //
  // Erase the chip completely, or only a selected sector of it,
  // where a sector is a hardware defined chunk of ROM.
  void ChipErase(UWORD sector = 0xffff);
  //
public:
  AmdChip(class Machine *mach,ChipType ct,const char *name,int unit,class CartFlash *cf);
  virtual ~AmdChip(void);
  //
  // Get the emulated hardware type here.
  ChipType GetType(void) const
  { 
    return type; 
  }
  //
  // Return the number of pages made available by this chip.
  UWORD TotalPagesOf(void) const
  {
    return TotalPages;
  }
  //
  // Return the number of banks made available by this chip.
  UWORD TotalBanksOf(void) const
  {
    return TotalBanks;
  }
  //
  // Check whether this chip might intercept the ROM read and
  // might instead return chip state data. Used here to allow
  // inlining of the ROM read access.
  bool InterceptsRead(void) const
  {
    return (cmdState == CmdAutoSelect);
  }
  //
  // Return a boolean indicator whether the contents of this
  // chip has been modified.
  bool IsModified(void) const
  {
    return Modified;
  }
  //
  // Perform a write into the Flash ROM area, possibly modifying the content.
  // This never expects a WSYNC. Default is not to perform any operation.
  // Return code indicates whether this write was useful for this cart.
  bool RomAreaWrite(ADR mem,UBYTE val);
  //
  // Perform a read from the Flash ROM area.
  // If the memory chip is in an Autoselect state, it can return the chip-id.
  // otherwise the function returns the value passed for the actual memory.
  UBYTE RomAreaRead(ADR mem,UBYTE val);
  //
  // Read or write the contents of this class to memory.
  void ReadFromFile(FILE *fp);
  void WriteToFile(FILE *fp);
  //
  // Map the chip into the 6502 address space
  bool MapChip(class MMU *mmu,UBYTE activebank);
  //
  // MemController interfaces: Setup the contents of this chip.
  virtual void Initialize(void);
  //
  // Chip interfaces, for the monitor etc.
  //
  // Warmstart and ColdStart.
  virtual void WarmStart(void);
  virtual void ColdStart(void);
  //
  // Print the status of the chip over the monitor
  virtual void DisplayStatus(class Monitor *);
  //
  // Make a snapshot of this chip to the state file.
  virtual void State(class SnapShot *snap);
  //
  // Parse off the arguments for this chip.
  virtual void ParseArgs(class ArgParser *args);
};
///

///
#endif
