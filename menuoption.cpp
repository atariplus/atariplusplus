/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menuoption.cpp,v 1.27 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Definition of an option within the graphical fronent, i.e.
 ** something that is selectable/adjustable under a topic
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "menuoption.hpp"
#include "optioncollector.hpp"
#include "argparser.hpp"
#include "gadget.hpp"
#include "separatorgadget.hpp"
#include "textgadget.hpp"
#include "booleangadget.hpp"
#include "rangegadget.hpp"
#include "stringgadget.hpp"
#include "radiogadget.hpp"
#include "filegadget.hpp"
#include "menuitem.hpp"
#include "menuboolitem.hpp"
#include "menuselectionitem.hpp"
#include "menufileitem.hpp"
#include "filerequester.hpp"
#include "string.hpp"
#include "stdio.hpp"
///

/// Option::Option
// Constuctor of the Option administration class
Option::Option(const char *name,const char *help,
				const Option::OptionTypes type)
  : Name(new char[strlen(name)+1]),
    Help(new char[strlen(help)+1]), 
    OptionType(type)
{
  // We have to copy this data over as the class that owns these strings might
  // go away as part of our configuration process, or may have setup these strings
  // as temporaries
  strcpy(Name,name);
  strcpy(Help,help);
}
///

/// Option::~Option
// There is not much to do here.
Option::~Option(void)
{
  delete[] Name;
  delete[] Help;
}
///

/// Option::Matches
// Check whether the indicated name matches the option. This is just a
// string compare.
bool Option::Matches(const char *name)
{
  // Now, this is easy: If the name string matches, return that we match...
  if (!strcasecmp(name,Name))
    return true;
  return false;
}
///

/// Option::BuildOptionGadget
// Build a suitable gadget representing this option
class Gadget *Option::
  BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,LONG le,LONG te,LONG width)
{
  // Build a separator to separate this gadget from the previous.
  new class SeparatorGadget(glist,RPort,le,te,width,4);
  te += 4;
  // Then build a simple text gadget for the name of this option
  return new class TextGadget(glist,RPort,le,te,width,12,Name);
}
///

/// Option::BuildMenuItem
// Build a suitable menu item representing this option
// The default is to build none
class MenuItem *Option::BuildMenuItem(class MenuSuperItem *)
{
  return NULL;
}
///

/// Option::ParseMenu
// Parse off the settings from the menu
// we beLONG to and install them into the
// option. The default is to do nothing as
// most options don't carry menus.
bool Option::ParseMenu(void)
{
  return false;
}
///

/// OptionCollector::BooleanOption::BooleanOption
OptionCollector::BooleanOption::BooleanOption(const char *name,const char *help,
					      bool def)
  : Option(name,help,Option_Boolean), Setting(def), Default(def)
{
}
///

/// OptionCollector::BooleanOption::~BooleanOption
OptionCollector::BooleanOption::~BooleanOption(void)
{
}
///

/// OptionCollector::BooleanOption::Setting
bool OptionCollector::BooleanOption::SettingOf(void)
{
  return Setting;
}
///

/// OptionCollector::BooleanOption::BuildOptionGadget
// Build a suitable gadget representing this option
class Gadget *OptionCollector::BooleanOption::
  BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,LONG le,LONG te,LONG width)
{
  // Build a separator to separate this gadget from the previous.
  new class SeparatorGadget(glist,RPort,le,te,width,4);
  te    += 4;
  Gadget = new class BooleanGadget(glist,RPort,le,te,width,12,Name,Setting);
  return Gadget;
}
///

/// OptionCollector::BooleanOption::BuildMenuItem
// Build a menu item representing the option
class MenuItem *OptionCollector::BooleanOption::BuildMenuItem(class MenuSuperItem *parent)
{
  class MenuBoolItem *item;
  item = new class MenuBoolItem(parent,Name);
  item->SetState(Setting);
  item->UserPointerOf() = this;
  MenuItem = item;
  return item;
}
///

/// OptionCollector::BooleanOption::ParseGadget
// Read the gadget created for this option and return a flag whether the
// setting has changed and the options have to be re-read by the argument
// parser.
bool OptionCollector::BooleanOption::ParseGadget(void)
{
  class BooleanGadget *gadget = (class BooleanGadget *)Gadget;
  bool setting                = gadget->GetStatus();
  
  if (setting != Setting) {
    Setting = setting;
    return true;
  }
  return false;
}
///

/// OptionCollector::BooleanOption::ParseMenu
// Parse off the settings of the menu item that displays
// this option and install it into the option.
bool OptionCollector::BooleanOption::ParseMenu(void)
{
  class MenuBoolItem *item = (class MenuBoolItem *)MenuItem;
  bool setting             = item->GetState();
  
  if (setting != Setting) {
    Setting = setting;
    return true;
  }
  return false;
}
///

/// OptionCollector::BooleanOption::SaveOption
void OptionCollector::BooleanOption::SaveOption(FILE *to)
{
  fprintf(to,"%s\t=\t%s\n",Name,Setting?("on"):("off"));
}
///

/// OptionCollector::LongOption::LongOption
OptionCollector::LongOption::LongOption(const char *name,const char *help,
					LONG def,LONG min,LONG max)
  : Option(name,help,Option_Long), Min(min), Max(max)
{
  if (def < min) def = min;
  if (def > max) def = max;
  
  Setting = Default = def;
}
///

/// OptionCollector::LongOption::~LongOption
OptionCollector::LongOption::~LongOption(void)
{
}
///

/// OptionCollector::LongOption::Setting
LONG OptionCollector::LongOption::SettingOf(void)
{
  return Setting;
}
///

/// OptionCollector::LongOption::BuildOptionGadget
// Build a suitable gadget representing this option
class Gadget *OptionCollector::LongOption::
  BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,LONG le,LONG te,LONG width)
{
  class Gadget *gadget = Option::BuildOptionGadget(RPort,glist,le,te,width);
  
  te     = gadget->TopEdgeOf() + gadget->HeightOf();
  Gadget = new class RangeGadget(glist,RPort,le,te,width,12,Min,Max,Setting);
  return Gadget;
}
///

/// OptionCollector::LongOption::ParseGadget
// Read the gadget created for this option and return a flag whether the
// setting has changed and the options have to be re-read by the argument
// parser.
bool OptionCollector::LongOption::ParseGadget(void)
{
  class RangeGadget *gadget = (class RangeGadget *)Gadget;
  LONG setting;

  setting = gadget->GetStatus();
  if (setting != Setting && setting >= Min && setting <= Max) {
    Setting = setting;
    return true;
  }
  return false;
}
///

/// OptionCollector::LongOption::SaveOption
void OptionCollector::LongOption::SaveOption(FILE *to)
{
  fprintf(to,"%s\t=\t" LD "\n",Name,Setting);
}
///

/// OptionCollector::StringOption::StringOption
OptionCollector::StringOption::StringOption(const char *name,const char *help,
					    const char *def)
  : Option(name,help,Option_String), 
    Setting(new char[((def)?(strlen(def)):0) + 1]),
    Default(new char[((def)?(strlen(def)):0) + 1])
{
  if (def) {
    strcpy(Setting,def);
  } else {
    *Setting = 0;
  }
  if (def) {
    strcpy(Default,def);
  } else {
    *Default = 0;
  }
}
///

/// OptionCollector::StringOption::~StringOption
OptionCollector::StringOption::~StringOption(void)
{
  delete[] Setting;
  delete[] Default;
}
///

/// OptionCollector::StringOption::Setting
char *OptionCollector::StringOption::SettingOf(void)
{
  return Setting;
}
///

/// OptionCollector::StringOption::BuildOptionGadget
// Build a suitable gadget representing this option
class Gadget *OptionCollector::StringOption::
  BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,LONG le,LONG te,LONG width)
{
  class Gadget *gadget = Option::BuildOptionGadget(RPort,glist,le,te,width);
  
  te     = gadget->TopEdgeOf() + gadget->HeightOf();
  Gadget = new class StringGadget(glist,RPort,le,te,width,12,Setting);

  return Gadget;
}
///

/// OptionCollector::StringOption::ParseGadget
// Read the gadget created for this option and return a flag whether the
// setting has changed and the options have to be re-read by the argument
// parser.
bool OptionCollector::StringOption::ParseGadget(void)
{
  class StringGadget *gadget = (class StringGadget *)Gadget;
  const char *setting;
  
  setting = gadget->GetStatus();
  if (strcmp(setting,Setting)) {
    gadget->ReadContents(Setting);
    return true;
  }

  return false;
}
///

/// OptionCollector::StringOption::SaveOption
void OptionCollector::StringOption::SaveOption(FILE *to)
{
  fprintf(to,"%s\t=\t%s\n",Name,Setting);
}
///

/// OptionCollector::FileOption::FileOption
OptionCollector::FileOption::FileOption(const char *name,const char *help,
					const char *def,bool forsave,bool filesonly,bool dirsonly)
  : Option(name,help,Option_File), 
    Setting(new char[((def)?(strlen(def)):0) + 1]),
    Default(new char[((def)?(strlen(def)):0) + 1]),
    ForSave(forsave), FilesOnly(filesonly), DirsOnly(dirsonly)
{
  if (def) {
    strcpy(Setting,def);
  } else {
    *Setting = 0;
  }
  if (def) {
    strcpy(Default,def);
  } else {
    *Default = 0;
  }
}
///

/// OptionCollector::FileOption::~FileOption
OptionCollector::FileOption::~FileOption(void)
{
  delete[] Setting;
  delete[] Default;
}
///

/// OptionCollector::FileOption::Setting
char *OptionCollector::FileOption::SettingOf(void)
{
  return Setting;
}
///

/// OptionCollector::FileOption::BuildOptionGadget
// Build a suitable gadget representing this option
class Gadget *OptionCollector::FileOption::
  BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,LONG le,LONG te,LONG width)
{
  class Gadget *gadget = Option::BuildOptionGadget(RPort,glist,le,te,width);
  
  te     = gadget->TopEdgeOf() + gadget->HeightOf();
  Gadget = new class FileGadget(glist,RPort,le,te,width,12,Setting,ForSave,FilesOnly,DirsOnly);
  return Gadget;
}
///

/// OptionCollector::FileOption::BuildMenuItem
// Build a menu item representing the option
class MenuItem *OptionCollector::FileOption::BuildMenuItem(class MenuSuperItem *parent)
{
  class MenuFileItem *item;
  item = new class MenuFileItem(parent,Name);
  item->UserPointerOf() = this;
  MenuItem = item;
  return item;
}
///

/// OptionCollector::FileOption::ParseGadget
// Read the gadget created for this option and return a flag whether the
// setting has changed and the options have to be re-read by the argument
// parser.
bool OptionCollector::FileOption::ParseGadget(void)
{
  class FileGadget *gadget = (class FileGadget *)Gadget;
  const char *setting;
  setting = gadget->GetStatus();
  if (strcmp(setting,Setting)) {
    gadget->ReadContents(Setting);
    return true;
  }

  return false;
}
///

/// OptionCollector::FileOption::RequestFile
// The same again, but for parsing off the menu item. This is a non-standard
// call since it requires a file requester.
bool OptionCollector::FileOption::RequestFile(class FileRequester *requester)
{
  if (requester->Request(Name,Setting,ForSave,FilesOnly,DirsOnly)) {
    const char *result;
    // The request worked. Check whether the contents changed.
    result = requester->SelectedItem();
    if (strcmp(result,Setting)) {
      // Ok, it really changed. Now allocate a new setting string.
      delete[] Setting;
      Setting = NULL;
      Setting = new char[strlen(result) + 1];
      strcpy(Setting,result);
      return true;
    }
  }
  // In all other cases: No change.
  return false;
}
///

/// OptionCollector::FileOption::SaveOption
void OptionCollector::FileOption::SaveOption(FILE *to)
{
  fprintf(to,"%s\t=\t%s\n",Name,Setting);
}
///

/// OptionCollector::RadioOption::RadioOption
OptionCollector::RadioOption::RadioOption(const char *name,const char *help,
					  const struct ArgParser::SelectionVector *selections,
					  LONG def)
  : Option(name,help,Option_Selection), Setting(def), Default(def), Names(NULL)
{
  try {
    const struct ArgParser::SelectionVector *src;
    struct ArgParser::SelectionVector *dst;
    LONG count = 1;
    // Compute the size (in entries) of a radio option array
    // including the final NUL
    src = selections;
    while(src->Name) {
      count++,src++;
    }
    // Make a copy of the selection vector here.
    dst = new struct ArgParser::SelectionVector[count];
    // Clear all names such that we are shure what to dispose
    memset(dst,0,sizeof(struct ArgParser::SelectionVector) * count);
    // Now make the copy
    Names = dst;
    src   = selections;
    while(src->Name) {
      char *newname = new char[strlen(src->Name) + 1];
      dst->Name     = newname;
      dst->Value    = src->Value;
      strcpy(newname,src->Name);
      src++,dst++;
    }
  } catch(...) {
    struct ArgParser::SelectionVector *sel;
    // Dispose the partial array now.
    if ((sel = Names)) {
      while(sel->Name) {
      delete[] TOCHAR(sel->Name);
      sel++;
      }
      delete[] Names;
      Names = NULL;
    }
    throw;
  }
}
///

/// OptionCollector::RadioOption::~RadioOption
OptionCollector::RadioOption::~RadioOption(void)
{
  struct ArgParser::SelectionVector *sel;

  // Dispose the selection vector now.
  if ((sel = Names)) {
    while(sel->Name) {
      delete[] TOCHAR(sel->Name);
      sel++;
    }
    delete[] Names;
  }
}
///

/// OptionCollector::RadioOption::Setting
LONG OptionCollector::RadioOption::SettingOf(void)
{
  return Setting;
}
///

/// OptionCollector::RadioOption::BuildOptionGadget
// Build a suitable gadget representing this option
class Gadget *OptionCollector::RadioOption::
  BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,LONG le,LONG te,LONG width)
{
  class Gadget *gadget = Option::BuildOptionGadget(RPort,glist,le,te,width);

  te = gadget->TopEdgeOf() + gadget->HeightOf();
  //
  // now build the radio gadget describing the contents of the selections.
  Gadget = new class RadioGadget(glist,RPort,le,te,width,12,Names,Setting);
  return Gadget;
}
///

/// OptionCollector::RadioOption::BuildMenuItem
// Build a menu item representing the option
class MenuItem *OptionCollector::RadioOption::BuildMenuItem(class MenuSuperItem *parent)
{
  class MenuSelectionItem *item;
  item = new class MenuSelectionItem(parent,Name,Names);
  item->SetState(Setting);
  item->UserPointerOf() = this;
  MenuItem = item;
  return item;
}
///

/// OptionCollector::RadioOption::ParseGadget
// Read the gadget created for this option and return a flag whether the
// setting has changed and the options have to be re-read by the argument
// parser.
bool OptionCollector::RadioOption::ParseGadget(void)
{
  class RadioGadget *gadget = (class RadioGadget *)Gadget;
  LONG setting;
  
  setting = gadget->GetStatus();
  if (setting != Setting) {
    Setting = setting;
    return true;
  }
  
  return false;
}
///

/// OptionCollector::RadioOption::ParseMenu
// Parse off the settings of the menu item that displays
// this option and install it into the option.
bool OptionCollector::RadioOption::ParseMenu(void)
{
  class MenuSelectionItem *item = (class MenuSelectionItem *)MenuItem;
  LONG setting                  = item->GetState();
  
  if (setting != Setting) {
    Setting = setting;
    return true;
  }
  return false;
}
///

/// OptionCollector::RadioOption::SaveOption
void OptionCollector::RadioOption::SaveOption(FILE *to)
{
  const struct SelectionVector *sv = Names;

  while(sv->Name) {
    if (sv->Value == Setting) {
      fprintf(to,"%s\t=\t%s\n",Name,sv->Name);
      return;
    }
    sv++;
  }
  // Huh, the item is not in here?
  fprintf(to,"#%s item is invalid\n",Name);
}
///
