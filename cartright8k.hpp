/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartright8k.hpp,v 1.3 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of the 800 "right cartridge" 8K cart
 ** that maps into 0x8000...0x9fff instead of 0xa000 and up.
 **********************************************************************************/

#ifndef CARTRIGHT8K_HPP
#define CARTRIGHT8K_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class CartRight8K
// The CartRight8K class implements a simple 8K cart without CartCtrl support
// whatsoever, but it maps into a different area, namely 0x8000 and up.
class CartRight8K : public Cartridge {
  //
  // The contents of the cart
  class RomPage Rom[32];
  //
public:
  CartRight8K(void);
  virtual ~CartRight8K(void);
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
