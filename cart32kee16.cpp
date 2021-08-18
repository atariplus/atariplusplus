/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart32kee16.cpp,v 1.4 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of a 5200 16K cart with
 ** incomplete mapping
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "cart32kee16.hpp"
///

/// Cart32KEE16::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD Cart32KEE16::CartSizes[] = {16,0};
///

/// Cart32KEE16::Cart32KEE16
// Construct the cart. There is nothing to do here.
Cart32KEE16::Cart32KEE16(void)
{
}
///

/// Cart32KEE16::~Cart32KEE16
// Dispose the cart. There is nothing to dispose
Cart32KEE16::~Cart32KEE16(void)
{
}
///

/// Cart32KEE16::CartType
// Return a string identifying the type of the cartridge.
const char *Cart32KEE16::CartType(void)
{
  return "32KEE16";
}
///

/// Cart32KEE16::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void Cart32KEE16::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 64;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("Cart32KEE16::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// Cart32KEE16::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool Cart32KEE16::MapCart(class MMU *mmu)
{
  ADR i;

  for(i=0x4000;i<0x6000;i+=Page::Page_Length) {
    mmu->MapPage(i,Rom + ((i-0x4000)>>Page::Page_Shift));
  }
  for(i=0x6000;i<0xa000;i+=Page::Page_Length) {
    mmu->MapPage(i,Rom + ((i-0x6000)>>Page::Page_Shift));
  }
  for(i=0xa000;i<0xc000;i+=Page::Page_Length) {
    mmu->MapPage(i,Rom + ((i-0xa000 + 0x2000)>>Page::Page_Shift));
  }

  return true;
}
///
