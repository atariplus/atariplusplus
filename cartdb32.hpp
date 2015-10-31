/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartdb32.hpp,v 1.5 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of the 5200 32K Debug supercart
 **********************************************************************************/

#ifndef CARTDB32_HPP
#define CARTDB32_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class CartDB32
// The CartDB32 class implements the 32K
// bankswitching debug cartridge for the 5200
// games system.
// This maps its pages into the 0x8000-0xbfff
// area with bankswitching in 0x8000 to 0x9fff.
class CartDB32 : public Cartridge {
  //
  // The contents of the cart. 32K are 128 pages.
  class RomPage Rom[128];
  //
  // Flag of the currently active bank.
  UBYTE         ActiveBank;
  //
public:
  // The cartridge requires the number of banks for construction,
  // unlike other carts.
  CartDB32(void);
  virtual ~CartDB32(void);
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
  // Test whether this cart is "available" in the sense that the CartCtl
  // line TRIG3 is pulled. 
  virtual bool IsMapped(void)
  {
    return true;
  }
  //
  // Initialize this memory controller, built its contents.
  virtual void Initialize(void);
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
