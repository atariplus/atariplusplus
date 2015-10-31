/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuboolitem.hpp,v 1.6 2015/05/21 18:52:40 thor Exp $
 **
 ** 
 ** In this module: Definition of the menu item class that is toggleable
 ** and carries a boolean data that can be picked.
 **********************************************************************************/

#ifndef MENUBOOLITEM_HPP
#define MENUBOOLITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "menuitem.hpp"
///

/// Class MenuBoolItem
// This class represents a toggle-able menu item
// with two states (on/off)
class MenuBoolItem : public MenuItem {
  //
  // The state of this item is here.
  bool State;
  //
public:
  MenuBoolItem(class MenuSuperItem *parent,const char *text);
  virtual ~MenuBoolItem(void);
  //
  // Get and set the state of this item.
  bool GetState(void) const
  {
    return State;
  }
  //
  // Define the state of this item.
  void SetState(bool state)
  {
    State = state;
  }
  //
  // Rendering engine. This renders the additional marker that
  // is used to indicate the boolean state
  virtual void RenderItem(class RenderPort *port,bool picked);  
  //
  // Feed events into the menu, and perform a hit-test. Modify the event
  // possibly. We also need to give a buffer port here since
  // the menu must render into the screen while the mouse
  // is moving.
  virtual bool HitTest(struct Event &ev,class BufferPort *port);
  //
};
///

///
#endif
