/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuactionitem.cpp,v 1.5 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: A direct descended of the menu item that generates high-
 ** level events that request specific activity from the supervisor.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "new.hpp"
#include "event.hpp"
#include "menuactionitem.hpp"
///

/// MenuActionItem::MenuActionItem
// Create a menu action item; this is straight.
MenuActionItem::MenuActionItem(class MenuSuperItem *parent,const char *text,int action)
  : MenuItem(parent,text), Action(action)
{
}
///

/// MenuActionItem::~MenuActionItem
// The destructor. All this happens in the menu item, no need to
// do any magic here.
MenuActionItem::~MenuActionItem(void)
{
}
///

/// MenuActionItem::HitTest
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving. 
// This here generates high-level events.
bool MenuActionItem::HitTest(struct Event &ev,class BufferPort *port)
{
  if (MenuItem::HitTest(ev,port)) {
    // This item has been hit and released. 
    // Generate a high-level event instead.
    if (ev.Type == Event::MenuPick) {
      ev.Type      = Event::Ctrl;
      ev.ControlId = Action;
    }
    return true;
  }
  return false;
}
///
