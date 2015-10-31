/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menurootitem.hpp,v 1.10 2015/05/21 18:52:40 thor Exp $
 **
 ** 
 ** In this module: Definition of the menu item class that keeps subitems,
 ** here specifically in the root menu.
 **********************************************************************************/

#ifndef MENUROOTITEM_HPP
#define MENUROOTITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "menuitem.hpp"
#include "menusuperitem.hpp"
///

/// Class MenuRootItem
// An extended super item that carries the
// root menu. Some rendering is done a bit
// differently here.
class MenuRootItem : public MenuSuperItem { 
  //
  // The following methods are overloaded because they
  // look different than that of the super item.
  //
  // Layout this item, i.e. compute the position of all
  // its sub-items on the screen givne the
  // anchor position of this item. This does not yet draw
  // the item list, though.
  virtual void LayoutMenu(LONG w,LONG h);
  //
  // Open a submenu by rendering it into the buffer port
  virtual void ShowMenu(class BufferPort *port);
  //
  // Perform event handling on all subitems.
  virtual bool CheckSubItems(struct Event &ev,class BufferPort *port);
  //
public:
  // No parent here.
  MenuRootItem(void);
  virtual ~MenuRootItem(void);
  //
  // Overloaded with operator: This returns the full width
  virtual LONG WidthOf(void) const;
  // Return the height of the bar: Render one additional line
  virtual LONG HeightOf(void) const;
  //
  // Display the root menu on the screen: This performs the layout
  // process and then renders the menu.
  void DisplayMenu(class BufferPort *port);
  //
  // Hide the menu again. This is just the same call as that of the
  // super item, we just make it public here.
  virtual void HideMenu(class BufferPort *port)
  {
    MenuSuperItem::HideMenu(port);
  }
  //
  // Event handling. This converts MenuAbort to high-level events.
  // Feed events into the menu, and perform a hit-test. Modify the event
  // possibly. We also need to give a buffer port here since
  // the menu must render into the screen while the mouse
  // is moving.
  virtual bool HitTest(struct Event &ev,class BufferPort *port);
};
///

///
#endif
