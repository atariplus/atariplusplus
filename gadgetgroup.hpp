/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gadgetgroup.hpp,v 1.4 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of a meta gadget containing other gadgets
 **********************************************************************************/

#ifndef GADGETGROUP_HPP
#define GADGETGROUP_HPP

/// Includes
#include "gadget.hpp"
///

/// Class GadgetGroup
// A "meta gadget" that groups various gadgets together. We derive
// this from the gadget list such that we can straightforwardly add
// gadgets to it.
class GadgetGroup : public Gadget, public List<class Gadget> {
protected:
  //
  // The currently active (sub-)gadget of this gadget
  class Gadget *ActiveGadget;
  //
public:
  GadgetGroup(List<Gadget> &gadgetlist,
	      class RenderPort *rp,LONG le,LONG te,LONG w,LONG h);
  virtual ~GadgetGroup(void);
  //
  // Perform action if the gadget was hit, resp. release the gadget.
  virtual bool HitTest(struct Event &ev);
  //
  // Refresh this gadget and all gadgets inside.
  virtual void Refresh(void);
  //
  // Move this gadget (and its contents) by a given amount
  virtual void MoveGadget(LONG dx,LONG dy);  
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  virtual const class Gadget *FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const;
};
///

///
#endif
