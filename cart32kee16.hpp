/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart32kee16.hpp,v 1.3 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of a 5200 16K cart with
 ** incomplete mapping
 **********************************************************************************/

#ifndef CART32KEE16_HPP
#define CART32KEE16_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class Cart32KEE16
// The Cart32K class implements a 16K cart with incomplete mapping.
class Cart32KEE16 : public Cartridge {
  //
  // The contents of the cart. 16K = 64 pages
  class RomPage Rom[64];
  //
public:
  Cart32KEE16(void);
  virtual ~Cart32KEE16(void);
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
