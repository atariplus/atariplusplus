/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartatmax.cpp,v 1.4 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of an ATMax Supercart
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
#include "cartatmax.hpp"
#include "new.hpp"
///

/// CartATMax::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartATMax::CartSizes[] = {128,1024,0};
///

/// CartATMax::CartATMax
// Construct the cart. There is nothing to do here.
CartATMax::CartATMax(UBYTE banks)
  : Rom(new class RomPage[banks << 5]), Banks(banks)
  // A page is 256 bytes large, making 512*256 = 128K bytes
{
  if (banks == 128) { // The 1MB cart
    ActiveBank = 127;
  } else {
    ActiveBank = 0;
  }
  Disabled   = false;
}
///

/// CartATMax::~CartATMax
// Dispose the cart. There is nothing to dispose
CartATMax::~CartATMax(void)
{
  delete[] Rom;
}
///

/// CartATMax::Initialize
// Initialize this memory controller, built its contents.
void CartATMax::Initialize(void)
{  
  if (Banks == 128) { // The 1MB cart
    ActiveBank = 127;
  } else {
    ActiveBank = 0;
  }
  Disabled   = false;
}
///

/// CartATMax::CartType
// Return a string identifying the type of the cartridge.
const char *CartATMax::CartType(void)
{
  return "ATMax";
}
///

/// CartATMax::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartATMax::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = ULONG(Banks) << 5;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartATMax::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartATMax::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartATMax::MapCart(class MMU *mmu)
{
  if (!Disabled) {
    ADR i;
    int displacement;
    // Get the first bank and map it into 0xa000 to 0xbfff
    // 8K = 2^13
    displacement = (ActiveBank << 13) - 0xa000;
    for(i=0xa000;i<0xc000;i+=Page::Page_Length) {
      mmu->MapPage(i,Rom+((i+displacement)>>Page::Page_Shift));
    }
    return true;
  }
  return false;
}
///

/// CartATMax::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartATMax::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  int  newbank     = 0;
  bool newdisabled;
  //
  // The value we write does not matter, only the address within
  // cartCtrl.
  newbank = mem & 0xff;
  if (newbank < (int(Banks) << 1)) {
    if (newbank & Banks) {
      newdisabled = true;
    } else {
      newdisabled = false;
    }
    // Get the bank to map in, from the lower nibble.
    newbank = mem & (Banks - 1);
    // This will/might require rebuilding now
    if (newdisabled != Disabled || newbank != ActiveBank) {
      ActiveBank = newbank;
      Disabled   = newdisabled;
      mmu->BuildCartArea();
    }
    //
    // Only react on the lower part of CartCtrl.
    return true;
  }
  return false;
}
///

/// CartATMax::DisplayStatus
// Display the status over the monitor.
void CartATMax::DisplayStatus(class Monitor *mon)
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

/// CartATMax::State
// Perform the snapshot operation for the CartCtrl unit.
void CartATMax::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","ATMax cartridge active bank selection",0,511,ActiveBank);
  sn->DefineBool("CartDisabled","ATMax cartridge disable flag",Disabled);
}
///
