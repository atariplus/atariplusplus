/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cartrt8.cpp,v 1.11 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Implementation of the RTime8 real-time cartridge as 
 ** "pass thru"
 **
 ** Credits go to Jason Duerstock's for its analyzation of the RT8 cartridge. 
 ** Unfortunately, something seems to be still not quite right with it. The 
 ** current implementation here uses a somewhat different route how the register
 ** assignment *might* work. This is all an educated guess, though.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "mmu.hpp"
#include "stdio.hpp"
#include "rompage.hpp"
#include "cartrom.hpp"
#include "cartridge.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "monitor.hpp"
#include "cartrt8.hpp"
#include "time.hpp"
///

/// CartRT8::CartRT8
// Construct the cart.
CartRT8::CartRT8(void)
{
  RegisterState = Idle;
  RegisterIndex = 0;
  memset(Registers,0,sizeof(Registers));
}
///

/// CartRT8::~CartRT8
// Dispose the cart. There is nothing to dispose
CartRT8::~CartRT8(void)
{
}
///

/// CartRT8::Initialize
// Initialize this memory controller, built its contents.
void CartRT8::Initialize(void)
{
  RegisterState = Idle;
  RegisterIndex = 0;
}
///

/// CartRT8::UpdateClock
// Update the registers with the contents of the unixoid system time
void CartRT8::UpdateClock(void)
{
  // We can only do this if we have the time function
#if HAVE_TIME && HAVE_LOCALTIME
  int m,w,y;
  time_t t;
  struct tm *lt;
  if (time(&t) != (time_t)(-1)) {
    lt = localtime(&t);
    m  = lt->tm_mon+1;          // month, counting from 1..12
    w  = ((lt->tm_wday+2)%7)+1; // day of the week, starting at friday... ????
    y  = lt->tm_year % 100;
    // Now perform the conversion. Register contents are in BCD. Yuck.
    Registers[0] = UBYTE((lt->tm_sec  % 10) | ((lt->tm_sec     / 10) << 4)); // seconds
    Registers[1] = UBYTE((lt->tm_min  % 10) | ((lt->tm_min     / 10) << 4)); // minutes
    Registers[2] = UBYTE((lt->tm_hour % 10) | ((lt->tm_hour    / 10) << 4)); // hours
    Registers[3] = UBYTE((lt->tm_mday % 10) | ((lt->tm_mday    / 10) << 4)); // day of the month
    Registers[4] = UBYTE((m % 10)           | ((m              / 10) << 4)); // month, counting from one
    Registers[5] = UBYTE((y           % 10) | ((y              / 10) << 4)); // oh bagger, this is not y2k proof (or at most, year 2055-proof...)
    Registers[6] = UBYTE(w);                                                 // day of the week.
  }
#endif
}
///

/// CartRT8::CartType
// Return a string identifying the type of the cartridge.
const char *CartRT8::CartType(void)
{
  return "R-Time 8";
}
///

/// CartRT8::ParseArgs
// Build up an argument parser to get or verify the type: This may return
// a different type than suggested, or no type at all. Since this cart type
// is unique, we can either use it or not.
void CartRT8::ParseArgs(class ArgParser *args,LONG &carttoload,bool)
{ 
  static const struct ArgParser::SelectionVector cartvector[] = 
    { {"None",Cartridge::Cart_None        },
      {"RT8" ,Cartridge::Cart_RTime8      },
      {NULL  ,0}
    };
  if (carttoload != Cartridge::Cart_None         && 
      carttoload != Cartridge::Cart_RTime8) {
    carttoload = Cartridge::Cart_RTime8;
    args->SignalBigChange(ArgParser::Reparse);
  }
  args->DefineSelection("CartType","cartridge type to use",cartvector,carttoload);
}
///

/// CartRT8::ReadFromFile
// Read the contents of this cart from an open file. Headers and other
// mess has been skipped already here. Throws on failure.
// There is nothing to load for an RT8 cartridge since it doesn't
// contain a ROM.
void CartRT8::ReadFromFile(FILE *)
{ 
}
///

/// CartRT8::MapCart
// Remap this cart into the address spaces by using the MMU class.
// It must know its settings itself, but returns false if it is not
// mapped. Then the MMU has to decide what to do about it.
// The RTime8 cart doesn't occupy any address space and nothing has
// to be mapped therefore.
bool CartRT8::MapCart(class MMU *)
{
  return false;
}
///

/// CartRT8::ComplexWrite
// Perform a write into the CartCtrl area, possibly modifying the mapping.
// This never expects a WSYNC. Default is not to perform any operation.
bool CartRT8::ComplexWrite(class MMU *,ADR mem,UBYTE val)
{  
  // Only two addresses are read here: 0xd5b8 and 0xd5b9
  if (mem == 0xd5b8 || mem == 0xd5b9) {
    switch (RegisterState) {
    case Idle:
      // This defines the active register and updates the clock.
      RegisterIndex = val;
      RegisterState = HighNibble;
      break;
    case LowNibble:
      // Hmm. Modifications into the RTime clock. What should we do about it? 
      // Nothing. I don't want to set the system clock from within the
      // emulator.
      // However, for identification, I must be able to write into the
      // registers 7..15.
      Registers[RegisterIndex] = UBYTE((Registers[RegisterIndex] & 0xf0) | (val & 0x0f));
      RegisterState            = Idle;
      break;
    case HighNibble:
      Registers[RegisterIndex] = UBYTE((Registers[RegisterIndex] & 0x0f) | ((val & 0x0f) << 4));
      RegisterState            = LowNibble;
      break;
    }
    return true;
  }
  return false;
}
///

/// CartRT8::ComplexRead
// We also need to read from the real-time clock.
bool CartRT8::ComplexRead(class MMU *,ADR mem,UBYTE &val)
{
  // Only two addresses are read here: 0xd5b8 and 0xd5b9
  if (mem == 0xd5b8 || mem == 0xd5b9) {
    switch (RegisterState) {
    case Idle:
      // Read the register index. Well, I hope that this is the meaning of all that.
      // We also decrement the register index here and then return the
      // register index. If we reach zero, then the clock is again free-running
      // and we can update it. This construction would allow the user of the clock
      // to read it while it is ticking by reading the registers backwards.
      val           = RegisterIndex;
      if (RegisterIndex == 0) {
	UpdateClock();
      } else {
	RegisterIndex--;
      }
      break;
    case LowNibble:
      // Read the low-nibble of the active register.
      val           = UBYTE(Registers[RegisterIndex] & 0x0f);
      RegisterState = Idle;
      break;
    case HighNibble:
      val           = UBYTE(Registers[RegisterIndex] >> 4);
      RegisterState = LowNibble;
      break;
    }
    return true;
  }
  return false;
}
///

/// CartRT8::DisplayStatus
// Display the status over the monitor.
void CartRT8::DisplayStatus(class Monitor *mon)
{  
  mon->PrintStatus("Cart type inserted : %s\n"
		   "Active register    : %d\n"
		   "Register state     : %s\n"
		   "Register contents  : %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x"
		   "\n",
		   CartType(),
		   RegisterIndex,
		   (RegisterState==Idle)?("idle"):((RegisterState==LowNibble)?("LowNibble"):("HighNibble")),
		   Registers[0],Registers[1],Registers[2],Registers[3],Registers[4],Registers[5],
		   Registers[6],Registers[7],Registers[8],Registers[9],Registers[10],Registers[11],
		   Registers[12],Registers[13],Registers[14],Registers[15]
		   );
}
///
