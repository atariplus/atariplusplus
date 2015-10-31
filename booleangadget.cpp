/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: booleangadget.cpp,v 1.4 2015/05/21 18:52:36 thor Exp $
 **
 ** In this module: Definition of a single on/off radio button
 **********************************************************************************/

/// Includes
#include "booleangadget.hpp"
///

/// BooleanGadget::BooleanGadget
// A boolean gadget indicates a toggle-able on/off value together with
// a text explaining what it does.
BooleanGadget::BooleanGadget(List<Gadget> &gadgetlist,
				   class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
				   const char *label,bool initialstate)
  : Gadget(gadgetlist,rp,le,te,w,h), GadgetLabel(label), Toggle(initialstate)
{ 
}
///

/// BooleanGadget::~BooleanGadget
BooleanGadget::~BooleanGadget(void)
{ 
}
///

/// BooleanGadget::HitTest
bool BooleanGadget::HitTest(struct Event &ev)
{
  // If we click the button within the inactive gadget, toggle the state 
  // and activate it. We only care about click events, everything else is
  // not required here.
  switch(ev.Type) {
  case Event::Click:
    if (Within(ev) && ev.Button && Active == false) {
      Active = true;
      Toggle = !Toggle;
      Refresh();
      ev.Type   = Event::GadgetDown;
      ev.Object = this;
      return true;
    } else if (ev.Button==false && Active) {
      // de-activate again if we're releasing the button.
      Active    = false;
      ev.Type   = Event::GadgetUp;
      ev.Object = this;
      return true;
    }
    return false;
  case Event::Mouse:
    if (Active) {
      ev.Type   = Event::GadgetMove;
      ev.Object = NULL;
      return true;
    }
    return false;
  default:
    return false;
  }
}
///

/// BooleanGadget::Refresh
void BooleanGadget::Refresh(void)
{
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,0x08);
  // Draw a raised or recessed box showing the state of the gadget.
  RPort->Draw3DFrame(LeftEdge + 2,TopEdge + 2,Height - 4,Height - 4,Toggle);
  // Fill this box with black in case we're active.
  if (Toggle) {
    RPort->CleanBox(LeftEdge + 4, TopEdge + 4, Height - 8, Height - 8,0x00);
  }
  // Now render the label text behind it.
  RPort->TextClipLefty(LeftEdge + Height + 4,TopEdge,Width - Height - 4,Height,
		       GadgetLabel,0x0f);
}
///

/// BooleanGadget::SetStatus
// Modify the status of the boolean gadget.
void BooleanGadget::SetStatus(bool status)
{
  if (!Active) {
    // Only if the user doesn't currently hold this gadget.
    Toggle = status;
    Refresh();
  }
}
///
