/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartxegs.hpp,v 1.10 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: The implementation of an XEGS supercart
 **********************************************************************************/

#ifndef CARTXEGS_HPP
#define CARTXEGS_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class CartXEGS
// The CartXEGS class implements an XEGS supercartridge.
// These carts may have variable sizes and come with 4K
// banks of which the last one occupies the addresses
// 0xa000 to 0xbfff and all former ones can be mapped
// into the region of 0x8000 to 0x9fff
class CartXEGS : public Cartridge {
  //
  // The contents of the cart. As this can have a variable
  // size, we cannot allocate the pages statically here.
  class RomPage *Rom;
  //
  // a completely empty page
  class RomPage Blank;
  //
  // Number of banks in this cartridge
  UBYTE         TotalBanks;
  //
  // Flag of the currently active bank.
  UBYTE         ActiveBank;
  //
  // This is set in case the cart is disabled.
  bool          Disabled;
  //
  // Additional preferences: Whether this cart
  // can be disabled or not.
  bool          Switchable;
  //
public:
  // The cartridge requires the number of banks for construction,
  // unlike other carts.
  CartXEGS(UBYTE banks,bool switchable);
  virtual ~CartXEGS(void);
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
  // Test whether this cart is "available" in the sense that the CartCtl
  // line TRIG3 is pulled. 
  virtual bool IsMapped(void)
  {
    return (!Disabled);
  }
  //  
  // Perform a write into the CartCtrl area, possibly modifying the mapping.
  // This never expects a WSYNC. Default is not to perform any operation.
  // Return code indicates whether this write was useful for this cart.
  virtual bool ComplexWrite(class MMU *mmu,ADR mem,UBYTE val);
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
