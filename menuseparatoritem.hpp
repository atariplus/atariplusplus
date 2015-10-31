/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuseparatoritem.hpp,v 1.6 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of a separator bar in the menu
 **********************************************************************************/

#ifndef MENUSEPARATORITEM_HPP
#define MENUSEPARATORITEM_HPP

/// Includes
#include "types.h"
#include "menuitem.hpp"
///

/// Class MenuSeparatorItem
// This class describes a separator bar within
// the menu. It performs no function.
class MenuSeparatorItem : public MenuItem {
  //
public:
  MenuSeparatorItem(class MenuSuperItem *parent);
  virtual ~MenuSeparatorItem(void);
  //  
  // Compute the width of the menu item. This is used to
  // layout the menu before we render it.
  virtual LONG WidthOf(void) const;
  //
  // Compute the height of the menu item in pixels. Also
  // used for layout.
  virtual LONG HeightOf(void) const;
  //
  // Render the separator item into a render port,
  // clip it to the given dimensions.
  virtual void RenderItem(class RenderPort *port,bool picked);
};
///

///
#endif
