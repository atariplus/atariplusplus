/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartdb32.cpp,v 1.5 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of the DB32 Supercart
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
#include "cartdb32.hpp"
#include "new.hpp"
///

/// CartDB32::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartDB32::CartSizes[] = {32,0};
///

/// CartDB32::CartDB32
// Construct the cart. There is nothing to do here.
CartDB32::CartDB32(void)
{
  ActiveBank = 0;
}
///

/// CartDB32::~CartDB32
// Dispose the cart. There is nothing to dispose
CartDB32::~CartDB32(void)
{
}
///

/// CartDB32::Initialize
// Initialize this memory controller, built its contents.
void CartDB32::Initialize(void)
{ 
  ActiveBank = 0;
}
///

/// CartDB32::CartType
// Return a string identifying the type of the cartridge.
const char *CartDB32::CartType(void)
{
  return "DB32";
}
///

/// CartDB32::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartDB32::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 128; // number of pages to read = 32K
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartDB32::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartDB32::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartDB32::MapCart(class MMU *mmu)
{
  ADR i;
  int displacement;
  // Get the first bank and map it into 0x8000 to 0x9fff
  // 8K = 2^13
  displacement = (ActiveBank << 13) - 0x8000;
  for(i=0x8000;i<0xa000;i+=PAGE_LENGTH) {
    mmu->MapPage(i,Rom+((i+displacement)>>PAGE_SHIFT));
  }
  // Now map the last bank into the area of 0xa000 and up. This means
  // that the last bank could appear twice, once in the 0x8000 area,
  // and once here.
  displacement  = (3 << 13) - 0xa000;
  for(i=0xa000;i<0xc000;i+=PAGE_LENGTH) {
    mmu->MapPage(i,Rom+((i+displacement)>>PAGE_SHIFT));
  }
  return true;
}
///

/// CartDB32::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartDB32::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  int  newbank     = ActiveBank;
  //
  // The bank is given by modulo arithmetic
  // on the address.
  newbank          = (mem & 0x03);
  //
  // This will/might require rebuilding now
  if (newbank != ActiveBank) {
    ActiveBank     = newbank;
    mmu->BuildCartArea();
  }
  //
  // This cart only reacts to writes into the byte 0xd500 to 0xd503. Everything
  // else is understood as not part of this cart.
  if (mem >= 0xd500 && mem < 0xd504)
    return true;
  return false;
}
///

/// CartDB32::DisplayStatus
// Display the status over the monitor.
void CartDB32::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Active bank        : %d\n",
		   CartType(),
		   ActiveBank
		   );
}
///

/// CartDB32::State
// Perform the snapshot operation for the CartCtrl unit.
void CartDB32::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","DB32 cartridge active bank selection",0,3,ActiveBank);
}
///
