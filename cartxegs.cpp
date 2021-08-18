/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartxegs.cpp,v 1.17 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of an XEGS Supercart
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
#include "cartxegs.hpp"
#include "new.hpp"
///

/// CartXEGS::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartXEGS::CartSizes[] = {32,64,128,256,512,1024,0};
///

/// CartXEGS::CartXEGS
// Construct the cart. There is nothing to do here.
CartXEGS::CartXEGS(UBYTE banks,bool switchable)
  : Rom(new class RomPage[banks << 5]), TotalBanks(banks),
    Switchable(switchable)
  // A page is 256 bytes large, making 32*256 = 8192 bytes
{
  ActiveBank = 0;
  Disabled   = false;
  Blank.Blank();
}
///

/// CartXEGS::~CartXEGS
// Dispose the cart. There is nothing to dispose
CartXEGS::~CartXEGS(void)
{
  delete[] Rom;
}
///

/// CartXEGS::Initialize
// Initialize this memory controller, built its contents.
void CartXEGS::Initialize(void)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartXEGS::CartType
// Return a string identifying the type of the cartridge.
const char *CartXEGS::CartType(void)
{
  return "XEGS";
}
///

/// CartXEGS::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartXEGS::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = TotalBanks << 5; // number of pages to read as each bank is 8K in size
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartXEGS::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartXEGS::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartXEGS::MapCart(class MMU *mmu)
{
  if (!Disabled) {
    ADR i;
    int displacement;
    // Get the first bank and map it into 0x8000 to 0x9fff
    // 8K = 2^13
    displacement = (ActiveBank << 13) - 0x8000;
    for(i=0x8000;i<0xa000;i+=Page::Page_Length) {
      mmu->MapPage(i,Rom+((i+displacement)>>Page::Page_Shift));
    }
    //
    // Now map the last bank into the area of 0xa000 and up.
    displacement  = ((TotalBanks - 1) << 13) - 0xa000;
    for(i=0xa000;i<0xc000;i+=Page::Page_Length) {
      mmu->MapPage(i,Rom+((i+displacement)>>Page::Page_Shift));
    }
    return true;
  }
  return false;
}
///

/// CartXEGS::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartXEGS::ComplexWrite(class MMU *mmu,ADR mem,UBYTE val)
{  
  UBYTE newbank     = ActiveBank;
  bool  newdisabled = Disabled;
  //
  // It does not matter which address we write to. Here, the value
  // maps to the bank we want to map in. Hmm. We map in a blank
  // page in case the value is out of range.
  // If bit 7 is set, then disable the cart if it is switchable.
  if (Switchable) {
    if (val & 0x80) {
      newdisabled  = true;  // Actually, only for some XEGS carts.
    } else {
      newdisabled  = false;
    }
  } else {
    newdisabled    = false;
  }
  //
  // The bank is given by modulo arithmetic.
  // TotalBanks is hopefully a power of two.
  newbank          = val & (TotalBanks - 1);
  //
  // This will/might require rebuilding now
  if (newdisabled != Disabled || newbank != ActiveBank) {
    ActiveBank     = newbank;
    Disabled       = newdisabled;
    mmu->BuildCartArea();
  }
  //
  // This cart only reacts to writes into the byte 0xd500. Everything
  // else is understood as not part of this cart.
  return (mem == 0xd500);
}
///

/// CartXEGS::DisplayStatus
// Display the status over the monitor.
void CartXEGS::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Number of banks    : %d\n"
		   "Active bank        : %d\n"
		   "Cart disabled      : %s\n",
		   CartType(),
		   TotalBanks,
		   ActiveBank,
		   Disabled?("yes"):("no")
		   );
}
///

/// CartXEGS::State
// Perform the snapshot operation for the CartCtrl unit.
void CartXEGS::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","XEGS cartridge active bank selection",0,TotalBanks-1,ActiveBank);
  sn->DefineBool("CartDisabled","XEGS cartridge disable flag",Disabled);
}
///
