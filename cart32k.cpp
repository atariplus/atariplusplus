/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart32k.cpp,v 1.14 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of a plain 32K cart
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "cart32k.hpp"
#include "monitor.hpp"
///

/// Cart32K::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD Cart32K::CartSizes[] = {4,8,16,32,0};
///

/// Cart32K::Cart32K
// Construct the cart. 
// We get the number of 4K banks here. Each bank contains 16
// pages.
Cart32K::Cart32K(UBYTE pages)
  : Rom(new class RomPage[pages << 4]), Size(pages << 2)
{
}
///

/// Cart32K::~Cart32K
// Dispose the cart. There is nothing to dispose
Cart32K::~Cart32K(void)
{
  delete[] Rom;
}
///

/// Cart32K::CartType
// Return a string identifying the type of the cartridge.
const char *Cart32K::CartType(void)
{
  return "32K";
}
///

/// Cart32K::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void Cart32K::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = Size << 2; // 1K contains four pages, and Size is the size in 4K
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("Cart32K::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// Cart32K::DisplayStatus
// Display the status over the monitor.
void Cart32K::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Size of the cart   : " ATARIPP_LD "K\n",
		   CartType(),
		   Size);
}
///

/// Cart32K::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool Cart32K::MapCart(class MMU *mmu)
{
  ADR i;
  ULONG adrmask;

  // Compute the mask for the incomplete mapping.
  adrmask = (Size << 10) - 1;
  //
  for(i=0x4000;i<0xc000;i+=Page::Page_Length) {
    mmu->MapPage(i,Rom + (((i-0x4000) & adrmask)>>Page::Page_Shift));
  }

  return true;
}
///
