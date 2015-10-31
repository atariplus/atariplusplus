/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuselectionitem.hpp,v 1.7 2015/05/21 18:52:41 thor Exp $
 **
 ** 
 ** In this module: Definition of an item that offers several selections 
 **********************************************************************************/

#ifndef MENUSELECTIONITEM_HPP
#define MENUSELECTIONITEM_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "argparser.hpp"
#include "menuvertitem.hpp"
#include "menuboolitem.hpp"
///

/// MenuSelectionItem
// This kind of item offers several mutual
// exclusive subitems to pick from.
class MenuSelectionItem : public MenuVertItem {
  //
  // A selection entry is just a boolean item
  // with an additional entry containing the value
  // this entry describes.
  class MenuSelectionEntry : public MenuBoolItem {
    //
    // The value this entry represents within
    // a selection vector.
    LONG Value;
    //
  public:
    MenuSelectionEntry(class MenuSuperItem *parent,const char *text,LONG value);
    virtual ~MenuSelectionEntry(void);
    //
    // Get or set the selection value
    LONG &SelectionValueOf(void)
    {
      return Value;
    }
  };
  //
  // This presents the selection that is currently
  // active.
  LONG ActiveSelection;
  //
public:
  MenuSelectionItem(class MenuSuperItem *parent,const char *text,
		    const struct ArgParser::SelectionVector *selections);
  virtual ~MenuSelectionItem(void);
  //  
  // Feed events into the menu, and perform a hit-test. Modify the event
  // possibly. We also need to give a buffer port here since
  // the menu must render into the screen while the mouse
  // is moving.
  virtual bool HitTest(struct Event &ev,class BufferPort *port);
  //
  // Get and set the active selection
  LONG GetState(void) const
  {
    return ActiveSelection;
  }
  //
  void SetState(LONG value);
};
///

///
#endif
