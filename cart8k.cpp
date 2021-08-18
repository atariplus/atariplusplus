/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart8k.cpp,v 1.8 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: The implementation of a plain 8K cart
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "cart8k.hpp"
///

/// Cart8K::CartSizes
// This static array contains the possible cart sizes
// for this cart type.
const UWORD Cart8K::CartSizes[] = {8,0};
///

/// Cart8K::Cart8K
// Construct the cart. There is nothing to do here.
Cart8K::Cart8K(void)
{
}
///

/// Cart8K::~Cart8K
// Dispose the cart. There is nothing to dispose
Cart8K::~Cart8K(void)
{
}
///

/// Cart8K::CartType
// Return a string identifying the type of the cartridge.
const char *Cart8K::CartType(void)
{
  return "8K";
}
///

/// Cart8K::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
void Cart8K::ReadFromFile(FILE *fp)
{ 
  class RomPage *page;
  int pages;
  
  pages    = 32;
  page     = Rom;
  
  do {
    if (!page->ReadFromFile(fp))    
      ThrowIo("Cart8K::ReadFromFile","failed to read the ROM image from file");
    page++;
  } while(--pages);
}
///

/// Cart8K::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
bool Cart8K::MapCart(class MMU *mmu)
{
  ADR i;

  for(i=0xa000;i<0xc000;i+=Page::Page_Length) {
    mmu->MapPage(i,Rom + ((i-0xa000)>>Page::Page_Shift));
  }

  return true;
}
///
