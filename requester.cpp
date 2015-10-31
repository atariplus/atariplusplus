/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: requester.cpp,v 1.13 2014/06/01 20:07:53 thor Exp $
 **
 ** In this module: A generic requester class
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "machine.hpp"
#include "display.hpp"
#include "gadget.hpp"
#include "event.hpp"
#include "requester.hpp"
#include "timer.hpp"
#include "gadgetgroup.hpp"
#include "new.hpp"
///

/// Requester::Requester
// Build up an requester. This only initializes all stuff, it does not build
// up all classes.
Requester::Requester(class Machine *mach)
  : Machine(mach), RPort(NULL), EventFeeder(NULL), TopLevel(NULL)
{ }
///

/// Requester::~Requester
// Destroy the requester: Remove and dispose all gadgets, and the eventfeeder
// and the renderport.
Requester::~Requester(void)
{  
  delete RPort;
  RPort = NULL;
  
  ShutDown();
}
///

/// Requester::CleanupGadgets
void Requester::CleanupGadgets(void)
{
  // In case we run into an error in the constructor,
  // perform nothing here.
}
///

/// Requester::RequesterGadget::RequesterGadget
// Build up a requester gadget by the gadget
// group and remembering the requester here.
Requester::RequesterGadget::RequesterGadget(List<class Gadget> &GList,class RenderPort *rport,
					    LONG le,LONG te,LONG w,LONG h,
					    class Requester *top)
  : GadgetGroup(GList,rport,le,te,w,h), Container(top)
{  
  // Now insert custom gadgets into this requester.
  top->BuildGadgets(*this,RPort);
}
///

/// Requester::RequesterGadget::~RequesterGadget
Requester::RequesterGadget::~RequesterGadget(void)
{  
  // Call the gadget cleanup method. The sub-class might perform
  // miscellaneous cleanup work here.
  Container->CleanupGadgets();
}
///

/// Requester::RequesterGadget::HitTest
// Perform action if the gadget was hit, resp. release the gadget.
bool Requester::RequesterGadget::HitTest(struct Event &ev)
{
  if (GadgetGroup::HitTest(ev)) {
    int change;
    // Try to get a reaction from the user call-back. If so, change the event
    // to top-level and return it.
    change = Container->HandleEvent(ev);
    if (change) {
      // Ok, paste this into the event.
      ev.Type      = Event::Ctrl;
      ev.ControlId = change;
    }
    return true;
  }
  return false;
}
///

/// Requester::ShutDown
// Shut down the requester without deleting it
// entirely.
void Requester::ShutDown(void)
{  
  class AtariDisplay *display;
  class Gadget *gadget;
  
  if (RPort) {
    display = Machine->Display();
    // We might not have a display (and hence the requester could not be build)
    if (display) {
      display->EnforceFullRefresh();
      display->ShowPointer(false);
    }
    RPort->Link(NULL);
  }
  //
  while((gadget = GList.First())) {
    // Gadgets unlink themselves when deleted.
    delete gadget;
  }
  // There is actually only this gadget, but hey...
  TopLevel    = NULL;
  delete EventFeeder;
  EventFeeder = NULL;
  delete RPort;
  RPort       = NULL;
}
///

/// Requester::BuildUp
// Build the requester and all the gadgets.
// This internal method prepares the graphics
// for the requester.
bool Requester::BuildUp(void)
{
  class AtariDisplay *display;
  class Gadget *gadget;
#if CHECK_LEVEL > 0
  if (EventFeeder || RPort || TopLevel) {
    Throw(ObjectExists,"Requester::BuildUp",
	  "The GUI has been build up already");
  }
#endif  
  // We need to build a new render port.
  display = Machine->Display();
  if (display == NULL)
    return false;
  //
  // The following might trigger an error again.
  // Fail in this case, do not deliver the error that
  // caused the requester.
  try {
      RPort = new class RenderPort;
      RPort->Link(Machine);
      RPort->SetPen(8);
      RPort->FillRaster(); // grey background.  
      //
      //
      TopLevel = new class RequesterGadget(GList,RPort,0,0,320,192,this);
      //
      // get the first gadget and iterate until its over.
      for(gadget = GList.First();gadget;gadget = gadget->NextOf()) {
	  gadget->Refresh();
      }
      EventFeeder = new class EventFeeder(display,Machine->Keyboard(),Machine->Joystick(0),
					  &GList,RPort);
      display->ShowPointer(true);
      display->EnforceFullRefresh();  
      RPort->Refresh();
      return true;
  } catch(...) {
      ShutDown();
      return false;
  }
}
///

/// Requester::Request
// Build-up the requester, capture events and perform the custom hooks
// of an overloaded requester sub-class
int Requester::Request(void)
{
  class Timer eventtimer;
  struct Event event;
  int change;
  //
  if (BuildUp() == false)
    return 0; // Requester could not be created. Yuck!
  //
  // Now for the event loop.
  eventtimer.StartTimer(0,25*1000); // use a 25 msec cycle here.
  do {
    // Get the next event request.
    change = EventFeeder->PickedOption(event);
    if (change == RQ_Comeback) {
      change = int(RQ_Nothing);
      continue;
    }
    //
    // Otherwise, refresh the contents and delay.
    RPort->Refresh();
    eventtimer.WaitForEvent(); // Now wait 25msecs
    eventtimer.TriggerNextEvent();
  } while(change == int(RQ_Nothing));

  ShutDown();
  return change;
}
///

/// Requester::isHeadLess
// Return an indicator whether this requester is head-less, i.e. has no GUI.
bool Requester::isHeadLess(void) const
{
  return !Machine->hasGUI();
}
///

/// Requester::SwitchGUI
// Make the GUI visible or invisible.
void Requester::SwitchGUI(bool foreground) const
{
  class AtariDisplay *d = Machine->Display();

  if (d)
    d->SwitchScreen(foreground);
}
///

