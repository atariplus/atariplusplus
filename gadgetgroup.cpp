/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gadgetgroup.cpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of a meta gadget containing other gadgets
 **********************************************************************************/

/// Includes
#include "gadgetgroup.hpp"
///

/// GadgetGroup::GadgetGroup
// A "meta gadget" that groups various gadgets together. We derive
// this from the gadget list such that we can straightforwardly add
// gadgets to it.
GadgetGroup::GadgetGroup(List<Gadget> &gadgetlist,
			       class RenderPort *rp,LONG le,LONG te,LONG w,LONG h)
  : Gadget(gadgetlist,rp,le,te,w,h), ActiveGadget(NULL)
{
}
///

/// GadgetGroup::~GadgetGroup
// Dispose this gadget and all subgadgets of this gadget
GadgetGroup::~GadgetGroup(void)
{
  class Gadget *next;

  // Gadgets remove themselves from the list when deleted, hence do not 
  // unlink us here.
  while((next = First())) {
    delete next;
  }
}
///

/// GadgetGroup::Refresh
// Refresh all gadgets within this gadget group
void GadgetGroup::Refresh(void)
{
  class Gadget *gadget;

  for(gadget = First();gadget;gadget = gadget->NextOf()) {
    gadget->Refresh();
  }
}
///

/// GadgetGroup::HitTest
// Get the active gadget within this gadget group by running
// the hit-test of all gadgets here.
bool GadgetGroup::HitTest(struct Event &ev)
{
  // If we have an active gadget, just forward this to the active
  // gadget. Otherwise, forward this to all gadgets.
  if (ActiveGadget) {
    if (ActiveGadget->HitTest(ev)) {
      if (ev.Type == Event::GadgetUp || ev.Type == Event::Request) {
	ActiveGadget = NULL;
      }
      return true; // The gadget remains active
    }
    // Otherwise, it is no longer active.
    ActiveGadget = NULL;
  } 
  {
    class Gadget *gadget;
    // Check whether we have any gadget that passes the hit-test.
    // If so, we make this the active gadget.
    for(gadget = First();gadget;gadget = gadget->NextOf()) {
      if (gadget->HitTest(ev)) {
	ActiveGadget = gadget;
	return true;
      }
    }
    return false;
  }
}
///

/// GadgetGroup::MoveGadget
// Move this gadget (and its contents) by a given amount
void GadgetGroup::MoveGadget(LONG dx,LONG dy)
{
  class Gadget *gadget;
  //
  LeftEdge += dx;
  TopEdge  += dy;
  // Recursively move all subgadgets by the same distance.
  for(gadget = First();gadget;gadget = gadget->NextOf()) {
    gadget->MoveGadget(dx,dy);
  }
}
///

/// GadgetGroup::FindGadgetInDirection
// Check for the nearest gadget in the given direction dx,dy,
// return this (or a sub-gadget) if a suitable candidate has been
// found, alter x and y to a position inside the gadget, return NULL
// if this gadget is not in the direction.
const class Gadget *GadgetGroup::FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const
{
  // At least in this group.
  return Gadget::FindGadgetInDirection(*this,x,y,dx,dy);
}
///
