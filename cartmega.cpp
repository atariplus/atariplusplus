/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartmega.cpp,v 1.8 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of an MEGA Supercart
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
#include "cartmega.hpp"
#include "new.hpp"
///

/// CartMega::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartMEGA::CartSizes[] = {16,32,64,128,256,512,1024,0};
///

/// CartMEGA::CartMEGA
// Construct the cart. There is nothing to do here.
CartMEGA::CartMEGA(UBYTE banks)
  : Rom(new class RomPage[banks << 6]), TotalBanks(banks) 
  // A page is 256 bytes large, making 64*256 = 16384 bytes
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartMEGA::~CartMEGA
// Dispose the cart. There is nothing to dispose
CartMEGA::~CartMEGA(void)
{
  delete[] Rom;
}
///

/// CartMEGA::Initialize
// Initialize this memory controller, built its contents.
void CartMEGA::Initialize(void)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartMEGA::CartType
// Return a string identifying the type of the cartridge.
const char *CartMEGA::CartType(void)
{
  return "Mega";
}
///

/// CartMEGA::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartMEGA::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = TotalBanks << 6; // number of pages to read as each bank is 16K in size
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartMEGA::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartMEGA::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartMEGA::MapCart(class MMU *mmu)
{
  ADR i;
  //
  if (!Disabled) {
    int displacement;
    // Get the bank and map it into 0x8000 to 0xafff
    // 16K = 2^14
    displacement = (ActiveBank << 14) - 0x8000;
    for(i=0x8000;i<0xc000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom+((i+displacement)>>PAGE_SHIFT));
    }
    return true;
  }
  return false;
}
///

/// CartMEGA::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartMEGA::ComplexWrite(class MMU *mmu,ADR mem,UBYTE val)
{  
  int  newbank;
  bool newdisable;
      
  // It does not matter which address we write to. Here, the value
  // maps to the bank we want to map in. 
  newbank = (TotalBanks - 1) & val;
  if (val & 0x80) {
    // Bit 7 disables the cart.
    newdisable = true;
  } else {
    newdisable = false;
  }
  //
  // This will/might require rebuilding now
  if (newbank    != ActiveBank || 
      newdisable != Disabled) {
    ActiveBank = newbank;
    Disabled   = newdisable;
    mmu->BuildCartArea();
  }
  //
  // This cart only reacts to writes into the byte 0xd500. Everything
  // else is understood as not part of this cart and will get forwarded
  // for example to the real time cart.
  return (mem == 0xd500);
}
///

/// CartMEGA::DisplayStatus
// Display the status over the monitor.
void CartMEGA::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Number of banks    : %d\n"
		   "Active bank        : %d\n"
		   "Cart disabled      : %s\n",
		   CartType(),
		   TotalBanks,
		   ActiveBank,
		   Disabled?("yes"):("no"));
}
///

/// CartMEGA::State
// Perform the snapshot operation for the CartCtrl unit.
void CartMEGA::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","Mega cartridge active bank selection",0,TotalBanks-1,ActiveBank);  
  sn->DefineBool("CartDisabled","Mega cartridge disable flag",Disabled);
}
///
