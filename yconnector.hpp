/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: yconnector.hpp,v 1.3 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: Definition of a Y (or T) connector that links two
 ** or more chips/pages into one page.
 **********************************************************************************/

#ifndef YCONNECTOR_HPP
#define YCONNECTOR_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "page.hpp"
///

/// Class YConnector
// This class connects two or more pages into one,
// dependent on the bit pattern of the lower bits.
// This class is required as an intermediate page
// if more than two entities have to be mapped into
// one page. This does not happen for "natural"
// Atari memory layouts, though.
class YConnector : public Page {
  //
  // The following defines the bit mask defining
  // which of the lower bits determine the selection
  // of the individual sub-pages of this page.
  UBYTE        Discriminator;
  //
  // The downshift (computed) to get an index from
  // the address/page offset
  int          DownShift;
  //
  //
  // Array of the pages we administrate. Lifetime
  // must be controlled outside.
  class Page **SubPages;
  //
  //
public:
  // Construct the Y connector from a mask.
  // Pages must be added later. The mask
  // sets those bits to one which select between
  // the sub-pages.
  YConnector(ADR addressmask);
  virtual ~YConnector(void);
  // 
  // Connect a sub-page at the given bit mask 
  // to the sub-page list.
  void ConnectPage(class Page *page,ADR mem);
  //
  // Methods imported from the page.
  virtual UBYTE ComplexRead(ADR mem);
  virtual void ComplexWrite(ADR mem,UBYTE value);
};
///

///
#endif

