/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: ramextension.hpp,v 1.6 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Base class for all ram extension classes
 **********************************************************************************/

#ifndef RAMEXTENSION_HPP
#define RAMEXTENSION_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "saveable.hpp"
///

/// Forward References
class MMU;
class AdrSpace;
class ArgParser;
class Machine;
class Monitor;
///

/// Class RamExtension
// This class defines the base class for all
// RAM extensions, to be mapped in by the MMU class.
// This class itself is not a self-contained chip, hence
// does not configure itself, but is rather a helper
// class for MMU and PIA.
class RamExtension : public Saveable, public Node<class RamExtension> {
protected:
  //
  // This is a service for all subclasses.
  class MMU *MMU;
  //
public:
  RamExtension(class Machine *mach,const char *name);
  ~RamExtension(void);
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
  virtual bool MapExtension(class AdrSpace *adr,bool forantic) = 0;
  //
  // Map in/replace a page in RAM to add here a ram extension specific
  // IO page. This is required for all AXLON compatible RAM disks that
  // expect a custom IO entry at 0xcfff.
  // It returns true if such a mapping has been performed. The default
  // is that no such mapping is required.
  virtual bool MapControlPage(class AdrSpace *,class Page *)
  {
    return false;
  }
  //
  // The following method is called by PIA whenever a write into PORTB
  // is made and hence the RAM disk *might* want to perform a
  // remapping. It returns true in case the call was relevant for this
  // disk. The default is that it is not. It requires a reference
  // since some "captured" write bits have to be altered to bypass the
  // usual PIA/MMU mapping.
  virtual bool PIAWrite(UBYTE &)
  {
    return false;
  }
  //
  // Reset the RAM extension. This should reset the banking.
  virtual void ColdStart(void) = 0;
  virtual void WarmStart(void) = 0;
  //
  // Parse the configuration of the RAM disk. This is called as
  // part of the MMU setup and should hence not define a new
  // title.
  virtual void ParseArgs(class ArgParser *args) = 0;
  //
  // Load/save the machine state of the RAM within here.
  // This is part of the saveable interface.
  virtual void State(class SnapShot *snap) = 0;
  //
  // Display the machine state of this extension for the monitor.
  // This is called as part of the MMU status.
  virtual void DisplayStatus(class Monitor *monitor) = 0;
  //
  // List/Node implementations to get the next node in a list of
  // ram disks and to make definite what "next" in which list
  // we mean.
  class RamExtension *NextOf(void) const
  {
    return Node<RamExtension>::NextOf();
  }
  class RamExtension *PrevOf(void) const
  {
    return Node<RamExtension>::PrevOf();
  }
  void Remove(void)
  {
    Node<RamExtension>::Remove();
  }
};
///

///
#endif
