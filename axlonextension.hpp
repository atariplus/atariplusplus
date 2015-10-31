/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: axlonextension.hpp,v 1.6 2015/10/25 09:11:22 thor Exp $
 **
 ** In this module: This RAM extension implements various AXLON compatible
 ** RAM extensions.
 **********************************************************************************/

#ifndef AXLONEXTENSION_HPP
#define AXLONEXTENSION_CPP

/// Includes
#include "types.hpp"
#include "ramextension.hpp"
#include "page.hpp"
#include "rampage.hpp"
///

/// Class AxlonExtension
// This class implements various RAM extensions
// that perform bank switching by 0xcfff accesses
class AxlonExtension: public RamExtension {
  //
  // The Extra RAM is here, though we have to 
  // allocate it dynamically as we don't have the size
  // of the RAM in pages. Default is, however, 4 banks,
  // hence 256 pages.
  class RamPage *RAM;
  //
  // The user-definable value: The number of bits
  // reserved in the banking register.
  LONG BankBits;
  //
  // The following boolean must be set whenever ANTIC
  // should be granted access to the extra RAM.
  bool MapAntic;
  //
  // The following is the special control page that goes
  // into the 0xcf00 area. It passes access to all bytes
  // except for 0xcf00 thru.
  class AxlonControlPage : public Page {
  public:
    // The page we mirror. Hence, all accesses that are
    // not for the IO address go thru to here.
    class Page *Hidden;
  private:
    //
    // The MMU that performs the mapping. We need it on
    // bank switches.
    class MMU  *MMU;
    //
  public:
    // The active bank. One of the above.
    // We keep it here since we must know up upon access
    // of this page.
    UBYTE ActiveBank;
    //
    // The bank mask, defines how many banks we have.
    UBYTE BankMask;
    //
    // For contruction, we need the MMU here. The hidden page
    // cannot be inserted until we map this guy and know what
    // we hide.
    AxlonControlPage(class MMU *mmu,int mask);
    ~AxlonControlPage(void);
    //
    // The read/write access pages. This may trigger the
    // IO register, but might get thru to the hidden page
    // directly.
    virtual UBYTE ComplexRead(ADR mem);
    virtual void  ComplexWrite(ADR mem,UBYTE value);
    //
    // Return an indicator whether this is an I/O area or not.
    // This is used by the monitor to check whether reads are harmless
    virtual bool isIOSpace(ADR mem) const;
  } ControlPage;
  //
  //
public:
  AxlonExtension(class Machine *mach);
  ~AxlonExtension(void);
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
  // Map in/replace a page in RAM to add here a ram extension specific
  // IO page. This is required for all AXLON compatible RAM disks that
  // expect a custom IO entry at 0xcfff.
  // It returns true if such a mapping has been performed. The default
  // is that no such mapping is required.
  virtual bool MapControlPage(class AdrSpace *adr,class Page *cfpage);
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
