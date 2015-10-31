/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menusuperitem.cpp,v 1.15 2015/05/21 18:52:41 thor Exp $
 **
 ** 
 ** In this module: Definition of the menu item class that keeps subitems. 
 ** This class is only an abstract interface that gets implemented by the
 ** root and vert menu items.
 **********************************************************************************/

/// Includes
#include "new.hpp"
#include "bufferport.hpp"
#include "menusuperitem.hpp"
#include "stdio.hpp"
///

/// MenuSuperItem::MenuSuperItem
// Build up a super item. The list
// gets initialized by itself.
MenuSuperItem::MenuSuperItem(class MenuSuperItem *parent,const char *text)
  : MenuItem(parent,text),
    ActiveItem(NULL), Backsave(NULL)
{
  HasSubItems = true;
}
///

/// MenuSuperItem::~MenuSuperItem
// Dispose a superitem. This also disposes
// all subitems of this item.
MenuSuperItem::~MenuSuperItem(void)
{
  class MenuItem *mi;

  while((mi = First())) {
    // The destructor removes the item from
    // this list.
    delete mi;
  }
  delete Backsave;
}
///

/// MenuSuperItem::HideMenu
// Close a submenu again.
void MenuSuperItem::HideMenu(class BufferPort *port)
{
  // Check whether we are closed already. If so,
  // perform nothing.
  if (Backsave) {
    // Check whether we have an active submenu. If
    // so, we need to hide it first (since we
    // opened it last).
    if (ActiveItem && ActiveItem->IsSuperMenu()) {
      ((class MenuSuperItem *)ActiveItem)->HideMenu(port);
    }
    // Now restore the graphics under the menu
    port->RestoreRegion(Backsave);
    // The above also disposed the backsave buffer
    Backsave = NULL;
  }
}
///

/// MenuSuperItem::CheckSubItems
// Check all sub items (event-handling) for all items between the
// first, inclusive, and last exclusive. Return true if an item
// got activated.
bool MenuSuperItem::CheckSubItems(struct Event &ev,class BufferPort *port,class MenuItem *first,class MenuItem *last)
{
  class MenuItem *item;
  class MenuSuperItem *super;
  // Perform the checks on all sub-items. Possibly open or close some,
  // or activate or deactivate some. Return a boolean indicator if
  // one of the sub-items is active and hence, this menu should remain open.
  // First step: Check whether the active item is still active. In case it
  // is not, we hide it.
  if (ActiveItem) {
    if (ActiveItem->HitTest(ev,port) == false) {
      // It is no longer active. In case this is a
      // sub-menu, remove it, and then re-render it.
      if (ActiveItem->IsSuperMenu()) {
	super = (class MenuSuperItem *)ActiveItem;
	super->HideMenu(port);
      }
      // Now render it unactive.
      ActiveItem->RenderItem(port,false);
      // Is no longer active, hence remove it.
      ActiveItem = NULL;
    } else {
      // Active item remains active, hence we stay
      // active here.
      return true;
    }
  }
  // Second step: In case we do not have an active item,
  // scan for a new one by performing the hit-test 
  // recursively
  if (ActiveItem == NULL) {
    item = first;
    while(item && item != last) {
      // Perform the hit-test recursively on the given entry.
      if (item->HitTest(ev,port)) {
	// Ok, this item is active and hence this menu is.
	ActiveItem = item;
	// Now check whether it just became active or it just
	// stays active. If it became active, we need to re-render it
	if (item->IsActive() == false) {
	  // Is not active. We need to re-render it and activate it
	  // by this step.
	  item->RenderItem(port,true);
	  // Check whether this is a super-item. If so, we need to
	  // open its sub-menu here
	  if (item->IsSuperMenu()) {
	    super = (class MenuSuperItem *)item;
	    super->LayoutMenu(port->WidthOf(),port->HeightOf());
	    super->ShowMenu(port);
	  }
	}
	return true;
      }
      item = item->NextOf();
    }
  }
  return false;
}
///

/// MenuSuperItem::HitTest
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving.
bool MenuSuperItem::HitTest(struct Event &ev,class BufferPort *port)
{
  // Check whether the mouse is clicked into or moved into
  // the active region of this item.
  switch(ev.Type) {
  case Event::Mouse:
    // Check whether we are currently picked. If so, then
    // we could be here because one of our sub-items has been
    // picked.
    if (IsPicked) {
      // We are. Hence, check the list of sub-items
      if (CheckSubItems(ev,port)) {
	// Subitems active, hence we are active.
	return true;
      } else {
	// Otherwise, we are picked, but none of our sub-items
	// is. Test whether the mouse button is at least within
	// the container of this menu. If so, stay active.
	if (ev.X >= AnchorX && ev.X < AnchorX + SubWidth &&
	    ev.Y >= AnchorY && ev.Y < AnchorY + SubHeight) {
	  return true;
	}
	//
	// We are also active in case we are within this menu
	if (IsWithin(ev)) {
	  return true;
	} else {
	  // Otherwise, hide this menu, and deliver the news.
	  ActiveItem = NULL;
	  HideMenu(port);
	  return false;
	}
      }
    } else if (IsWithin(ev)) {    
      // Check whether we are within the item. If so, then
      // this item must become active (return true then), otherwise,
      // make it inactive.
      return true;
    }
    // We are not picked. Ignore this 
    return false;
  case Event::Click:
    // We could only care about "up" items. 
    // First check whether we hit any of its sub-items.
    if (ActiveItem) {
      if (ActiveItem->HitTest(ev,port)) {
	return true;
      }
    }
    // Otherwise, check for ourselfs. This is a super item and 
    // not selectable, abort the menu in this case
    if (IsWithin(ev) && ev.Button == false) {
      ev.Type = Event::MenuAbort;
      return true;
    }
    return false;
  default:
    return false;
  }
}
///
