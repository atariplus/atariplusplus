/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartatrax.cpp,v 1.6 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of an Atrax Supercart
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
#include "cartatrax.hpp"
#include "new.hpp"
///

/// CartAtrax::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartAtrax::CartSizes[] = {128,0};
///

/// CartAtrax::CartAtrax
// Construct the cart. There is nothing to do here.
CartAtrax::CartAtrax(void)
  : Rom(new class RomPage[512])
  // A page is 256 bytes large, making 512*256 = 128K bytes
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartAtrax::~CartAtrax
// Dispose the cart. There is nothing to dispose
CartAtrax::~CartAtrax(void)
{
  delete[] Rom;
}
///

/// CartAtrax::Initialize
// Initialize this memory controller, built its contents.
void CartAtrax::Initialize(void)
{ 
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartAtrax::CartType
// Return a string identifying the type of the cartridge.
const char *CartAtrax::CartType(void)
{
  return "Atrax";
}
///

/// CartAtrax::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartAtrax::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 512; // number of pages to read
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartAtrax::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartAtrax::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartAtrax::MapCart(class MMU *mmu)
{
  if (!Disabled) {
    ADR i;
    int displacement;
    // Get the first bank and map it into 0xa000 to 0xbfff
    // 8K = 2^13
    displacement = (ActiveBank << 13) - 0xa000;
    for(i=0xa000;i<0xc000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom+((i+displacement)>>PAGE_SHIFT));
    }
    return true;
  }
  return false;
}
///

/// CartAtrax::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartAtrax::ComplexWrite(class MMU *mmu,ADR mem,UBYTE val)
{  
  int  newbank     = 0;
  bool newdisabled;
  //
  // It does not matter which address we write to. Here, the value
  // maps to the bank we want to map in. Hmm. We map in a blank
  // page in case the value is out of range.
  // If bit 7 is set, then disable the cart.
  if (val & 0x80) {
    newdisabled = true;  // Actually, only for some Atrax carts.
  } else {
    newdisabled = false;
  }
  // Get the bank to map in.
  newbank = val & 0x0f;
  // This will/might require rebuilding now
  if (newdisabled != Disabled || newbank != ActiveBank) {
    ActiveBank = newbank;
    Disabled   = newdisabled;
    mmu->BuildCartArea();
  }
  //
  // This cart only reacts to writes into the byte 0xd500. Everything
  // else is understood as not part of this cart.
  return (mem == 0xd500);
}
///

/// CartAtrax::DisplayStatus
// Display the status over the monitor.
void CartAtrax::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Active bank        : %d\n"
		   "Cart disabled      : %s\n",
		   CartType(),
		   ActiveBank,
		   Disabled?("yes"):("no")
		   );
}
///

/// CartAtrax::State
// Perform the snapshot operation for the CartCtrl unit.
void CartAtrax::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","Atrax cartridge active bank selection",0,511,ActiveBank);
  sn->DefineBool("CartDisabled","Atrax cartridge disable flag",Disabled);
}
///
