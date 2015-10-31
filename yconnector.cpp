/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: yconnector.cpp,v 1.3 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: Definition of a Y (or T) connector that links two
 ** or more chips/pages into one page.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "yconnector.hpp"
#include "new.hpp"
#include "exceptions.hpp"
#include "string.hpp"
///

/// YConnector::YConnector
// Construct the Y connector from a mask.
// Pages must be added later. The mask
// sets those bits to one which select between
// the sub-pages.
YConnector::YConnector(ADR addressmask)
  : Discriminator(UBYTE(addressmask & 0xff)), SubPages(NULL)
{
  int bits; // counts number of selection bits.
  //
#if CHECK_LEVEL > 2
  // Check whether the addressmask is valid. It is not if any
  // of the higher bits is set.
  if ((addressmask >> 8) || (addressmask == 0)) {
    Throw(InvalidParameter,"YConnector::YConnector",
	  "the page bit contains bits outside the page bits");
  }
#endif
  // Compute the downshift. For that, we need to shift-down the
  // address mask until we get the first one-bit in position 0.
  DownShift = 0;
  while ((addressmask & 0x01) == 0) {
    DownShift++;
    addressmask >>= 1;
  }
  //
  // Now count the overall number of bits.
  bits = 0;
  while(addressmask) {
    bits++;
    addressmask >>= 1;
  }
  //
  // allocate the pages now.
  SubPages = new class Page *[1<<bits];
  //
  // Reset all pointers here to NULL.
  memset(SubPages,0,(sizeof(class Page *))<<bits);
}
///

/// YConnector::~YConnector
// Cleanup, basically just dispose the subselection 
YConnector::~YConnector(void)
{
  delete[] SubPages;
}
///

/// YConnector::ComplexRead
// Read from one of the pages defined in the sub-pages
UBYTE YConnector::ComplexRead(ADR mem)
{
  class Page *page;
  //
  // Extract the page from the bits.
  page = SubPages[(mem & Discriminator) >> DownShift];
  //
  // If this page is addressed, forward the result to
  // the corresponding sub-page.
  if (page) {
    return page->ReadByte(mem);
  } else {
    // Otherwise, emulate "tri-state" output, namely
    // 0xff.
    return 0xff;
  }
}
///

/// YConnector::ComplexWrite
// Write to eone of the sub-pages, selected
// by the discriminator-bits.
void YConnector::ComplexWrite(ADR mem,UBYTE data)
{
  class Page *page;
  //
  // Same as above, check for the validity of
  // the page, i.e. whether we address something.
  page = SubPages[(mem & Discriminator) >> DownShift];
  //
  // For occupied bits: Forward to the sub-pages.
  if (page) {
    page->WriteByte(mem,data);
  }
}
///

/// YConnector::ConnectPage
// Connect a sub-page at the given bit mask 
// to the sub-page list.
void YConnector::ConnectPage(class Page *page,ADR mem)
{
  SubPages[(mem & Discriminator) >> DownShift] = page;
}
///
