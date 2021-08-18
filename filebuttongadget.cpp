/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: filebuttongadget.cpp,v 1.6 2021/07/03 15:48:34 thor Exp $
 **
 ** In this module: Definition of a graphical button gadget 
 ** that brings up a file requester
 **********************************************************************************/

/// Includes
#include "filebuttongadget.hpp"
///

/// FileButtonGadget::FileButtonGadget
// A gadget with a text rendered within
FileButtonGadget::FileButtonGadget(List<Gadget> &gadgetlist,
				   class RenderPort *rp,LONG le,LONG te,LONG w,LONG h)
  : Gadget(gadgetlist,rp,le,te,w,h), HitImage(false)
{ }
///

/// FileButtonGadget::HitTest
// Perform action if the gadget was hit, resp. release the gadget.
bool FileButtonGadget::HitTest(struct Event &ev)
{
  switch(ev.Type) {
  case Event::Mouse:
    // A mouse move event. We ignore it unless we are active.
    if (Active) {
      HitImage = Within(ev);
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

/// FileButtonGadget::Refresh
// Refresh the button gadget frame and text.
void FileButtonGadget::Refresh(void)
{
  LONG xmin,ymin,xmax,ymax;
  //
  RPort->CleanBox(LeftEdge,TopEdge,Width,Height,4);
  RPort->Draw3DFrame(LeftEdge,TopEdge,Width,Height,HitImage);
  //
  // Compute boundaries
  xmin  = LeftEdge + 3;
  xmax  = LeftEdge + Width  - 4;
  ymin  = TopEdge  + 2;
  ymax  = TopEdge  + Height - 3;
  //
  RPort->SetPen(0x0f);
  RPort->DrawFrame(xmin,ymin,xmax,ymax);
  RPort->SetPen(4);
  RPort->DrawFrame(xmax,ymin,xmax,ymin);
  ymax  = ymin + 2;
  xmin += 2;
  xmax -= 3;
  RPort->SetPen(0x0f);
  RPort->FillRectangle(xmin,ymin,xmax,ymax);
  ymin++;
  ymax--;
  xmax--;
  xmin = xmax - 1;
  RPort->SetPen(4);
  RPort->FillRectangle(xmin,ymin,xmax,ymax);
  //
  xmin  = LeftEdge + 5;
  xmax  = LeftEdge + Width  - 6;
  ymax  = TopEdge  + Height - 4;
  ymin  = ymax - 2;
  RPort->SetPen(8);
  RPort->FillRectangle(xmin,ymin,xmax,ymax);
}
///

/// FileButtonGadget::~FileButtonGadget
FileButtonGadget::~FileButtonGadget(void)
{
}
///
