/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartwill.cpp,v 1.5 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of an SDX Supercart
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
#include "cartwill.hpp"
///

/// CartWillCartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartWill::CartSizes[] = {32,64,0};
///

/// CartWill::CartWill
// Construct the cart. There is nothing to do here.
CartWill::CartWill(int banks)
  : Rom(new RomPage[banks<<5]), TotalBanks(banks)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartWill::~CartWill
// Dispose the cart. 
CartWill::~CartWill(void)
{
  delete[] Rom;
}
///

/// CartWill::Initialize
// Initialize this memory controller, built its contents.
void CartWill::Initialize(void)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartWill::CartType
// Return a string identifying the type of the cartridge.
const char *CartWill::CartType(void)
{
  return "WILL";
}
///

/// CartWill::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartWill::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = TotalBanks << 5; // A bank has 8K = 32 pages
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartWill::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartWill::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartWill::MapCart(class MMU *mmu)
{
  ADR i;

  if (Disabled == false) {
    int displacement = (ActiveBank << 13) - 0xa000;
    for(i=0xa000;i<0xc000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom+(((i+displacement)>>PAGE_SHIFT)));
    }
    return true;
  }
  return false;
}
///

/// CartWill::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartWill::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  int  newbank     = ActiveBank;
  bool newdisabled = Disabled;
  // Indeed, a write into the cartridge control space.
  if (mem & 0x08) {
    // This is the cart-disable line. Got hit, so disable me.
    newdisabled = true;
  } else {
    // Cart remains enabled. 
    newdisabled = false;
    newbank     = mem & (TotalBanks - 1);
  }
  //
  // This will/might require rebuilding now
  if (newbank != ActiveBank || newdisabled != Disabled) {
    Disabled   = newdisabled;
    ActiveBank = newbank;
    mmu->BuildCartArea();
  }
  // This cart accepts this write.
  return (mem == 0xd500);
}
///

/// CartWill::DisplayStatus
// Display the status over the monitor.
void CartWill::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Cart disabled      : %s\n"
		   "Active bank        : %d\n",
		   CartType(),
		   Disabled?("yes"):("no"),
		   ActiveBank);
}
///

/// CartWill::State
// Perform the snapshot operation for the CartCtrl unit.
void CartWill::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","WILL cartridge active bank selection",0,TotalBanks-1,ActiveBank);
  sn->DefineBool("CartDisabled","WILL cartridge disable flag",Disabled);
}
///
