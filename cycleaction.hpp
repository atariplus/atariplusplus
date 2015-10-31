/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cycleaction.hpp,v 1.2 2015/05/21 18:52:38 thor Exp $
 **
 ** In this module: Interface for frequent operations that have to happen
 **                 each cycle.
 **********************************************************************************/

#ifndef CYCLEACTION_HPP
#define CYCLEACTION_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
///

/// Class CycleAction
// This class defines an interface for a callback function that is
// called for each CPU cycle.
class CycleAction : public Node<class CycleAction> {
public:
  //
  CycleAction(class Machine *mach);
  virtual ~CycleAction(void);
  //
  // This is the callback function that requires overloading to be
  // useful. 
  virtual void Step(void) = 0;
};
///

///
#endif

    
  
