/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart8k.hpp,v 1.6 2015/09/13 14:30:31 thor Exp $
 **
 ** In this module: The implementation of a plain 8K cart
 **********************************************************************************/

#ifndef CART8K_HPP
#define CART8K_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class Cart8K
// The Cart8K class implements a simple 8K cart without CartCtrl support
// whatsoever.
class Cart8K : public Cartridge {
  //
protected:
  // The basic requires access here.
  // The contents of the cart
  class RomPage Rom[32];
  //
public:
  Cart8K(void);
  virtual ~Cart8K(void);
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
