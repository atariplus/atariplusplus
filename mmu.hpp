/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mmu.hpp,v 1.32 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of all MMU functions required for the Atari emulator
 **********************************************************************************/

#ifndef MMU_HPP
#define MMU_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "exceptions.hpp"
#include "adrspace.hpp"
#include "debugadrspace.hpp"
#include "argparser.hpp"
#include "chip.hpp"
#include "memcontroller.hpp"
#include "saveable.hpp"
#include "osrom.hpp"
#include "basicrom.hpp"
#include "ram.hpp"
///

/// Forward references
class GTIA;
class ParPort;
class Pokey;
class PIA;
class Antic;
class CartCtrl;
class Cartridge;
class Monitor;
class CartROM;
class RamExtension;
class XEExtension;
class AxlonExtension;
class Machine;
///

/// Class MMU
// Definition of the MMU management, handles all kinds
// of memory, defines memory mappings and much more.
class MMU : public Chip, public Saveable, public MemController {
  // 
  // The link to the Os ROM, Basic ROM and Cartridge ROM
  class OsROM     *osrom;
  class BasicROM  *basicrom;
  class CartROM   *cartrom;
  class CartCtrl  *cartctrl;
  class RAM       *ram;        // up to 512 pages RAM = 128K
  class RomPage   *blank;      // up to 8K blank undefined ROM
  class RomPage   *handlers;   // used by the emulator to patch-in CIO extensions
  //
  class AdrSpace  *cpuspace;       // memory as visible from the CPU
  class AdrSpace  *anticspace;     // memory as visible from ANTIC
  class DebugAdrSpace *debugspace; // debug memory, otherwise the CPU space
  //
  // The list of currently active RAM extensions (including the 130XE RAM)
  List<RamExtension> Extensions;
  //
  // Some individual extensions we may or may not construct, depending on the
  // machine type.
  class XEExtension    *xeram;
  class AxlonExtension *axlonram;
  //
  //
  // Various flags required for miscellaneous control functions
  bool basic_mapped;    // true if basic is enabled
  bool rom_disabled;    // true if OS ROM is disabled
  bool selftest_mapped; // true if selftest is enabled
  bool mathpack_disable;// true if the mathpack ROM is disabled
  bool extended_4K;     // true if additional 4K for 800's enabled.
  bool axlon;           // true if axlon RAM extension should be made available
  //
  // Create all necessary RAM extensions for the MMU
  bool BuildExtensions(void);
  // Remove MMU/RAM Extensions no longer required. This
  // must happen after all options have been parsed or
  // RAM contents is lost in repeated parsing.
  void RemoveExtensions(void);
  //
public:
  MMU(class Machine *mach);
  ~MMU(void);
  //
  // Map a page to the CPU address space
  void MapCPUPage(ADR mem,class Page *page)
  {
    cpuspace->MapPage(mem,page);
  }
  //
  // Map a page to the ANTIC address space
  void MapANTICPage(ADR mem,class Page *page)
  {
    anticspace->MapPage(mem,page);
  }
  //
  // Map a page to both CPU and ANTIC address space
  void MapPage(ADR mem,class Page *page)
  {
    cpuspace->MapPage(mem,page);
    anticspace->MapPage(mem,page);
  }
  //
  // Parse arguments off 
  void ParseArgs(class ArgParser *args);
  //
  // Print the settings of the MMU class
  void DisplayStatus(class Monitor *mon);
  //
  // Build all address spaces given the internal state machine
  void BuildRamRomMapping(void);
  //
  // selective building methods follow for optimized bank switching
  // effords
  //
  // Build mapping 0x0000 to 0x4000 RAM
  void BuildLowRam(void);
  //
  // Build the mapping from 0x4000 to 0x8000
  void BuildMedRam(void);
  //
  // Build the mapping from 0x8000 to 0xc000
  void BuildCartArea(void);
  //
  // Build all areas except selftest that
  // could be mapped by ROM. This is 0xc000 to 0xd000
  // and 0xd800 to 0x10000
  void BuildOsArea(void);
  //
  // Return whether GTIA TRIG3 will return a CART_LOADED flag here.
  // Return false if no cart is loaded. Return true if so.
  // NOTE! An disabled Oss super cart will show up here as
  // no cart whatsoever.
  bool Trig3CartLoaded(void);
  //
  // Reset routines for the MMU
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //  
  // Read or set the internal status
  virtual void State(class SnapShot *);
  //
  // Select the basic RAM for the XL and XE models
  void SelectXLBasic(bool on)
  {
    if (on && basicrom->BasicLoaded()) {
      basic_mapped = true;
    } else {
      basic_mapped = false;
    }
    BuildCartArea();
  }
  //
  // Enable or disable Os RAM for the XL and XE models
  void SelectXLOs(bool on)
  {
    if (on) {
      rom_disabled = false;
    } else {
      rom_disabled = true;
    }
    BuildOsArea();
  }
  //
  // Select the Self Test for XL and XE models
  void SelectXLSelftest(bool on)
  {
    if (on) {
      selftest_mapped = true;
    } else {
      selftest_mapped = false;
    }
    // Rebuild the memory mapping accordingly
    BuildMedRam();
  }
  //
  // Enable or disable the mathpack
  void SelectMathPack(bool on)
  {
    if (on) {
      mathpack_disable = false;
    } else {
      mathpack_disable = true;
    }
    // Rebuild the Os part of the ROM
    BuildOsArea();
  }
  //
  // Return the address space of Antic
  class AdrSpace *AnticRAM(void) const
  {
    return anticspace;
  }
  //
  // Return the address space of the CPU
  class AdrSpace *CPURAM(void) const
  {
    return cpuspace;
  }
  //
  // Return an address space copy for debug.
  class DebugAdrSpace *DebugRAM(void) const
  {
    return debugspace;
  }
  //
  // Return the currently active Os type
  OsROM::OsType ActiveOsType(void) const
  {
    return osrom->RomType();
  }
  //  
  // Import this single method: Initialize us.
  virtual void Initialize(void);
  //
  // Return the list head of the first RAM extension. This is required
  // for the PIA managed RAM extensions and hence called by the PIA
  // class
  class RamExtension *FirstExtension(void) const
  {
    return Extensions.First();
  }
};

#endif
///
