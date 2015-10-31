/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: irqsource.hpp,v 1.5 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: IRQ forwarding/administration
 **********************************************************************************/

#ifndef IRQSOURCE_HPP
#define IRQSOURCE_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
///

/// Class IRQSource
// This class administrates various sources for the
// interrupt request (maskable) interrupt of the 6502
class IRQSource : public Node<class IRQSource> {
  //
  // Pointer to the machine we get the CPU from if needed
  class Machine     *Machine;
  //
  // The IRQ mask. This defines the bit we set in the
  // CPU IRQ mask. Each source is uniquely identified by this
  // bit mask. It is purely for internal use only as this bit
  // is never visible from within the Atari virtual machine.
  ULONG              IRQMask;
  //
public:
  IRQSource(class Machine *mach);
  ~IRQSource(void);
  //
  // Pull the IRQ line low to signal the arrival of an IRQ
  void PullIRQ(void);
  //
  // Drop the IRQ line, i.e. let it go high, at least as far
  // as this source is concerned.
  void DropIRQ(void);
  //
};
///

///
#endif
