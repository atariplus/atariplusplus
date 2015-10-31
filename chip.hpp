/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: chip.hpp,v 1.10 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Definition of a generic chip
 **********************************************************************************/

#ifndef CHIP_HPP
#define CHIP_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "page.hpp"
#include "configurable.hpp"
#include "machine.hpp"
///

/// Forward definitions
class Monitor;
///

/// Class Chip
// This class defines a chip which is both configurable
// and reset-able. It need not to occupy a memory page
class Chip : public Configurable, public Node<class Chip> {
protected:
  //
  class Machine *machine;  // connection to the machine
  //
private:
  const char    *name;
  //
public:
  // Constructor
  Chip(class Machine *mach,const char *n);
  virtual ~Chip(void);
  //
  // Warmstart and ColdStart.
  virtual void WarmStart(void) = 0;
  virtual void ColdStart(void) = 0;
  //
  // Print the status of the chip over the monitor
  virtual void DisplayStatus(class Monitor *) = 0;
  //
  // Return the name of the chip
  const char *NameOf(void) const
  {
    return name;
  }
  //
  // Of course the chip as nexts and prevs, but as it is part of
  // two chains here, we must enforce which chain we mean. The
  // only chain here that makes sense is the chip chain of course.
  class Chip *NextOf(void) const
  {
    return Node<Chip>::NextOf();
  }
  class Chip *PrevOf(void) const
  {
    return Node<Chip>::PrevOf();
  }
};
///

///
#endif
