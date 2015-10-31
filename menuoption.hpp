/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuoption.hpp,v 1.6 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Definition of an option within the graphical fronent, i.e.
 ** something that is selectable/adjustable under a topic
 **********************************************************************************/

#ifndef MENU_OPTION_HPP
#define MENU_OPTION_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "stdio.hpp"
///

/// Forwards
class Gadget;
class MenuItem;
class RenderPort;
///

/// Class MenuOption
// An option is defined by one of the classes the configurable is able
// to generate.
class Option : public Node<class Option> {
protected:
  //
  // Name of this option. This is also defined by the various configurables.
  char                     *Name;
  // A helper text, also provided by the configurable.
  char                     *Help;
  //
  // If we have: The gadget that the user plays with to change the
  // setting of this option.
  class Gadget             *Gadget;
  //
  // If we have: The menu item that represents this option.
  class MenuItem           *MenuItem;
  //
public:
  // The type of this configurable. This further defines 
  // the topic.
  enum OptionTypes {
    Option_Boolean   = 1,   // a bool option, contains an on/off value
    Option_Long      = 2,   // a range option, containing min/max
    Option_String    = 3,   // a string, to be defined by the user
    Option_File      = 4,   // a file name, resp. a path
    Option_Selection = 5    // a radio button list
  }                         OptionType;
  //
  Option(const char *name,const char *help,const OptionTypes type);
  virtual ~Option(void);
  //
  // Check whether the name matches the name of the option passed in.
  bool Matches(const char *name);
  //
  // Create a suitable gadget for this option
  virtual class Gadget *BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,
					  LONG le,LONG te,LONG width);
  //
  // Create a suitable menu item for this option (if possible)
  // Return NULL if it is not possible
  virtual class MenuItem *BuildMenuItem(class MenuSuperItem *parent);
  //
  // Read the gadget created for this option and return a flag whether the
  // setting has changed and the options have to be re-read by the argument
  // parser.
  virtual bool ParseGadget(void) = 0;
  //
  // The same again, but for parsing off the menu item.
  virtual bool ParseMenu(void);
  //
  // Re-install the default to fix a configuration error.
  virtual void InstallDefault(void) = 0;
  //
  // Save the state of the option to a configuration file.
  virtual void SaveOption(FILE *to) = 0;
};
///

///
#endif

