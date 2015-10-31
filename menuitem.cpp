/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuitem.cpp,v 1.12 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Definition of the base class for menu items, the class
 ** that defines entries in the quick menu.
 **********************************************************************************/

/// Includes
#include "new.hpp"
#include "string.hpp"
#include "menuitem.hpp"
#include "menusuperitem.hpp"
#include "stdio.hpp"
///

/// MenuItem::MenuItem
// Build up a menu item. Provide the name that gets
// copied, and fill in some defaults.
MenuItem::MenuItem(class MenuSuperItem *parent,const char *text)
  : Parent(parent),
    MenuText((text)?(new char[strlen(text) + 1]):(NULL)), 
    HasSubItems(false), IsToggleAble(false), IsSelectAble(true), IsPicked(false),
    UserPtr(NULL)
{
  if (text)
    strcpy(MenuText,text);
  if (Parent) 
    Parent->AddTail(this);
}
///

/// MenuItem::~MenuItem
// Dispose a menu item. This only disposes its
// body text here.
MenuItem::~MenuItem(void)
{
  delete[] MenuText;
  if (Parent)
    Remove();
}
///

/// MenuItem::FillBackground
// Clear the background with a default color,
// ready to render the text upon it.
void MenuItem::FillBackground(class RenderPort *port,bool picked) const
{
  port->SetPen(picked?0x00:0x08);
  port->FillRectangle(LeftEdge,TopEdge,LeftEdge + Width - 1,TopEdge + Height - 1);
}
///

/// MenuItem::WidthOf
// Compute the width of the menu item. This is used to
// layout the menu before we render it.
LONG MenuItem::WidthOf(void) const
{
  LONG w;
  // The default width is just given by the size of the
  // text.
  w  = LONG(strlen(MenuText) << 3);
  // More room is required for sub item and boolean indicators.
  w += 16;

  return w;
}
///

/// MenuItem::HeightOf
// Compute the height of the menu item in pixels. Also
// used for layout.
LONG MenuItem::HeightOf(void) const
{
  // Menus are eight pixel high with the standard font.
  return 8;
}
///

/// MenuItem::RenderItem
// Render the menu item at the given coordinates into
// the render port, the "selected" boolean is set in case
// the mouse is currently on top of the item
// and the item needs to be drawn in inverse video.
void MenuItem::RenderItem(class RenderPort *port,bool picked)
{
  LONG w  = Width;
  LONG le = LeftEdge;
  //
  // Update the "picked" flag.
  IsPicked = picked;
  //
  // Fill the background
  FillBackground(port,picked);
  //
  // Render an arrow in case we have subitems.
  if (HasSubItems && w >= 8) {
    char arrow[2];
    arrow[0] = 0x7f; // ATASCII for a right arrow
    arrow[1] = 0x00;
    port->TextClipLefty(le,TopEdge,8,Height,arrow,picked?0x0f:0x00);
  }
  //
  // Now advance the cursor over the special marker.
  // We cannot print the toggle marker since we don't
  // know its state
  w  -= 8;
  le += 8;
  //
  if (w > 8) {
    // Write now the remaining text.
    port->TextClipLefty(le,TopEdge,w,Height,MenuText,picked?0x0f:0x00);
  }
}
///

/// MenuItem::PlaceItemAt
// Since the position and dimension of the menu depends on the
// layout performed by the super item, it requires a method
// to define where the menu is placed.
void MenuItem::PlaceItemAt(LONG le,LONG te,LONG w,LONG h)
{
  LeftEdge = le;
  TopEdge  = te;
  Width    = w;
  Height   = h;
}
///

/// MenuItem::IsWithin
// Check whether the given coordinate is within the
// coordinate rectangle of the item. Return true if so.
bool MenuItem::IsWithin(struct Event &ev) const
{
  if (ev.X >= LeftEdge && ev.X < LeftEdge + Width &&
      ev.Y >= TopEdge  && ev.Y < TopEdge  + Height) {
    return true;
  }
  return false;
}
///

/// MenuItem::HitTest
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving.
bool MenuItem::HitTest(struct Event &ev,class BufferPort *)
{
  // This performs only the trivial event test. The item
  // generates a MenuPick in case the right mouse button is
  // released over the item.
  switch(ev.Type) {
  case Event::Mouse:
    // No matter whether this is pick-able or not,
    // we return a true indicator for the mouse within
    // the item. Note that this does, however, not
    // generate any kind of event.
    return IsWithin(ev);
  case Event::Click:
    if (ev.Button == false && IsWithin(ev)) {
      // We cannot really remove this item since this is
      // the matter of the super item.
      if (IsSelectAble) {
	ev.Type = Event::MenuPick;
      } else {
	ev.Type = Event::MenuAbort;
      }
      ev.Menu   = this;
      return true;
    }
    return false;
  default:
    return false;
  }
}
///
