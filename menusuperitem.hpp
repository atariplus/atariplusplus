/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menusuperitem.hpp,v 1.10 2015/05/21 18:52:41 thor Exp $
 **
 ** 
 ** In this module: Definition of the menu item class that keeps subitems.
 ** This class is only an abstract interface that gets implemented by the
 ** root and vert menu items.
 **********************************************************************************/

#ifndef MENUSUPERITEM_HPP
#define MENUSUPERITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "menuitem.hpp"
#include "bufferport.hpp"
///

/// Class MenuSuperItem
// This class carries a list of subitems and is
// hence a super item. This class is only an abstract 
// interface that gets implemented by the root and vert menu items.
class MenuSuperItem : public MenuItem, public List<class MenuItem> {
protected:
  //
  // In case a subitem is active, its pointer is here
  class MenuItem              *ActiveItem;
  //
  // In case the menu has been rendered, here's
  // the buffer for the data that is hidden
  // by it.
  struct BufferPort::Backsave *Backsave;
  // Anchor position of the sub-items of this item on the
  // screen.
  LONG                         AnchorX,AnchorY;
  // Size of the region we render into.
  LONG                         SubWidth,SubHeight;
  //
  // If this is the root window: Maximal length of a subitem
  // Also used as such for recursive sub-items.
  LONG                         SubItemLimit;
  //
  // Check all sub items (event-handling) for all items between the
  // first, inclusive, and last exclusive. Return true if an item
  // got activated.
  bool CheckSubItems(struct Event &ev,class BufferPort *port,
		     class MenuItem *first,class MenuItem *last);
  //
  // Layout this item, i.e. compute the position of all
  // its sub-items on the screen givne the
  // anchor position of this item. This does not yet draw
  // the item list, though. Might probably want to use the
  // above protected IsActivated() helper.
  virtual void LayoutMenu(LONG w,LONG h) = 0;
  //
  // Open a submenu by rendering it into the buffer port
  virtual void ShowMenu(class BufferPort *port) = 0;
  // Close a submenu again. This is the default
  // implementation an inherited subclass may overwrite.
  virtual void HideMenu(class BufferPort *port); 
  //
  // Check the sub-items of this item for activation. This must
  // happen here because only we - and not the subitems -
  // can redraw them.
  virtual bool CheckSubItems(struct Event &ev,class BufferPort *port) = 0;
  //
public:
  MenuSuperItem(class MenuSuperItem *parent,const char *text);
  virtual ~MenuSuperItem(void);
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
