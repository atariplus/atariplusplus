/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menufileitem.cpp,v 1.3 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: A direct descended of the menu item that generates requester
 ** events to open a file requester from a base name.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "new.hpp"
#include "event.hpp"
#include "menufileitem.hpp"
///

/// MenuFileItem::MenuFileItem
// Create a menu action item; this is straight.
MenuFileItem::MenuFileItem(class MenuSuperItem *parent,const char *text)
  : MenuItem(parent,text)
{
}
///

/// MenuFileItem::~MenuFileItem
// The destructor. All this happens in the menu item, no need to
// do any magic here.
MenuFileItem::~MenuFileItem(void)
{
}
///

/// MenuFileItem::HitTest
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving. 
// This here generates high-level events.
bool MenuFileItem::HitTest(struct Event &ev,class BufferPort *port)
{
  if (MenuItem::HitTest(ev,port)) {
    // This item has been hit and released. 
    // Generate a high-level event instead.
    if (ev.Type == Event::MenuPick) {
      ev.Type      = Event::Request;
      ev.Menu      = this;
    }
    return true;
  }
  return false;
}
///
