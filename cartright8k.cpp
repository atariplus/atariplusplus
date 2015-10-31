/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartright8k.cpp,v 1.3 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of a plain 8K cart for
 ** the right slot of the 800. This is almost a standard 8K cart
 ** except that it maps into the 0x8000 and up area.
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "cartright8k.hpp"
///

/// CartRight8K::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD CartRight8K::CartSizes[] = {8,0};
///

/// CartRight8K::CartRight8K
// Construct the cart. There is nothing to do here.
CartRight8K::CartRight8K(void)
{
}
///

/// CartRight8K::~CartRight8K
// Dispose the cart. There is nothing to dispose
CartRight8K::~CartRight8K(void)
{
}
///

/// CartRight8K::CartType
// Return a string identifying the type of the cartridge.
const char *CartRight8K::CartType(void)
{
  return "Right8K";
}
///

/// CartRight8K::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void CartRight8K::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 32;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("CartRight8K::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// CartRight8K::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool CartRight8K::MapCart(class MMU *mmu)
{
  ADR i;

  for(i=0x8000;i<0xa000;i+=PAGE_LENGTH) {
    mmu->MapPage(i,Rom + ((i-0x8000)>>PAGE_SHIFT));
  }

  return true;
}
///
