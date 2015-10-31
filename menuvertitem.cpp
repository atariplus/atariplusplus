/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuvertitem.cpp,v 1.10 2015/05/21 18:52:41 thor Exp $
 **
 **
 ** In this module: Implementation of a superitem class that keeps
 ** its sub-items vertically aligned.
 **********************************************************************************/

/// Includes
#include "new.hpp"
#include "menusuperitem.hpp"
#include "menuvertitem.hpp"
///

/// Statics
#ifndef HAS_MEMBER_INIT
const LONG MenuVertItem::ScrollTime = 30;
#endif
///

/// MenuVertItem::MenuVertItem
// Build up a super item. The list
// gets initialized by itself.
MenuVertItem::MenuVertItem(class MenuSuperItem *parent,const char *text)
  : MenuSuperItem(parent,text), 
    UpArrowBar(NULL), DownArrowBar(NULL),
    FirstItem(NULL), LastItem(NULL)
{
}
///

/// MenuVertItem::~MenuVertItem
// Dispose a vertitem. Most work is
// done in the super item.
MenuVertItem::~MenuVertItem(void)
{
  delete UpArrowBar;
  delete DownArrowBar;
}
///

/// MenuVertItem::ArrowItem::ArrowItem
// Private sub class for dummy items that render an
// arrow up or error down line if only a part of the
// items is visible.
MenuVertItem::ArrowItem::ArrowItem(class MenuSuperItem *parent,char arrow)
  : MenuItem(parent,NULL), Arrow(arrow)
{
}
///

/// MenuVertItem::ArrowItem::~ArrowItem
// Dispose the arrow item. Not much to do,
// all this happens in the menu item.
MenuVertItem::ArrowItem::~ArrowItem(void)
{
}
///

/// MenuVertItem::ArrowItem::WidthOf
// Return the desired width of the arrow item.
LONG MenuVertItem::ArrowItem::WidthOf(void) const
{
  return 1<<3;
}
///

/// MenuVertItem::ArrowItem::RenderItem
// Render the menu item.
void MenuVertItem::ArrowItem::RenderItem(class RenderPort *port,bool picked)
{
  char arrowbuffer[81];
  //
  // Fill with the background color
  FillBackground(port,picked);
  //
  // Setup a line of arrows, clip at the right position.
  // Select either the ATASCII up or down arrow chaa
  memset(arrowbuffer,Arrow,80);
  if (Width > (80<<3)) {
    // How that?
    arrowbuffer[80]     = 0x00;
  } else {
    arrowbuffer[Width >> 3] = 0x00;
  }
  //
  // Render centered into the item.
  port->TextClip(LeftEdge,TopEdge,Width,8,arrowbuffer,picked?0x0f:0x00);
}
///

/// MenuVertItem::LayoutMenu
// Layout this item, i.e. compute the position of all
// its sub-items on the screen givne the
// anchor position of this item. This does not yet draw
// the item list, though.
void MenuVertItem::LayoutMenu(LONG w,LONG h)
{    
  class MenuItem *sub;
  LONG anchorx,anchory;
  LONG maxwidth,maxheight;
  LONG subx,suby;
  bool isatroot = false;
  //
  // Check whether this is a descendend of root. If so,
  // layout is a bit different and we align it below and
  // not right of root.
  if (Parent->ParentOf() == NULL) {
    isatroot = true;
  }
  // If the parent exists, we perform a vertical layout of all
  // subitems anchored below this item. Anchor position
  // is the position of this item.
  anchorx   = LeftEdge;
  anchory   = TopEdge;
  maxwidth  = 0;
  maxheight = 0;
  sub       = First();
  while(sub) {
    LONG w;
    // get the width of the sub-item.
    // check whether this is a new maximum
    w          = sub->WidthOf(); 
    if (w > maxwidth)
      maxwidth = w;
    maxheight += sub->HeightOf();
    sub        = sub->NextOf();
  }
  //
  // Add the size of the border and the sup-item
  // separator.
  maxwidth  += 2 + 16;
  maxheight += 2;
  //
  // Check whether the result is larger than the screen
  // width. If so, we must limit this to the screen
  // size.
  if (maxwidth > w) {
    maxwidth = w;
  }
  // The size of the subitems when we render it
  SubWidth     = maxwidth;
  SubItemLimit = maxwidth - 2 - 16;
  //
  // Check whether we can align it to the right
  // of the current item.
  subx  = anchorx;
  if (isatroot == false) {
    subx += Width >> 1;
  }
  if (maxwidth + subx <= w) {
    // Yes, it is. Anchor it at the right of
    // this menu.
    anchorx = subx;
  } else {
    // Try to move it to the left of this item.
    subx    = anchorx - (maxwidth >> 1);
    if (subx >= 0 && subx < w) {
      // Yes, keep the submenu here.
      anchorx = subx;
    } else {
      subx    = anchorx - maxwidth;
      if (subx >= 0 && subx < w) {
	// Yes, keep the submenu here.
	anchorx = subx;
      } else {
	// Did not work either. Move to the left edge.
	anchorx = 0;
      }
    }
  }
  // Check whether we can only show a sub-set of the
  // sub-items here.
  if (maxheight > h) {
    MustClip  = true;
    maxheight = h;
  } else {
    MustClip  = false;
  }
  SubHeight   = maxheight;
  // Default position is at the anchor position.
  suby = anchory;
  if (isatroot) {
    suby += Height;
  }
  if (suby + maxheight <= h) {
    // Yes, fits below this menu. Render it here.
    anchory   = suby;
  } else {
    // Maybe it helps to place it on top of the screen.
    suby = anchory + HeightOf() - maxheight;
    if (suby >= 0 && suby < h) {
      anchory = suby;
    } else {
      // No other chance to move it on top.
      suby    = 0;
    }
  }
  // Now keep this as the anchor position of the
  // sub items.
  AnchorX    = anchorx,AnchorY = anchory;
  FirstItem  = First();
  ActiveItem = NULL;
}
///

/// MenuVertItem::ShowMenu
// Open a submenu by rendering it into the buffer port
void MenuVertItem::ShowMenu(class BufferPort *port)
{
  class MenuItem *item;
  LONG x,y,w,h,total;
  // Check whether we have a saveback buffer
  // If not, make a saveback of the buffer
  // we need to render in
  if (Backsave == NULL) {
    Backsave = port->SaveRegion(AnchorX,AnchorY,SubWidth,SubHeight);
  }
  //
  // Fill the background with grey
  port->SetPen(0x08);
  port->FillRectangle(AnchorX,AnchorY,AnchorX + SubWidth - 1,AnchorY + SubHeight - 1);
  // Render a 3D frame around this region
  port->Draw3DFrame(AnchorX,AnchorY,SubWidth,SubHeight,false);
  x     = AnchorX + 1;
  y     = AnchorY + 1;
  w     = SubWidth - 2;
  total = SubHeight - 2;
  // Check whether the first item to render is the first item on the list.
  if (FirstItem != First()) {
    // No, it isn't. Render a "go up" at the first position by
    // using a row of arrows. For that, we might
    // have to create the UpArrowBar here.
    if (UpArrowBar == NULL)
      UpArrowBar = new class ArrowItem(this,0x1c); // ATASCII code for arrow pointing up
    UpArrowBar->PlaceItemAt(x,y,w,8);
    UpArrowBar->RenderItem(port,false);
    y     += 8;
    total -= 8;
  } else {
    // We don't need the up arrow gadget, remove it.
    delete UpArrowBar;
    UpArrowBar = NULL;
  }
  //
  // Now render the rest of this
  item = FirstItem;
  while(item) {
    // Check whether there is enough room for this one and possibly the next
    // one in case we must render an arrow-down row.
    h = item->HeightOf();
    if (MustClip) {
      if (total < h)
	break; // Not enough room to render the next one
      if (item->NextOf()) {
	// We have a next item. Would there be enough room to render the next one?
	// If not, abort and render a row of arrows instead.
	if (total < h + item->NextOf()->HeightOf())
	  break;
      }
    }
    // Ok, here we know that there is enough room to
    // render this item. 
    item->PlaceItemAt(x,y,w,h);
    item->RenderItem(port,false);
    // Advance the position and go to the next
    // item now.
    item   = item->NextOf();
    y     += h;
    total -= h;
  }
  //
  // Remember the first item no longer to render here.
  LastItem = item;
  // If the last rendered is not the last one,
  // render a row of arrows pointing down, indicating
  // that this is not the last item.
  if (item) {
    // We possibly need to build an arrow bar pointing down
    // to indicate that this is not the end of the list.
    if (DownArrowBar == NULL)
      DownArrowBar = new class ArrowItem(this,0x1d); // 0x1d is the ATASCII code for the arrow pointing down
    DownArrowBar->PlaceItemAt(x,y,w,8);
    DownArrowBar->RenderItem(port,false);
  } else {
    // We don't need the down arrow bar any longer, dispose it.
    delete DownArrowBar;
    DownArrowBar = NULL;
  }
}
///

/// MenuVertItem::CheckScrolling
// Check on the arrow bar passed in whether we shall
// scroll the menu. If so, then the first item is set
// to the passed in new first item.
bool MenuVertItem::CheckScrolling(struct Event &ev,class BufferPort *port,
				  class MenuVertItem::ArrowItem *arrow,class MenuItem *nextfirst)
{
  // If not, check whether we have scroll bars. If these are hit, a menu move is generated
  // and we scroll up or down.
  if (arrow) {
    // Ok, we have an arrow bar. Check whether we are in it.
    if (arrow->IsWithin(ev)) {
      // Yes, check whether we have been picked before. If so, count down
      // the timer and scroll on zero. Otherwise, make it active and
      // reset the timer.
      if (arrow->IsActive()) {
	// Was active before. Check whether we should scroll up
	// on a timer wrap-around.
	if (--ScrollTimer == 0) {
	  if (nextfirst) {
	    // There is a previous item. Scroll up and update the menu.
	    FirstItem = nextfirst;
	    ShowMenu(port);
	  }
	  // Reset the timer now.
	  ScrollTimer = ScrollTime;
	}
      } else {
	// Render it active and initialize the scroll time
	arrow->RenderItem(port,true);
	ScrollTimer = ScrollTime;
	ActiveItem  = arrow;
      }
      // The active item will be made non-active automatically#
      // by the super menu, no need to do this here.
      return true;
    }
  }
  return false;
}
///

/// MenuVertItem::CheckSubItems
// Perform event handling on all subitems.
bool MenuVertItem::CheckSubItems(struct Event &ev,class BufferPort *port)
{
  // First let's see whether the super item finds anything active within
  // the range of displayed items.
  if (MenuSuperItem::CheckSubItems(ev,port,FirstItem,LastItem))
    return true;
  // If not, check whether we have scroll bars. If these are hit, a menu move is generated
  // and we scroll up or down.
  if (CheckScrolling(ev,port,UpArrowBar,FirstItem->PrevOf()))
    return true;
  // Now the same for the arrow bar pointing down.
  if (CheckScrolling(ev,port,DownArrowBar,FirstItem->NextOf()))
    return true;
  // That's it.
  return false;
}
///
