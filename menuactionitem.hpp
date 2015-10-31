/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuactionitem.hpp,v 1.3 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: A direct descended of the menu item that generates high-
 ** level events that request specific activity from the supervisor.
 **********************************************************************************/

#ifndef MENUACTIONITEM_HPP
#define MENUACTIONITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "menuitem.hpp"
///

/// Class MenuActionItem
// The MenuActionItem is a direct descended of the menu item 
// that generates high-level events that request specific 
// activity from the supervisor.
class MenuActionItem : public MenuItem {
  //
  // The high-level action identifier that we invoke
  int Action;
  //
public:
  MenuActionItem(class MenuSuperItem *parent,const char *text,int action);
  virtual ~MenuActionItem(void);
  //   
  // Feed events into the menu, and perform a hit-test. Modify the event
  // possibly. We also need to give a buffer port here since
  // the menu must render into the screen while the mouse
  // is moving.
  virtual bool HitTest(struct Event &ev,class BufferPort *port); 
};
///

///
#endif
