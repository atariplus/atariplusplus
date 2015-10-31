/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menufileitem.hpp,v 1.3 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: A direct descended of the menu item that generates requester
 ** events to open a file requester from a base name.
 **********************************************************************************/

#ifndef MENUFILEITEM_HPP
#define MENUFILEITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "menuitem.hpp"
///

/// Class MenuFileItem
// The MenuActionItem is a direct descended of the menu item 
// that generates requester events to open a file 
// requester from a base name.
class MenuFileItem : public MenuItem {
  //
  //
public:
  MenuFileItem(class MenuSuperItem *parent,const char *text);
  virtual ~MenuFileItem(void);
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
