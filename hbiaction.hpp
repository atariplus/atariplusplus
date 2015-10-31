/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: hbiaction.hpp,v 1.3 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Interface for frequent operations that have to happen
 **                 each horizontal blank periodically.
 **********************************************************************************/

#ifndef HBIACTION_HPP
#define HBIACTION_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
///

/// Class HBIAction
// This class defines an interface for a callback function that is
// called each time the emulator runs into a "horizontal blank", to
// be specific, after the horizontal blank finishes.
class HBIAction : public Node<class HBIAction> {
public:
  //
  HBIAction(class Machine *mach);
  virtual ~HBIAction(void);
  //
  // This is the callback function that requires overloading to be
  // useful. 
  virtual void HBI(void) = 0;
};
///

///
#endif

    
  
