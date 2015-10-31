/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: vbiaction.hpp,v 1.7 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Interface for frequent operations that have to happen
 **                 each VBI periodically.
 **********************************************************************************/

#ifndef VBIACTION_HPP
#define VBIACTION_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
///

/// Forwards
class Timer;
class Machine;
///

/// Class VBIAction
// This class defines an interface for a callback function that is
// called each time the emulator runs into a "vertical blank". This
// is useful to trigger frequent activity 
// (as polling the keyboard or the mouse)
class VBIAction : public Node<class VBIAction> {
public:
  VBIAction(class Machine *mach);
  virtual ~VBIAction(void);
  //  
  // This is the callback function that requires overloading to be
  // useful. The "quick" flag is set if we're better quick because
  // the VBI is "too late" anyhow and should not defer activity
  // further.
  // If pause is set, then the emulator is currently pausing.
  // The "timer" keeps a time stamp updated each VBI and running
  // out as soon as the VBI is over.
  virtual void VBI(class Timer *time,bool quick,bool pause) = 0;
  //
  //
};
///

///
#endif

    
  
