/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartsdx.cpp,v 1.15 2021/08/16 10:31:01 thor Exp $
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
#include "cartsdx.hpp"
///

/// CartSDX::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartSDX::CartSizes[] = {64,0};
///

/// CartSDX::CartSDX
// Construct the cart. There is nothing to do here.
CartSDX::CartSDX(UBYTE ctrl)
  : ControlAddr(ctrl)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartSDX::~CartSDX
// Dispose the cart. There is nothing to dispose
CartSDX::~CartSDX(void)
{
}
///

/// CartSDX::Initialize
// Initialize this memory controller, built its contents.
void CartSDX::Initialize(void)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartSDX::CartType
// Return a string identifying the type of the cartridge.
const char *CartSDX::CartType(void)
{
  return "SDX";
}
///

/// CartSDX::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartSDX::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 256;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartSDX::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartSDX::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartSDX::MapCart(class MMU *mmu)
{
  ADR i;

  if (Disabled == false) {
    int displacement = (ActiveBank << 13) - 0xa000;
    for(i=0xa000;i<0xc000;i+=Page::Page_Length) {
      mmu->MapPage(i,Rom+(((i+displacement)>>Page::Page_Shift)));
    }
    return true;
  }
  return false;
}
///

/// CartSDX::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartSDX::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  //
  // Check whether we got the right offset here.
  if ((mem & 0xf0) == ControlAddr) {  
    int  newbank     = ActiveBank;
    bool newdisabled = Disabled;
    // Indeed, a write into the cartridge control space.
    if (mem & 0x08) {
      // This is the cart-disable line. Got hit, so disable me.
      newdisabled = true;
    } else {
      // Cart remains enabled. Mapping is a bit strange, but
      // that's life. It's upside down...
      newdisabled = false;
      newbank     = (~mem) & 0x07;
    }
    //
    // This will/might require rebuilding now
    if (newbank != ActiveBank || newdisabled != Disabled) {
      Disabled   = newdisabled;
      ActiveBank = newbank;
      mmu->BuildCartArea();
    }
    // This cart accepts this write.
    return true;
  }
  //
  // All other writes are ignored.
  return false;
}
///

/// CartSDX::DisplayStatus
// Display the status over the monitor.
void CartSDX::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Cart disabled      : %s\n"
		   "Active bank        : %d\n",
		   CartType(),
		   Disabled?("yes"):("no"),
		   ActiveBank);
}
///

/// CartSDX::State
// Perform the snapshot operation for the CartCtrl unit.
void CartSDX::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","SDX cartridge active bank selection",0,3,ActiveBank);
  sn->DefineBool("CartDisabled","SDX cartridge disable flag",Disabled);
}
///
