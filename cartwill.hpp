/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartwill.hpp,v 1.5 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of the WILL supercarts
 **********************************************************************************/

#ifndef CARTWILL_HPP
#define CARTWILL_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class CartWill
// The CartWill class implements the Will supercartridge.
// They map into the 0xa000 to 0xbfff area with four or eight
// banks of 8K each.
class CartWill : public Cartridge {
  //
  // The contents of the cart. These carts come in various
  // ROM sizes.
  class RomPage *Rom;
  //
  // Number of banks we hold. This should be a power of
  // two.
  UBYTE         TotalBanks;
  //
  // Flag of the currently active bank.
  UBYTE         ActiveBank;
  //
  // Flag whether this cart has been disabled.
  bool          Disabled;
  //
  //
public:
  CartWill(int banks);
  virtual ~CartWill(void);
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
  // Initialize this memory controller, built its contents.
  virtual void Initialize(void);
  //  
  // Perform a write into the CartCtrl area, possibly modifying the mapping.
  // This never expects a WSYNC. Default is not to perform any operation.
  virtual bool ComplexWrite(class MMU *mmu,ADR mem,UBYTE val);
  //
  // Test whether this cart is "available" in the sense that the CartCtl
  // line TRIG3 is pulled. Default result is "yes".
  virtual bool IsMapped(void)
  {
    return !Disabled;
  }  
  //
  // Display the status over the monitor.
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Perform the snapshot operation for the CartCtrl unit.
  virtual void State(class SnapShot *snap);
};
///

///
#endif
