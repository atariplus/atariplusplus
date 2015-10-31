/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartossb.cpp,v 1.10 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of an Oss Supercart
 ** with alternative mapping
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
#include "cartossb.hpp"
///

/// CartOSSB::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartOSSB::CartSizes[] = {16,0};
///

/// CartOSSB::CartOSSB
// Construct the cart. There is nothing to do here.
CartOSSB::CartOSSB(void)
{
  ActiveBank = 0;
  Disabled   = false;
  Blank.Blank();
}
///

/// CartOSSB::~CartOSSB
// Dispose the cart. There is nothing to dispose
CartOSSB::~CartOSSB(void)
{
}
///

/// CartOSSB::Initialize
// Initialize this memory controller, built its contents.
void CartOSSB::Initialize(void)
{
  ActiveBank = 0;
  Disabled   = false;
}
///

/// CartOSSB::CartType
// Return a string identifying the type of the cartridge.
const char *CartOSSB::CartType(void)
{
  return "OssB";
}
///

/// CartOSSB::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartOSSB::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
        
  pages    = 48;
  page     = Rom + 16;
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartOSSB::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
  
  pages    = 16;
  page     = Rom;
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartOSSB::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartOSSB::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartOSSB::MapCart(class MMU *mmu)
{
  ADR i;

  if (Disabled == false) {    
    // Map upper part of Oss cart now. This is always ROM A hi
    // upper.
    for(i=0xb000;i<0xc000;i+=PAGE_LENGTH) {
      mmu->MapPage(i,Rom+((i-0xa000)>>PAGE_SHIFT));
    }
    switch(ActiveBank) {
    case 0:
      // ROM A Lo here (0xd503)
      for(i=0xa000;i<0xb000;i+=PAGE_LENGTH) {
	mmu->MapPage(i,Rom+(((i-0xa000)>>PAGE_SHIFT)));
      }
      break;
    case 0xff: 
      // map blank area here (0xd502)
      for(i=0xa000;i<0xb000;i+=PAGE_LENGTH) {
	mmu->MapPage(i,&Blank);
      }
      break;
    case 2: 
      // ROM B Lo here (0xd500)
      for(i=0xa000;i<0xb000;i+=PAGE_LENGTH) {
	mmu->MapPage(i,Rom+((i-0xa000)>>PAGE_SHIFT) + 32);
      }
      break;
    case 3: 
      // ROM B Hi here (0xd504)
      for(i=0xa000;i<0xb000;i+=PAGE_LENGTH) {
	mmu->MapPage(i,Rom+(((i-0xa000)>>PAGE_SHIFT) + 32 + 16));
      }
      break;
    }
    return true;
  }
  return false;
}
///

/// CartOSSB::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartOSSB::ComplexWrite(class MMU *mmu,ADR mem,UBYTE)
{  
  int  newbank     = 0;
  bool newdisabled = false;
  
  // This is not exactly the superbank logic you find documented,
  // but at least the logic that makes the Mac65 working! Mac65
  // requires addresses 0xd500,0xd501 and 0xd509
  //
  switch(mem & 0x0f) {
  case 0:
    newbank      = 2;
    newdisabled  = false;
    break;
  case 2:
  case 6:
    newbank      = 0xff;
    newdisabled  = false;
    break;
  case 1:
  case 3:
  case 7:
    newbank      = 0;
    newdisabled  = false;
    break;
  case 4:
  case 9:
    newbank      = 3;
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
  if (newbank != ActiveBank || newdisabled != Disabled) {
    Disabled   = newdisabled;
    ActiveBank = newbank;
    mmu->BuildCartArea();
  }
  //
  // We say that only writes into the area 0xd500 to 0xd50f really
  // beLONG to this cart type.
  return ((mem & 0xf0) == 0);
}
///

/// CartOSSB::DisplayStatus
// Display the status over the monitor.
void CartOSSB::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Cart disabled      : %s\n"
		   "Active bank        : %d\n",
		   CartType(),
		   Disabled?("yes"):("no"),
		   ActiveBank);
}
///

/// CartOSSB::State
// Perform the snapshot operation for the CartCtrl unit.
void CartOSSB::State(class SnapShot *sn)
{
  sn->DefineLong("SuperBank","OSS cartridge active bank selection",0,3,ActiveBank);
  sn->DefineBool("CartDisabled","OSS cartridge disable flag",Disabled);
}
///
