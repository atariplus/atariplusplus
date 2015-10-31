/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: radiogadget.cpp,v 1.5 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of a mutually exclusive arrangement of buttons
 ** forming a radio-button (array)
 **********************************************************************************/

/// Includes
#include "radiogadget.hpp"
#include "booleangadget.hpp"
#include "new.hpp"
///

/// RadioGadget::RadioGadget
// A radio button gadget. This is a gadget group build on several
// boolean gadgets.
RadioGadget::RadioGadget(List<Gadget> &gadgetlist,
			 class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
			 const struct ArgParser::SelectionVector *items,
			 LONG initial)
  : GadgetGroup(gadgetlist,rp,le,te,w,h), Selection(initial), Items(items)
{
  // Now build up the boolean gadgets that are part of this radio gadget
  while(items->Name) {
    new BooleanGadget(*this,RPort,le,te,w,12,items->Name,items->Value == initial);
    te += 12;
    items++;
  }
  Height = te - TopEdge;
}
///

/// RadioGadget::~RadioGadget
// Destroy a radio gadget. Luckely, there is not much to do as the
// gadget group already disposes all subgadgets
RadioGadget::~RadioGadget(void)
{
}
///

/// RadioGadget::HitTest
bool RadioGadget::HitTest(struct Event &ev)
{
  // We only react on "click" events. Everything else is not the buisiness of
  // this class.
  switch(ev.Type) {
  case Event::Click:
    // If we have an active gadget, check whether the button goes up and 
    // return a gadget up even then.
    if (Active && ActiveGadget->HitTest(ev)) {
      ev.Object = this; // make us appear as a single gadget
      if (ev.Type == Event::GadgetUp) {
	// Ok, the active gadget goes up. Deliver the gadgetup and make
	// us inactive.
	ActiveGadget = NULL;
	Active       = false;
      }
      return true;
    } else if (ev.Button && Active == false) {
      class BooleanGadget *gadget,*other;
      const struct ArgParser::SelectionVector *item;      
      // We are inactive. Check whether any of the inactive subgadgets was hit
      // if so,make it active.
      // We check here whether a boolean gadgets in its "off" state is hit. If
      // so, we enable it and disable all others.
      for(gadget = (class BooleanGadget *)First();gadget;
	  gadget = (class BooleanGadget *)(gadget->NextOf())) {
	if (gadget->GetStatus() == false && gadget->HitTest(ev)) {
	  // Ok, we hit this inactive gadget. Now modify all other gadgets.
	  for(item  = Items,other = (class BooleanGadget *)First();other && item->Name;
	      other = (class BooleanGadget *)(other->NextOf()),item++) {
	    if (gadget == other) {
	      // We fould our's. Keep it active, but also keep the selection.
	      Selection = item->Value;
	    } else {
	      // disable all other gadgets
	      other->SetStatus(false);
	    }
	  }
	  // Make this gadget the active gadget and activate us
	  ActiveGadget = gadget;
	  Active       = true;
	  ev.Object    = this; // appear as a single gadget;
	  return true;
	}
      }
    }
    return false;
  case Event::Mouse:
    // Just a mouse movement. If we're active, deliver a gadget move.
    if (Active) {
      ev.Type   = Event::GadgetMove;
      ev.Object = this;
      return true;
    }
    return false;
  default:
    return false;
  }
}
///

/// RadioGadget::Refresh
// Refresh the radio gadget. This does nothing as the gadget group does this for us.
void RadioGadget::Refresh(void)
{
  GadgetGroup::Refresh();
}
///

/// RadioGadget::SetStatus
// Update the status of the selection gadget here
void RadioGadget::SetStatus(LONG select)
{
  class BooleanGadget *other;
  const struct ArgParser::SelectionVector *item;
  
  for(item  = Items,other = (class BooleanGadget *)First();other && item->Name;
      other = (class BooleanGadget *)(other->NextOf()),item++) {
    if (item->Value == select) {
      // We fould our's. Set it active, but also keep the selection.
      other->SetStatus(true);
      Selection = item->Value;
    } else {
      other->SetStatus(false);
    }
  }
}
///
