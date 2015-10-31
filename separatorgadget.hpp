/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: separatorgadget.hpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: Definition of a separator bar gadget
 **********************************************************************************/

#ifndef SEPARATORGADGET_HPP
#define SEPARATORGADGET_HPP

/// Includes
#include "gadget.hpp"
///

/// SeparatorGadget
// The SeparatorGadget draws a horizontal separation bar. It does not
// react on user input either.
class SeparatorGadget : public Gadget {
public:
  SeparatorGadget(List<Gadget> &gadgetlist,
		  class RenderPort *rp,LONG le,LONG te,LONG w,LONG h);
  virtual ~SeparatorGadget(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Re-render the gadget
  virtual void Refresh(void);
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  virtual const class Gadget *FindGadgetInDirection(LONG &,LONG &,WORD,WORD) const
  {
    return NULL;
  }
};
///

///
#endif
