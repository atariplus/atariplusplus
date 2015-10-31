/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: xeextension.hpp,v 1.7 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: This RAM extension implements the 130XE 64K of
 ** additional RAM.
 **********************************************************************************/

#ifndef XEEXTENSION_HPP
#define XEEXTENSION_CPP

/// Includes
#include "types.hpp"
#include "ramextension.hpp"
#include "rampage.hpp"
///

/// Class XEExtension
// This class implements the 130XE 64K extra mem
// and its bank-switching mechanism.
// It is built by the MMU whenever the machine type
// is 130XE (clearly).
class XEExtension : public RamExtension {
  //
  // The Extra RAM is here: 256 pages a 256 bytes.
  class RamPage *RAM;
  //
  // The page the CPU resp. ANTIC has access to.
  UBYTE CPUBank,AnticBank;
  //
  // Whether CPU or ANTIC have access to these pages.
  bool CPUAccess,AnticAccess;
  //
  // The array that defines the bits of PIA port B that give
  // the bank #.
  static const UBYTE BankBits[8];
  //
  // The number of bits of PIA we spend for addressing the banks.
  // The more bits we allocate here, the more other features
  // will break.
  LONG PIABankBits;
  //
public:
  XEExtension(class Machine *mach);
  ~XEExtension(void);
  //
  // The following method is used to map the RAM disk into the
  // RAM area. It is called by the MMU as part of the medium
  // RAM area setup. We hence expect that ram extensions are part
  // of the 0x4000..0x8000 area, which is true for all RAM extensions
  // the author is aware of. If this call returns false, no RAM disk
  // is mapped into the area and the MMU has to perform the mapping
  // of default RAM. Further, this might be called several times
  // once for ANTIC and once for the CPU. The antic argument tells
  // us whether we map for CPU or antic.
  virtual bool MapExtension(class AdrSpace *adr,bool forantic);
  //
  // The following method is called by PIA whenever a write into PORTB
  // is made and hence the RAM disk *might* want to perform a
  // remapping. It returns true in case the call was relevant for this
  // disk. The default is that it is not.
  virtual bool PIAWrite(UBYTE &data);
  //
  // Reset the RAM extension. This should reset the banking.
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Parse the configuration of the RAM disk. This is called as
  // part of the MMU setup and should hence not define a new
  // title.
  virtual void ParseArgs(class ArgParser *args);
  //
  // Load/save the machine state of the RAM within here.
  // This is part of the saveable interface.
  virtual void State(class SnapShot *snap);
  //
  // Display the machine state of this extension for the monitor.
  // This is called as part of the MMU status.
  virtual void DisplayStatus(class Monitor *monitor);
};
///

///
#endif
