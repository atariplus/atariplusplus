/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuitem.hpp,v 1.10 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Definition of the base class for menu items, the class
 ** that defines entries in the quick menu.
 **********************************************************************************/

#ifndef MENUITEM_HPP
#define MENUITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "event.hpp"
///

/// Forwards
class MenuSuperItem;
class RenderPort;
class BufferedPort;
struct Event;
///

/// Class MenuItem
// This class is the base class for all menu items
// and represents the building stone of the 
// quick menu activated by the right mouse button.
class MenuItem : public Node<class MenuItem> {
protected:
  //
  // The rectangle that is covered by this item.
  // This is the size assigned to this item
  // by the super item.
  LONG                 LeftEdge,TopEdge;
  LONG                 Width,Height;
  //
  // Pointer to the super item we are part of or
  // NULL for a root item.
  class MenuSuperItem *Parent;
  //
  // The text making up the menu entry.
  char                *MenuText;
  //
  // If the following boolean is set, then this
  // menu item has subitems.
  bool                 HasSubItems;
  //
  // If this item is set, then the menu item is
  // a boolean item that can be toggled on or
  // of.
  bool                 IsToggleAble;
  //
  // If this bool is set, then the menu can be 
  // picked.
  bool                 IsSelectAble;
  //
  // The following bool gets set if this menu
  // item is currently picked.
  bool                 IsPicked;
  //
  // A pointer to whatever. This is a service
  // to the instance using the menu.
  void                *UserPtr;
  //
  // Clear the background with a default color,
  // ready to render the text upon it.
  void FillBackground(class RenderPort *port,bool picked) const;
  //
public:
  MenuItem(class MenuSuperItem *parent,const char *text);
  virtual ~MenuItem(void);
  //  
  // Get the parent of this item.
  class MenuSuperItem *ParentOf(void) const
  {
    return Parent;
  }
  //
  // The following two methods return the dimensions this
  // menu item wants to have, not the dimensions it will
  // get assigned due to layout restrictions.
  //
  // Compute the width of the menu item. This is used to
  // layout the menu before we render it.
  virtual LONG WidthOf(void) const;
  //
  // Compute the height of the menu item in pixels. Also
  // used for layout.
  virtual LONG HeightOf(void) const;
  //
  // Render the menu item at the given coordinates into
  // the render port, the "selected" boolean is set in case
  // the mouse is currently on top of the item
  // and the item needs to be drawn in inverse video.
  virtual void RenderItem(class RenderPort *port,bool picked);
  //
  // Return whether this menu item is a super menu
  bool IsSuperMenu(void) const
  {
    return HasSubItems;
  }
  // Return whether this item can be selected
  bool IsSelectable(void) const
  {
    return IsSelectAble;
  }
  // Return whether this item is currently picked, i.e.
  // under the pointer.
  bool IsActive(void) const
  {
    return IsPicked;
  }
  //
  // Since the position and dimension of the menu depends on the
  // layout performed by the super item, it requires a method
  // to define where the menu is placed.
  void PlaceItemAt(LONG le,LONG te,LONG w,LONG h);
  //
  // Check whether the given coordinate is within the
  // coordinate rectangle of the item. Return true if so.
  // This check is not sufficient to check for activiation
  // since an item also is active as LONG as the mouse is over
  // one of its sub-items.
  bool IsWithin(struct Event &ev) const;
  //
  // Feed events into the menu, and perform a hit-test. Modify the event
  // possibly. We also need to give a buffer port here since
  // the menu must render into the screen while the mouse
  // is moving.
  virtual bool HitTest(struct Event &ev,class BufferPort *port);
  //
  // Return or set the user pointer
  void *&UserPointerOf(void)
  {
    return UserPtr;
  }
};
///

///
#endif
