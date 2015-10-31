/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartrt8.hpp,v 1.6 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Implementation of the RTime8 real-time cartridge as "pass thru" 
 **
 ** Credits go to Jason Duerstock's for its analyzation of the RT8 cartridge. 
 ** Unfortunately, something seems to be still not quite right with it. The 
 ** current implementation here uses a somewhat different route how the register
 ** assignment *might* work. This is all an educated guess, though.
 **********************************************************************************/

#ifndef CARTRT8_HPP
#define CARTRT8_HPP

/// Includes
#include "rompage.hpp"
#include "cartridge.hpp"
///

/// Class CartRT8
// The CartRT8 class implements the real time pass-thru cart.
class CartRT8 : public Cartridge {
  //
  // State definitions for the real-time register access
  enum RTState {
    Idle,             // the clock is currently unlocked
    LowNibble,        // read from the low-nibble of a register byte
    HighNibble        // read from the high-nibble of a register byte
  }            RegisterState;
  // The index of the register we are going to read.
  UBYTE        RegisterIndex;
  //
  // The array of internal (packed) registers of the clock.
  UBYTE        Registers[16];
  //
  // Update the registers with the contents of the unixoid system time
  void UpdateClock(void);
  //
public:
  CartRT8(void);
  virtual ~CartRT8(void);
  //
  // Return a string identifying the type of the cartridge.
  virtual const char *CartType(void);
  //
  // Build up an argument parser to get or verify the type: This may return
  // a different type than suggested, or no type at all.
  // CartToLoad is the guessed type, type the suggested/picked type.
  // withheader is true if we are sure about the suggested type.
  virtual void ParseArgs(class ArgParser *,LONG &carttoload,bool withheader);
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
  //  
  // Perform a write into the CartCtrl area, possibly modifying the mapping.
  // This never expects a WSYNC.
  virtual bool ComplexWrite(class MMU *mmu,ADR mem,UBYTE val);
  // We also need to read from the real-time clock.
  virtual bool ComplexRead(class MMU *mmu,ADR mem,UBYTE &val);
  //
  // Test whether this cart is "available" in the sense that the CartCtl
  // line TRIG3 is pulled. Default result is "yes", here "no" since this
  // cart doesn't map anything at all.
  virtual bool IsMapped(void)
  {
    return false;
  }
  //
  // Display the status over the monitor.
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif
