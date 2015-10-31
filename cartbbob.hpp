/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartbbob.hpp,v 1.7 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: The implementation of the wierd bounty bob cart
 **********************************************************************************/

#ifndef CARTBBOB_HPP
#define CARTBBOB_HPP

/// Includes
#include "page.hpp"
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Forwards
class MMU;
///

/// Class CartBBOB
// The CartBBOB class implements the bounty bob
// cartridge and its wierd bank switching mechanism.
// This cart is mapped from 0x8000 to 0xbfff, with
// bank-switching registers at 0x8ff6..0x8ff9 and
// 0x9ff6..0x9ff9 having a total size of 40K.
class CartBBOB : public Cartridge {
  //
  // The contents of the cart. These are 40K
  // or alternatively, 160 pages.
  class RomPage Rom[160];
  //
  // A special bank switching page that is mapped into the
  // page at 0x8ff0 and 0x9ff0.
  class BankPage : public Page {
  private:
    // Pointer to the MMU which is required to perform the mapping
    class MMU *MMU;
    //
    // Page offset for the "read bank" register
    UBYTE PageOffset;
    //
    // Switch the bounty bob cartridge into a new page.
    void SwitchBank(int newbank);
    //
    // The following must be accessed from the bounty bob
    // cartridge class:
  public:
    // The bank that is currently mapped into this
    // region. 
    UBYTE          ActiveBank;
    //
    // The pointer to the bank this class hides. This is part
    // of the ROM pages above and hence, a ROM.
    class RomPage *Hidden;
    //
    // We also need the special accessor functions
    // for reading and writing this "ROM"
    virtual UBYTE ComplexRead(ADR mem);
    virtual void  ComplexWrite(ADR,UBYTE);
    //
    // Constructor and destructor
    BankPage(class MMU *mmu,UBYTE pageoffset);
    ~BankPage(void);
  }              Bank40,Bank50;
  //
public:
  // The cartridge requires the MMU for construction,
  // unlike other carts.
  CartBBOB(class MMU *mmu);
  virtual ~CartBBOB(void);
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
  // There are no CartCtrl here. BountyBob does its mapping by private
  // banking registers within the cart area. Yuck!
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
