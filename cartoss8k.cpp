/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartoss8k.cpp,v 1.2 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of an Oss Supercart
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
#include "cartoss8k.hpp"
///

/// CartOSS8K::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartOSS8K::CartSizes[] = {8,0};
///

/// CartOSS8K::CartOSS8K
// Construct the cart. There is nothing to do here.
CartOSS8K::CartOSS8K(void)
{
  Disabled   = false;
}
///

/// CartOSS8K::~CartOSS8K
// Dispose the cart. There is nothing to dispose
CartOSS8K::~CartOSS8K(void)
{
}
///

/// CartOSS8K::Initialize
// Initialize this memory controller, built its contents.
void CartOSS8K::Initialize(void)
{ 
  Disabled   = false;
}
///

/// CartOSS8K::CartType
// Return a string identifying the type of the cartridge.
const char *CartOSS8K::CartType(void)
{
  return "Oss8K";
}
///

/// CartOSS8K::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartOSS8K::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 32;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartOSS8K::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartOSS8K::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartOSS8K::MapCart(class MMU *mmu)
{
  ADR i;

  if (Disabled == false) {      
    // Map upper part of Oss cart now. This is in the lower part of
    // the ROM.
    for(i=0xb000;i<0xc000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom+((i-0xb000)>>PAGE_SHIFT));
    }
    // Map lower part of the Oss cart, this comes from the upper part of
    // the ROM.
    for(i=0xa000;i<0xb000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom+(((i-0xa000)>>PAGE_SHIFT)) + 16);
    }
    return true;
  }
  return false;
}
///

/// CartOSS8K::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartOSS8K::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  bool newdisabled = false;
  
  //
  // This is not exactly the superbank logic you find documented,
  // but at least the logic that makes the Mac65 working! Mac65
  // requires addresses 0xd500,0xd501 and 0xd509
  //
  switch(mem & 0x0f) {
  case 0:
  case 2:
  case 6:
  case 1:
  case 3:
  case 7:
  case 4:
  case 9:
    newdisabled  = false;
    break;
  case 8:
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:
    newdisabled  = true;
    break;    
  default:
    return false;
  }
  // This will/might require rebuilding now
  if (newdisabled != Disabled) {
    Disabled   = newdisabled;
    mmu->BuildCartArea();
  }
  //
  // We say that only writes into the area 0xd500 to 0xd50f really
  // belong to this cart type.
  return ((mem & 0xf0) == 0);
}
///

/// CartOSS8K::DisplayStatus
// Display the status over the monitor.
void CartOSS8K::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Cart disabled      : %s\n",
		   CartType(),
		   Disabled?("yes"):("no"));
}
///

/// CartOSS8K::State
// Perform the snapshot operation for the CartCtrl unit.
void CartOSS8K::State(class SnapShot *sn)
{
  sn->DefineBool("CartDisabled","OSS 8K cartridge disable flag",Disabled);
}
///
