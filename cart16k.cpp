/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart16k.cpp,v 1.10 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of a plain 16K cart
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "cart16k.hpp"
///

/// Cart16K::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD Cart16K::CartSizes[] = {16,0};
///

/// Cart16K::Cart16K
// Construct the cart. There is nothing to do here.
Cart16K::Cart16K(void)
{
}
///

/// Cart16K::~Cart16K
// Dispose the cart. There is nothing to dispose
Cart16K::~Cart16K(void)
{
}
///

/// Cart16K::CartType
// Return a string identifying the type of the cartridge.
const char *Cart16K::CartType(void)
{
  return "16K";
}
///

/// Cart16K::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void Cart16K::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 64;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("Cart16K::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// Cart16K::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool Cart16K::MapCart(class MMU *mmu)
{
  ADR i;

  for(i=0x8000;i<0xc000;i+=Page::Page_Length) {
    mmu->MapPage(i,Rom + ((i-0x8000)>>Page::Page_Shift));
  }

  return true;
}
///
