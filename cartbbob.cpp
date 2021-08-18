/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartbbob.cpp,v 1.11 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of the bounty bob cart
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "monitor.hpp"
#include "snapshot.hpp"
#include "cartbbob.hpp"
///

/// CartBBOB::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartBBOB::CartSizes[] = {40,0};
///

/// CartBBOB::CartBBOB
// Construct the cart. There is nothing to do here.
CartBBOB::CartBBOB(class MMU *mmu)
  : Bank40(mmu,0x00), Bank50(mmu,0x10)
{
}
///

/// CartBBOB::~CartBBOB
// Dispose the cart. There is nothing to dispose
CartBBOB::~CartBBOB(void)
{
}
///

/// CartBBOB::Initialize
// Initialize this memory controller, built its contents.
void CartBBOB::Initialize(void)
{
  Bank40.ActiveBank = 0;
  Bank50.ActiveBank = 0;
}
///

/// CartBBOB::BankPage::BankPage
// Constructor of the bounty bob mapping page.
CartBBOB::BankPage::BankPage(class MMU *mmu,UBYTE pageoffset)
  : MMU(mmu), PageOffset(pageoffset), ActiveBank(0)
{
}
///

/// CartBBOB::BankPage::~BankPage
// Destructor of a bounty bob mapping page
CartBBOB::BankPage::~BankPage(void)
{
}
///

/// CartBBOB::BankPage::ComplexRead
// Read from a bounty bob page. This almost surely returns the ROM contents,
// except for the special 0xf6..0xf9 data base.
UBYTE CartBBOB::BankPage::ComplexRead(ADR mem)
{
  UBYTE offset = UBYTE(mem & 0xff);
  //
  // Now check whether we access the banking registers.
  if (offset >= 0xf6 && offset <= 0xf9) {
    // This is a banking access. Perform the banking.
    SwitchBank(offset - 0xf6);
  } else if (offset == 0xfb) {
    // This returns the currently mapped page.
    return UBYTE((ActiveBank<<2) | PageOffset);
  }
  // Now return the contents of this ROM anyhow: Use the direct
  // call to make this at least somehow fast.
  return Hidden->ReadByte(mem);
}
///

/// CartBBOB::BankPage::ComplexWrite
// Write into the ROM, which is impossible. Just perform the
// banking if required. 
void CartBBOB::BankPage::ComplexWrite(ADR mem,UBYTE)
{
  UBYTE offset = UBYTE(mem & 0xff);
  //
  // Check whether we access the banking logic.
  if (offset >= 0xf6 && offset <= 0xf9) {
    // This is also a banking access.
    SwitchBank(offset - 0xf6);
  }
  // Since this is a ROM, we cannot write and can bail
  // out directly here without further note.
}
///

/// CartBBOB::BankPage::SwitchBank
// Switch the bounty bob cartridge are.
void CartBBOB::BankPage::SwitchBank(int newbank)
{
  if (newbank != ActiveBank) {
    ActiveBank = newbank;
    // let the MMU remap the cartridge area now.
    MMU->BuildCartArea();
  }
}
///

/// CartBBOB::CartType
// Return a string identifying the type of the cartridge.
const char *CartBBOB::CartType(void)
{
  return "BountyBob";
}
///

/// CartBBOB::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartBBOB::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 160; // total number of pages is 160, making 40K
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartBBOB::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartBBOB::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartBBOB::MapCart(class MMU *mmu)
{
  ADR mem;
  class RomPage *page;
  // Get the active bank of the first cart bank
  // and map it into 0x8000 to 0x8xfff. These are 4K,
  // or 16 pages.
  page = Rom + (Bank40.ActiveBank << 4); // each bank is 16 pages LONG.
  for(mem = 0x4000;mem < 0x4f00;mem += Page::Page_Length) {
    mmu->MapPage(mem,page);
    page++;
  }
  // Now map the bank switching page here as last part of the page.
  mmu->MapPage(0x4f00,&Bank40);
  // And let this page know the real page it hides.
  Bank40.Hidden = page;
  //
  // The very same, but this time for the upper bank.
  page = Rom + 16*4 + (Bank50.ActiveBank << 4); // each bank is again 16 pages LONG.
  for(mem = 0x5000;mem < 0x5f00;mem += Page::Page_Length) {
    mmu->MapPage(mem,page);
    page++;
  }
  // Again, map the second bank switching page in.
  mmu->MapPage(0x5f00,&Bank50);
  Bank50.Hidden = page;
  //
  // Finally, map the last 8K in
  page = Rom + 16*4*2;
  for(mem = 0xa000;mem < 0xc000;mem += Page::Page_Length) {
    mmu->MapPage(mem,page);
    page++;
  }
  //
  // Signal the cart as active.
  return true;
}
///

/// CartBBOB::DisplayStatus
// Display the status over the monitor.
void CartBBOB::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Active 0x4000 bank : %d\n"
		   "Active 0x5000 bank : %d\n",
		   CartType(),
		   Bank40.ActiveBank,
		   Bank50.ActiveBank);
}
///

/// CartBBOB::State
// Perform the snapshot operation for the CartCtrl unit.
void CartBBOB::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank.0","Bounty Bob cartridge first active bank selection" ,0,3,Bank40.ActiveBank);
  sn->DefineLong("SuperBank.1","Bounty Bob cartridge second active bank selection",0,3,Bank50.ActiveBank);
}
///
