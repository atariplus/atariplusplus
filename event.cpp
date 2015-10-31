/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: event.cpp,v 1.11 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of GUI events
 **********************************************************************************/

/// Includes
#include "event.hpp"
#include "gadget.hpp"
#include "display.hpp"
#include "keyboard.hpp"
#include "gamecontroller.hpp"
///

/// EventFeeder::PositionMouse
// Re-position the mouse pointer in the dx,dy direction
void EventFeeder::PositionMouse(int dx,int dy)
{
  struct Event sent;
  const class Gadget *gad;
  bool button;
  LONG x,y,mx;
  // Locate the nearest gadget and move the mouse over it.
  Display->MousePosition(x,y,button);
  mx = x;
  gad = Gadget::FindGadgetInDirection(*GList,x,y,dx,dy);
  if (gad == NULL) {
    // If there is no gadget in that direction, try to
    // stay on the current gadget.
    gad = Gadget::FindGadgetInDirection(*GList,x,y,0,0);
  }
  if (gad) {
    // Check whether the target position is in the window. If not,
    // create a mouse wheel event to scroll the contents and try
    // again.
    if (y < 0) {
      sent.Type          = Event::Wheel;
      sent.ScrolledLines = -1;
      sent.X             = mx;
      sent.Y             = 0;
      sent.Button        = button;
      ForwardEvent(sent);
      Gadget::FindGadgetInDirection(*GList,x,y,0,1);
    } else if (y >= RPort->HeightOf()) {
      sent.Type          = Event::Wheel;
      sent.ScrolledLines = 1;
      sent.X             = mx;
      sent.Y             = RPort->HeightOf() - 1;
      sent.Button        = button;
      ForwardEvent(sent);
      Gadget::FindGadgetInDirection(*GList,x,y,0,-1);
    }
    if (x >= 0 && x < RPort->WidthOf() && y >= 0 && y < RPort->HeightOf())
      Display->SetMousePosition(x,y);
  }
}
///

/// EventFeeder::ClickMouse
// Create a mouse click event/active a gadget. Return the
// state change.
int EventFeeder::ClickMouse(void)
{
  struct Event sent;
  LONG x,y;
  bool button;
  // Generate a mouse hit into the gadget.
  Display->MousePosition(x,y,button);
  sent.Type   = Event::Click;
  sent.X      = x;
  sent.Y      = y;
  sent.Button = true;
  ForwardEvent(sent);
  sent.Button = false;
  return ForwardEvent(sent);
}
///

/// EventFeeder::ForwardEvent
// Forward an event to all gadget that are part of the list maintained by
// this event feeder
int EventFeeder::ForwardEvent(struct Event &ev)
{
  class Gadget *gadget;
  int change = Nothing;
  struct Event sent;
  bool hit   = false;
  //
  do {
    sent = ev;
    for(gadget  = GList->First();gadget;gadget = gadget->NextOf()) {
      if (gadget->HitTest(sent)) {
	hit = true;
	// Check whether we got a control event. If so, then we set the
	// action ID of the main menu.
	if (sent.Type == Event::Ctrl) {
	  change = sent.ControlId;
	  // Store changes back so the outside sees what we did with the event
	  ev     = sent;
	  break;
	}
      }
    }
  } while(sent.Resent);
  //
  // If no gadget hit, check whether this is potentially a keyboard movement event.
  if (hit == false) {
    if (sent.Type == Event::Keyboard) {
      if (sent.DownUp && sent.Control) {
	WORD dx = 0,dy = 0;
	// Potentially a cursor key movement
	switch(sent.Key) {
	case '-': // Up.
	  dy = -1;
	  break;
	case '=': // Down
	  dy =  1;
	  break;
	case '+': // Left
	  dx = -1;
	  break;
	case '*': // Right
	  dx =  1;
	  break;
	}
	if (dx || dy)
	  PositionMouse(dx,dy);
      } else if (!sent.DownUp && !sent.Control && (sent.Key == 0x0a || sent.Key == 0x0d)) {
	// Return->Button press
	return ClickMouse();
      }
    } else if (sent.Type == Event::Joystick) {
      if (sent.Button) {
	// Button pressed?
	return ClickMouse();
      } else {
	if (sent.X || sent.Y)
	  PositionMouse(sent.X,sent.Y);
      }
    }
  }

  return change;
}
///

/// EventFeeder::PickedOption
// Create events and feed them into the gadgets. Returns true if any option
// changed. In this case, we would need to re-parse the parameters.
int EventFeeder::PickedOption(struct Event &event)
{ 
  LONG x,y;
  bool button;
  int change = Nothing;
  int ctrl;
  //
  // This main loop seems indeed strange and is not at all "event driven"
  // in the classical sense. The problem here is, though, that the display
  // frontend is so utterly primitive that we cannot depend on any kind of
  // events it may or may not generate. Hence, we need to use a "polling
  // type" front end here.
  //
  // Update the GUI by reading the keyboard event.
  event.DownUp      = Keyboard->ReadKey(event.Key,event.Shift,event.Control);
  // Eat up this event.
  Keyboard->KeyboardInterrupt();
  if (event.DownUp != LastDownUp || event.Key != LastKey) {
    if (event.DownUp || KeyDownFound) {
      change          = Comeback;
      AutoCounter     = 15;
      LastDownUp      = event.DownUp;
      LastKey         = event.Key;
      event.Type      = Event::Keyboard;
      if (event.DownUp)
	KeyDownFound  = true;
      if ((ctrl = ForwardEvent(event)))
	change      = ctrl;
    }
  } else if (event.DownUp) {
    KeyDownFound  = true;
    // Check whether the auto repeat counter is done.
    if (--AutoCounter <= 0) {
      // repeat the down event.
      AutoCounter     = 2;
      event.Type      = Event::Keyboard;
      if ((ctrl = ForwardEvent(event)))
	change        = ctrl;
    }
  }
  //
  if (change == Nothing) {
    LONG lines = Display->ScrollDistance();
    if (lines != 0) { 
      Display->MousePosition(x,y,button);
      // Ok, generate an event for the mouse wheel.
      event.Type          = Event::Wheel;
      event.ScrolledLines = lines; 
      event.Button        = button;
      event.X             = x;
      event.Y             = y;
      if ((ctrl = ForwardEvent(event)))
	change  = ctrl;
    }
  }
  //
  //
  if (change == Nothing) {
    // Update the GUI by reading the mouse position from it.
    Display->MousePosition(x,y,button);
    // Construct an event from the mouse position and the button position.
    event.Type   = Event::Mouse;
    if (button  != LastButton) {
      if (x >= 0 && y >= 0 && x < RPort->WidthOf() && y < RPort->HeightOf()) {
	event.Type = Event::Click;
      } else {
	// Ignore button presses outside the window.
	return change;
      }
    }
    if (button || MouseDownFound) {
      if (button)
	MouseDownFound = true;
      event.Button = button;
      event.X      = x;
      event.Y      = y;
      if ((ctrl = ForwardEvent(event)))
	change     = ctrl;
      LastButton   = button;
    }
  }
  //
  // Check whether we may savely read the joystick 
  // (namely, only if it didn't allocate the mouse). If so, create joystick events.
  if (change == Nothing) {
    if (Joystick && Display->MouseIsAvailable()) {
      LONG dx = 0,dy = 0;
      bool button;
      int stick;
      // Yes, really different from the mouse.
      stick  = Joystick->Stick();
      button = Joystick->Strig();
      //
      // Read the direction.
      if ((stick & 0x01) == 0) dy = -1;
      if ((stick & 0x02) == 0) dy = +1;
      if ((stick & 0x04) == 0) dx = -1;
      if ((stick & 0x08) == 0) dx = +1;
      //
      if (stick != LastStick || button != LastStrig) {
	event.Type   = Event::Joystick;
	event.X      = dx;
	event.Y      = dy;
	event.Button = button;
	LastStrig    = button;
	LastStick    = stick;
	if ((ctrl = ForwardEvent(event)))
	  change     = ctrl;
      }
    }
  }
  
  return change;
}
///
