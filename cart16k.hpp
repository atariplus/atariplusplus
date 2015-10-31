/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart16k.hpp,v 1.5 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of a plain 16K cart
 **********************************************************************************/

#ifndef CART16K_HPP
#define CART16K_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class Cart16K
// The Cart16K class implements a simple 16K cart without CartCtrl support
// whatsoever.
class Cart16K : public Cartridge {
  //
  // The contents of the cart
  class RomPage Rom[64];
  //
public:
  Cart16K(void);
  virtual ~Cart16K(void);
  //
  // This static array contains the possible cart sizes
  // for this cart type.
  static const UWORD CartSizes[];
  //
  // Return a string identifying the type of the cartridge.
  virtual const char *CartType(void);
  //
  // Read the contents of this cart from an open file. Headers and other
  // mess has been skipped already here. Throws on failure.
  virtual void ReadFromFile(FILE *fp);
  //
  // Remap this cart into the address spaces by using the MMU class.
  // It must know its settings itself, but returns false if it is not
  // mapped. Then the MMU has to decide what to do about it.
  virtual bool MapCart(class MMU *mmu);
  //
};
///

///
#endif
