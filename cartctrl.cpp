/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartctrl.cpp,v 1.24 2020/07/18 15:20:39 thor Exp $
 **
 ** In this module: Cart Control logic for Oss Super Carts
 **********************************************************************************/

/// Includes
#include "cartctrl.hpp"
#include "mmu.hpp"
#include "monitor.hpp"
#include "cartrom.hpp"
#include "cpu.hpp"
#include "cartridge.hpp"
#include "snapshot.hpp"
///

/// CartCtrl::CartCtrl
// Constructor of this class
CartCtrl::CartCtrl(class Machine *mach)
  : Chip(mach,"CartCtrl"), Saveable(mach,"Cartctrl")
{ }
///

/// CartCtrl::~CartCtrl
CartCtrl::~CartCtrl(void)
{
}
///

/// CartCtrl::ComplexRead
UBYTE CartCtrl::ComplexRead(ADR mem)
{
  class Cartridge *c = cart;
  UBYTE value = 0xff;
  // Ask the cart whether it implements any
  // reading. If not, the cart virtual method
  // falls back to writing a 0xff.
  while(c) {
    // Perform the write into cartctrl until we find a cart
    // that feels responsible for it.
    if (c->ComplexRead(mmu,mem,value))
      return value;
    c = c->NextOf();
  }
  return value;
}
///

/// CartCtrl::ComplexWrite
// Write into CartCtrl. The value does not matter, only the address does.
void CartCtrl::ComplexWrite(ADR mem,UBYTE val)
{
  class Cartridge *c = cart;
  //
  // Forward this request to the installed cartridge.
  while(c) {
    // Perform the write into the cartctrl until we find a
    // cart that feels responsible for it.
    if (c->ComplexWrite(mmu,mem,val))
      return;
    c = c->NextOf();
  }
}
///

/// CartCtrl::ColdStart
// Issue a coldstart for an Oss super cart
void CartCtrl::ColdStart(void)
{
  // Get the link to the MMU.
  mmu  = machine->MMU();
  cart = machine->CartROM()->Cart();
  WarmStart();
}
///  

/// CartCtrl::WarmStart
// issue a warm start for an Oss super cart
void CartCtrl::WarmStart(void)
{
  mmu->BuildCartArea();
}
///

/// CartCtrl::ParseArgs
// Parse off CartCtrl specific arguments. There are none.
void CartCtrl::ParseArgs(class ArgParser *)
{
  // No user options here.
}
///

/// CartCtrl::DisplayStatus
// Display the internal status of the cartctrl logic.
void CartCtrl::DisplayStatus(class Monitor *mon)
{ 
  class Cartridge *c = cart;

  mon->PrintStatus("CartCtrl status:\n");
  while (c) {
    c->DisplayStatus(mon);
    mon->PrintStatus("\n");
    c = c->NextOf();
  }
}
///

/// CartCtrl::State
// Read or set the internal status
void CartCtrl::State(class SnapShot *sn)
{
  class Cartridge *c = cart;
  int i = 0;
  char buffer[16];

  while(c && i < 8) {
    snprintf(buffer,15,"CartCtrl.%d",i);
    sn->DefineTitle(buffer);
    c->State(sn);
    c = c->NextOf();
    i++;
  }
  //
  // Now re-install these settings.
  mmu->BuildCartArea();
}
///
