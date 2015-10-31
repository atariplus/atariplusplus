/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartctrl.hpp,v 1.10 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: Cart Control logic for Oss Super Carts
 **********************************************************************************/

#ifndef CARTCTRL_HPP
#define CARTCTRL_CPP

/// Includes
#include "chip.hpp"
#include "saveable.hpp"
///

/// Forwards
class MMU;
class ArgParser;
class Monitor;
class Cartridge;
///

/// Class CartCtrl
// This IO mapped device implements the Oss SuperCart bank
// switching logic, to be mapped at 0xd500..0xd600
class CartCtrl : public Chip, public Page, public Saveable {
  //
  // Link to the MMU
  class MMU       *mmu;
  // Link to the active cart
  class Cartridge *cart;
  //
  // Just the read and write implementation, nothing more.
  // Bank switching is done in the MMU.
  virtual UBYTE ComplexRead(ADR mem);
  virtual void  ComplexWrite(ADR mem,UBYTE byte);
  //
public:
  CartCtrl(class Machine *mach);
  ~CartCtrl(void);
  //
  // Coldstart and warmstart
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Read or save the state.
  virtual void State(class SnapShot *);
  //
  virtual void ParseArgs(class ArgParser *args);
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif
