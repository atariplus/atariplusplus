/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuseparatoritem.cpp,v 1.8 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of a separator bar in the menu
 **********************************************************************************/

/// Includes
#include "types.h"
#include "renderport.hpp"
#include "menuseparatoritem.hpp"
///

/// MenuSeparatorItem::MenuSeparatorItem
MenuSeparatorItem::MenuSeparatorItem(class MenuSuperItem *parent)
  : MenuItem(parent,NULL)
{
  IsSelectAble = false; // clearly not
}
///

/// MenuSeparatorItem::~MenuSeparatorItem
MenuSeparatorItem::~MenuSeparatorItem(void)
{
}
///

/// MenuSeparatorItem::WidthOf
// Return the width of the separator item.
// Since the separator item expands to its final
// length, we return the minimal width here.
LONG MenuSeparatorItem::WidthOf(void) const
{
  return 4;
}
///

/// MenuSeparatorItem::HeightOf
// Return the height of the separator item
// in pixels
LONG MenuSeparatorItem::HeightOf(void) const
{
  return 4;
}
///

/// MenuSeparatorItem::RenderItem
// Render the menu item at the given coordinates into
// the render port, the "selected" boolean is set in case
// the mouse is currently on top of the item
// and the item needs to be drawn in inverse video.
void MenuSeparatorItem::RenderItem(class RenderPort *port,bool picked)
{
  // Render two lines, dark and light
  // below each other.
  IsPicked = picked; // Consistency, the menu state machine requires this
  // but don't render it any different.
  FillBackground(port,false); // never rendered as picked
  port->Position(LeftEdge,TopEdge+1);
  port->SetPen(0x04);
  port->DrawHorizontal(Width);
  port->Position(LeftEdge,TopEdge+2);
  port->SetPen(0x0c);
  port->DrawHorizontal(Width);
}
///
