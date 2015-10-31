/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartphoenix.cpp,v 1.3 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of the Phoenix and Blizzard carts
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
#include "cartphoenix.hpp"
#include "new.hpp"
///

/// CartPhoenix::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartPhoenix::CartSizes[] = {8,16,0};
///

/// CartPhoenix::CartPhoenix
// Construct the cart. There is nothing to do here.
CartPhoenix::CartPhoenix(UBYTE banks)
  : Rom(new class RomPage[banks << 5]), Banks(banks)
  // A page is 256 bytes large, making 512*256 = 128K bytes
{
  Disabled   = false;
}
///

/// CartPhoenix::~CartPhoenix
// Dispose the cart. There is nothing to dispose
CartPhoenix::~CartPhoenix(void)
{
  delete[] Rom;
}
///

/// CartPhoenix::Initialize
// Initialize this memory controller, built its contents.
void CartPhoenix::Initialize(void)
{ 
  Disabled   = false;
}
///

/// CartPhoenix::CartType
// Return a string identifying the type of the cartridge.
const char *CartPhoenix::CartType(void)
{
  if (Banks == 1)
    return "Phoenix";
  return "Blizzard";
}
///

/// CartPhoenix::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartPhoenix::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = Banks << 5;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartPhoenix::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartPhoenix::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartPhoenix::MapCart(class MMU *mmu)
{
  if (!Disabled) {
    ADR i;
    // Get the first bank and map it into 0xa000 to 0xbfff
    // 8K = 2^13
    if (Banks == 1) {
      for(i=0xa000;i<0xc000;i+=PAGE_LENGTH) {
	mmu->MapPage(i,Rom+((i-0xa000)>>PAGE_SHIFT));
      }
    } else {
      for(i=0x8000;i<0xc000;i+=PAGE_LENGTH) {
	mmu->MapPage(i,Rom+((i-0x8000)>>PAGE_SHIFT));
      }
    }
    return true;
  }
  return false;
}
///

/// CartPhoenix::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartPhoenix::ComplexWrite(class MMU *mmu,ADR,UBYTE)
{
  // This cart disables itself on a write into the CartCtrl area.
  if (!Disabled) {
    Disabled = true;
    mmu->BuildCartArea();
  }
  //
  // This cart reacts on all writes into CartCtrl.
  return true;
}
///

/// CartPhoenix::DisplayStatus
// Display the status over the monitor.
void CartPhoenix::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Cart disabled      : %s\n",
		   CartType(),
		   Disabled?("yes"):("no")
		   );
}
///

/// CartPhoenix::State
// Perform the snapshot operation for the CartCtrl unit.
void CartPhoenix::State(class SnapShot *sn)
{
  sn->DefineBool("CartDisabled","Phoenix/Blizzard cartridge disable flag",Disabled);
}
///
