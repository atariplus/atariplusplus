/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuboolitem.cpp,v 1.8 2015/05/21 18:52:40 thor Exp $
 **
 ** 
 ** In this module: Definition of the menu item class that is toggleable
 ** and carries a boolean data that can be picked.
 **********************************************************************************/

/// Includes
#include "menuboolitem.hpp"
#include "renderport.hpp"
///

/// MenuBoolItem::MenuBoolItem
MenuBoolItem::MenuBoolItem(class MenuSuperItem *parent,const char *text)
  : MenuItem(parent,text)
{
  IsToggleAble = true;
}
///

/// MenuBoolItem::~MenuBoolItem
MenuBoolItem::~MenuBoolItem(void)
{
  // nothing to do here
}
///

/// MenuBoolItem::RenderItem
void MenuBoolItem::RenderItem(class RenderPort *port,bool picked)
{
  // Let the super method render here first.
  MenuItem::RenderItem(port,picked);
  // Now render the dot for whether we are selected or not.
  if (Width >= 8) {
    if (State) {
      port->Draw3DFrame(LeftEdge+1,TopEdge+1,6,6,true);
      port->CleanBox(LeftEdge+2,TopEdge+2,4,4,0x00);
    } else {
      port->Draw3DFrame(LeftEdge+1,TopEdge+1,6,6,false);      
      port->CleanBox(LeftEdge+2,TopEdge+2,4,4,0x08);
    }
  }
}
///

/// MenuBoolItem::HitTest
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving.
bool MenuBoolItem::HitTest(struct Event &ev,class BufferPort *port)
{
  // Perform the hit test on the item. If we get hit, toggle
  // the boolean state.
  if (MenuItem::HitTest(ev,port)) {
    if (ev.Type == Event::MenuPick) {
      State = !State;
    }
    return true;
  }
  return false;
}
///
