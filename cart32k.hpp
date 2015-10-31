/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cart32k.hpp,v 1.8 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of a plain 32K cart
 ** for the 5200. This has possibly incomplete mapping
 **********************************************************************************/

#ifndef CART32K_HPP
#define CART32K_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class Cart32K
// The Cart32K class implements a simple 32K cart without CartCtrl support
// whatsoever for the 5200 only, though.
class Cart32K : public Cartridge {
  //
  // The contents of the cart. Since the sizes
  // may vary, we must allocate this dynamically.
  class RomPage *Rom;
  //
  // Size of the cart in Kbyte.
  LONG           Size;
  //
public:
  // The constructor takes the number of 4K banks we actually have.
  Cart32K(UBYTE banks);
  virtual ~Cart32K(void);
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
  // Display the status of this cart
  virtual void DisplayStatus(class Monitor *mon);
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
