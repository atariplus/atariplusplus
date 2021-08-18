/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: axlonextension.cpp,v 1.9 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: This RAM extension implements various AXLON compatible
 ** RAM extensions.
 **********************************************************************************/

/// Includes
#include "types.hpp"
#include "axlonextension.hpp"
#include "mmu.hpp"
#include "exceptions.hpp"
#include "snapshot.hpp"
#include "mmu.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "new.hpp"
///

/// AxlonExtension::AxlonExtension
// Construct the extension by first providing some defaults
// and then building the RAM.
AxlonExtension::AxlonExtension(class Machine *mach)
  : RamExtension(mach,"AxlonRamBanks"),
    RAM(new RamPage[256]), BankBits(2), MapAntic(false),
    ControlPage(MMU,0x03)
{
  int i,banks = 1<<BankBits;
  //
  // Reset all pages.
  for(i=0;i < banks;i++) {
    RAM[i].Blank();
  } 
}
///

/// AxlonExtension::~AxlonExtension
// Dispose this extension, most noticably, its RAM.
AxlonExtension::~AxlonExtension(void)
{
  delete[] RAM;
}
///

/// AxlonExtension::AxlonControlPage::AxlonControlPage
// Setup the control page at 0xcf00 and up for
// mapping.
AxlonExtension::AxlonControlPage::AxlonControlPage(class MMU *mmu,int mask)
  : Hidden(NULL), MMU(mmu), ActiveBank(0), BankMask(mask)
{
}
///

/// AxlonExtension::AxlonControlPage::~AxlonControlPage
// Dispose the axlon control page again. This does
// nothing useful right now.
AxlonExtension::AxlonControlPage::~AxlonControlPage(void)
{
}
///

/// AxlonExtension::AxlonControlPage::ComplexRead
// Emulate a read into the axlon page, i.e. into the
// memory area from 0xcf00 to 0xcfff.
UBYTE AxlonExtension::AxlonControlPage::ComplexRead(ADR mem)
{
  // We cannot map 0xcfff for reading as otherwise the XL
  // ROM checksum would be wrong. Yikes!
#if CHECK_LEVEL > 0
  if (Hidden == NULL)
    Throw(ObjectDoesntExist,"AxlonExtension::AxlonControlPage::ComplexRead",
	  "Axlon page is not mapped in, or no page this hides");
#endif
  return Hidden->ReadByte(mem);
}
///

/// AxlonExtension::AxlonControlPage::ComplexWrite
// Emulate a write access into the axlon control page,
// hence into the memory from 0xcf00 to 0xcfff. This may
// result in a request to change the banking.
void AxlonExtension::AxlonControlPage::ComplexWrite(ADR mem,UBYTE value)
{
#if CHECK_LEVEL > 0
  if (Hidden == NULL)
    Throw(ObjectDoesntExist,"AxlonExtension::AxlonControlPage::ComplexWrite",
	  "Axlon page is not mapped in, or no page this hides");
#endif
  // Check whether this write is into the bank register.
  if ((mem & 0xff) == 0xff) {
    int bank = value & BankMask;
    //
    if (bank != ActiveBank) {
      // Well, we have to map it now. Let the MMU do this for
      // us. This will call the bank mapping method of the
      // parent AxlonExtension class.
      ActiveBank = bank;
      MMU->BuildMedRam();
    }
  }
  //
  // Otherwise, perform the write into the RAM/ROM/Whatever as
  // normal.
  Hidden->WriteByte(mem,value);
}
///

/// AxlonExtension::AxlonControlPage::isIOSpace
// Return an indicator whether this is an I/O area or not.
// This is used by the monitor to check whether reads are harmless
bool AxlonExtension::AxlonControlPage::isIOSpace(ADR mem) const
{
  if ((mem & 0xff) == 0xff)
    return true;
  return false;
}
///

/// AxlonExtension::MapExtension
// The following method is used to map the RAM disk into the
// RAM area. It is called by the MMU as part of the medium
// RAM area setup. We hence expect that ram extensions are part
// of the 0x4000..0x8000 area, which is true for all RAM extensions
// the author is aware of. If this call returns false, no RAM disk
// is mapped into the area and the MMU has to perform the mapping
// of default RAM. Further, this might be called several times
// once for ANTIC and once for the CPU. The antic argument tells
// us whether we map for CPU or antic.
bool AxlonExtension::MapExtension(class AdrSpace *adr,bool forantic)
{
  ADR i;
  class RamPage *ram;
  // Check whether we are antic. If so, and if antic access goes
  // not thru the bank, then perform no mapping at all.
  if (forantic && MapAntic == false)
    return false;
  // Bank #0 is the regular RAM. (Now, is this right?)
  if (ControlPage.ActiveBank == 0)
    return false;
  //
  // Otherwise: Each bank is 16K in size and starts at 0x4000.
  ram = RAM + ((ControlPage.ActiveBank & ControlPage.BankMask) << 6);
  for(i = 0x4000;i < 0x8000; i += Page::Page_Length) {
    adr->MapPage(i,ram);
    ram++;
  }
  return true;
}
///

/// AxlonExtension::MapControlPage
// Map in/replace a page in RAM to add here a ram extension specific
// IO page. This is required for all AXLON compatible RAM disks that
// expect a custom IO entry at 0xcfff.
// It returns true if such a mapping has been performed. The default
// is that no such mapping is required.
bool AxlonExtension::MapControlPage(class AdrSpace *adr,class Page *cfpage)
{
  // Hide this page under the IO control page, i.e. all "regular" accesses go
  // back to the hidden page.
  ControlPage.Hidden = cfpage;
  // Map this page into the address space.
  adr->MapPage(0xcf00,&ControlPage);
  //
  // We always stay active once installed
  return true;
}
///

/// AxlonExtension::ColdStart
// Reset the RAM extension. This should reset the banking.
// We do not emulate the bug of various AXLON implementations
// not to reset the RAM.
void AxlonExtension::ColdStart(void)
{
  int i,banks = 1<<BankBits;
  //
  // Reset all pages.
  for(i=0;i < banks;i++) {
    RAM[i].Blank();
  }
  WarmStart();
}
///

/// AxlonExtension::WarmStart
// Reset the RAM extension. This should reset the banking.
// We do not emulate the bug of various AXLON implementations
// not to reset the RAM.
void AxlonExtension::WarmStart(void)
{
  ControlPage.ActiveBank = 0;
}
///

/// AxlonExtension::ParseArgs
// Parse the configuration of the RAM disk. This is called as
// part of the MMU setup and should hence not define a new
// title.
void AxlonExtension::ParseArgs(class ArgParser *args)
{
  LONG bankbits = BankBits;

  args->DefineLong("AxlonBankBits","number of utilized 0xcfff bits for Axlon bank switching",0,8,bankbits);
  args->DefineBool("AxlonAnticAccess","route Antic accesses to the Axlon RAM extension",MapAntic);

  // Check whether the bank bits changed. If so, we need to rebuild the memory. Yikes.
  if (bankbits != BankBits) {
    int i,banks = 1<<bankbits;
    BankBits                = bankbits;
    ControlPage.BankMask    = (1<<bankbits)-1;
    ControlPage.ActiveBank &= ControlPage.BankMask; // avoid out-of-range bit settings;
    //
    // Rebuild the RAM now. This also looses its contents.
    delete[] RAM;
    RAM = NULL;
    RAM = new RamPage[banks<<6];
    //  
    // Reset all pages.
    for(i=0;i < banks;i++) {
      RAM[i].Blank();
    } 
    // This requires a cold-start since we invalidate memory.
    args->SignalBigChange(ArgParser::ColdStart);
  }
}
///

/// AxlonExtension::State
// Load/save the machine state of the RAM within here.
// This is part of the saveable interface.
void AxlonExtension::State(class SnapShot *snap)
{
  int i,banks = 1<<BankBits;
  char id[32],helptxt[80];

  snap->DefineTitle("AxlonBanking");
  snap->DefineLong("ActiveBank","currently active bank",0,banks,ControlPage.ActiveBank);
  // Just for savety:
  ControlPage.ActiveBank &= ControlPage.BankMask;
  // In case we just loaded the configuration, let the MMU rebuild the relevant parts
  // of the memory map.
  MMU->BuildMedRam();
  //
  // Now save the RAM contents.
  snap->DefineTitle("AxlonRAM");
  for(i = 0;i<banks;i++) {
    snprintf(id,31,"Page%d",i);
    snprintf(helptxt,79,"Axlon extra RAM page %d contents",i);
     snap->DefineChunk(id,helptxt,RAM[i].Memory(),256);
  }
}
///

/// AxlonExtension::DisplayStatus
// Display the machine state of this extension for the monitor.
// This is called as part of the MMU status.
void AxlonExtension::DisplayStatus(class Monitor *monitor)
{
  monitor->PrintStatus("\tAxlon ANTIC access    : %s\n"
		       "\tAxlon number of banks : %d\n"
		       "\tAxlon active bank     : %d\n",
		       (MapAntic)?("on"):("off"),
		       1<<BankBits,
		       ControlPage.ActiveBank);
}
///
