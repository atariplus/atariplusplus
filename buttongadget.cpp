/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: buttongadget.cpp,v 1.7 2021/07/03 15:48:34 thor Exp $
 **
 ** In this module: Definition of the button gadget
 **********************************************************************************/

/// Includes
#include "buttongadget.hpp"
#include "renderport.hpp"
///

/// Menu::ButtonGadget::ButtonGadget
// A gadget with a text rendered within
ButtonGadget::ButtonGadget(List<Gadget> &gadgetlist,
				 class RenderPort *rp,LONG le,LONG te,LONG w,LONG h,
				 const char *body)
  : Gadget(gadgetlist,rp,le,te,w,h), ButtonText(body), HitImage(false)
{ }
///

/// ButtonGadget::~ButtonGadget
ButtonGadget::~ButtonGadget(void)
{
}
///

/// ButtonGadget::HitTest
// Perform action if the gadget was hit, resp. release the gadget.
bool ButtonGadget::HitTest(struct Event &ev)
{
  switch(ev.Type) {
  case Event::Mouse:
    // A mouse move event. We ignore it unless we are active.
    if (Active) {
      bool oldhit = HitImage;
      HitImage    = Within(ev);
      // Avoid re-rendering unless we
      // change state
      if (oldhit != HitImage)
	Refresh();
      ev.Type   = Event::GadgetMove;
      ev.Object = this;
      return true;
    }
    return false;
  case Event::Click:
    if (ev.Button && Within(ev)) {
      // We are hit within the gadget, so active it.
      Active   = true;
      HitImage = true;
      Refresh();
      ev.Type   = Event::GadgetDown;
      ev.Object = this;
      return true;
    } else if (ev.Button == false && Active) {
      // We have been active, but the button has been released.
      // Generate a GadgetUp, but only validate the gadget if we are within
      // the gadget.
      ev.Type   = Event::GadgetUp;
      ev.Object = Within(ev)?(this):(NULL);
      Active    = false;
      HitImage  = false;
      Refresh();
      return true;
    }
    return false;
  default:
    return false;
  }
}
///

/// ButtonGadget::Refresh
// Refresh the button gadget frame and text.
void ButtonGadget::Refresh(void)
{
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,4);
  RPort->Draw3DFrame(LeftEdge,TopEdge,Width,Height,HitImage);
  RPort->TextClip(LeftEdge+2,TopEdge+2,Width-4,Height-4,ButtonText,15);
}
///
