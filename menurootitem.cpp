/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menurootitem.cpp,v 1.13 2015/05/21 18:52:40 thor Exp $
 **
 ** 
 ** In this module: Definition of the menu item class that keeps subitems,
 ** here specifically in the root menu.
 **********************************************************************************/

/// Includes
#include "new.hpp"
#include "menurootitem.hpp"
#include "titlemenu.hpp"
#include "optioncollector.hpp"
///

/// MenuRootItem::MenuRootItem
// Build up a root item. The list
// gets initialized by itself. Note that
// there is no parent here.
MenuRootItem::MenuRootItem(void)
  : MenuSuperItem(NULL,NULL)
{
}
///

/// MenuRootItem::~MenuRootItem
// Dispose a rootitem. There's not much
// to do since the super item does
// all this.
MenuRootItem::~MenuRootItem(void)
{
}
///

/// MenuRootItem::LayoutMenu
// Layout this item, i.e. compute the position of all
// its sub-items on the screen givne the
// anchor position of this item. This does not yet draw
// the item list, though.
void MenuRootItem::LayoutMenu(LONG w,LONG)
{    
  class MenuItem *sub;
  LONG totalw;
  LONG items;
  // Align the subitems horizontally here.
  // Add up the width of all the sub-items
  // and check whether we have enough for all of them.
  totalw = 0,items = 0,sub = First();
  while(sub) {
    totalw += sub->WidthOf();
    items++;
    sub     = sub->NextOf();
  }
  // Check whether we have enough room on the screen
  if (totalw > w) {
    // No. Oops, check how much room we have per sub-item
    // at most.
    SubItemLimit = w / items;
  } else {
    // Otherwise, no limit.
    SubItemLimit = w;
  }
  // Anchor of subitems at zero,zero.
  AnchorX       = AnchorY = 0;
  SubWidth      = w;  // Region covered by all this and its subitems.
  SubHeight     = HeightOf() - 1;
  // Room covered by the menu itself
  Width         = SubWidth;
  Height        = 1;
  LeftEdge      = 0;
  TopEdge       = SubHeight;
  ActiveItem    = NULL;
}
///

/// MenuRootItem::ShowMenu
// Open a submenu by rendering it into the buffer port
void MenuRootItem::ShowMenu(class BufferPort *port)
{
  class MenuItem *item;
  LONG x,y,w,h;
  // Check whether we have a saveback buffer
  // If not, make a saveback of the buffer
  // we need to render in
  if (Backsave == NULL) {
    Backsave = port->SaveRegion(AnchorX,AnchorY,SubWidth,SubHeight + 1);
  }
  //
  // Fill the background with grey
  port->SetPen(0x08);
  port->FillRectangle(AnchorX,AnchorY    ,AnchorX + SubWidth - 1,AnchorY + SubHeight - 1);
  port->SetPen(0x02);
  port->FillRectangle(AnchorX,AnchorY + 8,AnchorX + SubWidth - 1,AnchorY + SubHeight);
  x    = AnchorX,y = AnchorY;
  // Render all items, one after another.
  item = First();
  while(item) {
    // Get the width and height of the item.
    w = item->WidthOf();
    if (w > SubItemLimit)
      w = SubItemLimit;
    h    = item->HeightOf();
    item->PlaceItemAt(x,y,w,h);
    item->RenderItem(port,false);     
    item = item->NextOf();
    x   += w;
  }
}
///

/// MenuRootItem::CheckSubItems
// Perform event handling on all subitems.
bool MenuRootItem::CheckSubItems(struct Event &ev,class BufferPort *port)
{
  // Just call the super method here on all sub-items.
  return MenuSuperItem::CheckSubItems(ev,port,First(),NULL);
}
///

/// MenuRootItem::WidthOf
// Return the desired width of this item:
// This is the full width.
LONG MenuRootItem::WidthOf(void) const
{
  return Width;
}
///

/// MenuRootItem::HeightOf
// Return the height of the item.
LONG MenuRootItem::HeightOf(void) const
{
  return 8+1;
}
///

/// MenuRootItem::DisplayMenu
// Display the root menu on the screen: This performs the layout
// process and then renders the menu.
void MenuRootItem::DisplayMenu(class BufferPort *port)
{
  // First compute dimensions
  LayoutMenu(port->WidthOf(),port->HeightOf());
  // Then render into it.
  ShowMenu(port);
}
///

/// MenuRootItem::HitTest
// Event handling. This converts MenuAbort to high-level events.
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving.
bool MenuRootItem::HitTest(struct Event &ev,class BufferPort *port)
{  
  // Check whether the mouse is clicked into or moved into
  // the active region of this item.
  switch(ev.Type) {
  case Event::Mouse:
    // Always check the sub items for the hit-test. Otherwise, we are
    // always active until the user releases the mouse. 
    if (CheckSubItems(ev,port)) {
      // Subitems active, hence we are active.
      return true;
    }
    // Otherwise, we always count as active.
    return true;
  case Event::Click:
    // We could only care about "up" items. Everything else does not
    // matter. In case we receive an "up" item, deliver a TA_Exit
    // high-level event.
    if (ActiveItem) {
      if (ActiveItem->HitTest(ev,port)) {
	// A sub-item got active. Deliver what it has to
	// say.
	return true;
      }
      // Otherwise, it is a simple "exit" without adjusting the prefs.
      ev.Type      = Event::MenuAbort;
      return true;
    } else {
      if (ev.Button == false) {
	// Ok, this should quit the menu.
	ev.Type      = Event::MenuAbort;
	return true;
      }
    }
    return false;
  default:
    // Shut up the compiler
    return false;
  }
}
///
