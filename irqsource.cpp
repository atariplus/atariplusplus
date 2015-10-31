/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: irqsource.cpp,v 1.7 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: IRQ forwarding/administration
 **********************************************************************************/

/// Includes
#include "types.hpp"
#include "machine.hpp"
#include "cpu.hpp"
#include "exceptions.hpp"
#include "irqsource.hpp"
///

/// IRQSource::IRQSource
IRQSource::IRQSource(class Machine *mach)
  : Machine(mach)
{
  class IRQSource *prev;
  //
  // Allocate a new IRQ mask bit for us by upshifting the mask of the
  // previous source.
  mach->IRQChain().AddTail(this);
  if ((prev = PrevOf())) {
    IRQMask = prev->IRQMask<<1;
  } else {
    IRQMask = 1;
  }
  if (IRQMask == 0)
    Throw(OutOfRange,"IRQSource::IRQSource","no free IRQ slots for IRQ sources");
}
///

/// IRQSource::~IRQSource
IRQSource::~IRQSource(void)
{
  Remove();
}
///

/// IRQSource::PullIRQ
// Let the IRQ line for this source go down, signal the result to the CPU
// class to generate the IRQ.
void IRQSource::PullIRQ(void)
{
  Machine->CPU()->GenerateIRQ(IRQMask);
}
///

/// IRQSource::DropIRQ
// Release the IRQ line for this source. This could, but need not release
// the IRQ completely as there might be several sources for the IRQ line
void IRQSource::DropIRQ(void)
{
  Machine->CPU()->ReleaseIRQ(IRQMask);
}
///
