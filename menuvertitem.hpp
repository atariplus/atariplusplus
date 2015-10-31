/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuvertitem.hpp,v 1.8 2015/05/21 18:52:41 thor Exp $
 **
 ** 
 ** In this module: Implementation of a superitem class that keeps
 ** its sub-items vertically aligned.
 **********************************************************************************/

#ifndef MENUVERTITEM_HPP
#define MENUVERTITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "menusuperitem.hpp"
///

/// Class MenuVertItem
// A specialized implementation of the super item that
// keeps its sub-items vertically aligned. This class
// gets used to represent the super-items within all
// items except the root item.
class MenuVertItem : public MenuSuperItem {
  // Private sub class for dummy items that render an
  // arrow up or error down line if only a part of the
  // items is visible.
  class ArrowItem : public MenuItem { 
    // The character that gets used for the arrow.
    char Arrow;
  public:
    ArrowItem(class MenuSuperItem *parent,char arrow);
    virtual ~ArrowItem(void);
    //
    // Width: This differs from the default
    virtual LONG WidthOf(void) const;
    //
    // Render the menu item.
    virtual void RenderItem(class RenderPort *port,bool picked);
    //
  };
  //
  // Keep two pointers to arrow bars here.
  class ArrowItem             *UpArrowBar;
  class ArrowItem             *DownArrowBar; 
  //
  // The first and last item we need to render.
  class MenuItem              *FirstItem;
  class MenuItem              *LastItem;
  //  
  // Timer-count down for the scrolling
  LONG                         ScrollTimer;
  static const LONG            ScrollTime INIT(30);
  //
  // The following bool is set if we must perform clipping
  // and cannot show all items.
  bool                         MustClip;
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
  // Check on the arrow bar passed in whether we shall
  // scroll the menu. If so, then the first item is set
  // to the passed in new first item.
  bool CheckScrolling(struct Event &ev,class BufferPort *port,
		      class MenuVertItem::ArrowItem *arrow,class MenuItem *nextfirst);
public:
  MenuVertItem(class MenuSuperItem *parent,const char *text);
  virtual ~MenuVertItem(void);
};
///

///
#endif
