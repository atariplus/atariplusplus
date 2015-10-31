/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuselectionitem.cpp,v 1.7 2015/05/21 18:52:41 thor Exp $
 **
 ** 
 ** In this module: Definition of an item that offers several selections 
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "menuselectionitem.hpp"
///

/// MenuSelectionItem::MenuSelectionEntry::MenuSelectionEntry
// Construct a an entry within the items the user
// can select from.
MenuSelectionItem::MenuSelectionEntry::MenuSelectionEntry(class MenuSuperItem *parent,const char *text,LONG value)
  : MenuBoolItem(parent,text), Value(value)
{
}
///

/// MenuSelectionItem::MenuSelectionEntry::~MenuSelectionEntry
MenuSelectionItem::MenuSelectionEntry::~MenuSelectionEntry(void)
{
  // nothing else to do here.
}
///

/// MenuSelectionItem::MenuSelectionItem
// Build up a menu item that offers several subitems to select from
MenuSelectionItem::MenuSelectionItem(class MenuSuperItem *parent,const char *text,
				     const struct ArgParser::SelectionVector *selections)
  : MenuVertItem(parent,text)
{
  // Now build up the childs. Note that in case we throw we need not to clean this
  // up as the super item counts as constructed then here already, and it
  // is already responsible for disposing the childs.
  while(selections->Name) {
    new class MenuSelectionEntry(this,selections->Name,selections->Value);
    selections++;
  }
}
///

/// MenuSelectionItem::~MenuSelectionItem
// Dispose this item. This job is done by the super item already.
MenuSelectionItem::~MenuSelectionItem(void)
{
}
///

/// MenuSelectionItem::HitTest
// Feed events into the menu, and perform a hit-test. Modify the event
// possibly. We also need to give a buffer port here since
// the menu must render into the screen while the mouse
// is moving.
bool MenuSelectionItem::HitTest(struct Event &ev,class BufferPort *port)
{
  // Perform a hit-test on the vertical item. In case we receive a menu pick,
  // identify the sub-item that got picked and set this as selection.
  if (MenuVertItem::HitTest(ev,port)) {
    if (ev.Type == Event::MenuPick) {
      class MenuSelectionEntry *msi;
      LONG setting;
      // A sub-item got picked. Find out which.
      msi     = (class MenuSelectionEntry *)ev.Menu;
      setting = msi->SelectionValueOf();
      // Install this as active state.
      SetState(setting);
      //
      // Now tell the super that we, and not the sub-item, generated this
      // event such that the right instance gets asked about the picked
      // value.
      ev.Menu = this;
    }
    return true;
  }
  return false;
}
///

/// MenuSelectionItem::SetState
// Define the active selection by its value.
void MenuSelectionItem::SetState(LONG value)
{
  class MenuSelectionEntry *entry;
  //
  // Iterate over the list of sub items and set or
  // reset the entries.
  entry = (class MenuSelectionEntry *)First();
  while(entry) {
    if (entry->SelectionValueOf() == value) {
      entry->SetState(true); // activate if it fits
    } else {
      entry->SetState(false); // deactivate otherwise
    }
    entry = (class MenuSelectionEntry *)entry->NextOf();
  }
  // Now keep it globally as well.
  ActiveSelection = value;
}
///

