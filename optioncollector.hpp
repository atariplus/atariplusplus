/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: optioncollector.hpp,v 1.11 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Specialization of an argument parser that collects its
 ** arguments in sub-classes for further processing
 **********************************************************************************/

#ifndef OPTIONCOLLECTOR_HPP
#define OPTIONCOLLECTOR_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "exceptions.hpp"
#include "argparser.hpp"
#include "list.hpp"
#include "configurable.hpp"
#include "menuoption.hpp"
#include "menutopic.hpp"
///

/// Forwards
class Machine;
class RenderPort;
class Gadget;
class MenuItem;
class MenuSuperItem;
class FileRequester;
///

/// Class OptionCollector
// This class specializes the argument parser in the sense
// that it can collect all arguments in private subclasses
// suitable to build a GUI from it.
class OptionCollector : protected ArgParser {
  //
protected:
  //
  // Pointer to the machine
  class Machine *Machine;
  //
  // A simple exception printer that forwards its exception
  // to the machine warning printer.
  class OptionExceptionPrinter : public ExceptionPrinter {
    class Machine *Machine;
  public:
    OptionExceptionPrinter(class Machine *mach);
    // The following method is called to print out the exception.
    virtual void PrintException(const char *fmt,...) PRINTF_STYLE;
  };
  //
  // NOTE: The implementations of all these classes are found in
  // menuoption.cpp, not in optioncollector.cpp.
  //
  // Subclasses derived by the option class, depend on the
  // OptionType entry.
  class BooleanOption : public Option {
    //
    bool                     Setting;
    bool                     Default;
  public:
    BooleanOption(const char *name,const char *help,bool def);
    virtual ~BooleanOption(void);
    //
    bool SettingOf(void);    
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
    virtual bool ParseGadget(void);    
    //    
    // The same again, but for parsing off the menu item.
    virtual bool ParseMenu(void);
    //
    // Re-install the default to fix a configuration error.
    virtual void InstallDefault(void)
    {
      Setting = Default;
    }
    //
    // Save the state of the option to a configuration file.
    virtual void SaveOption(FILE *to);
  };
  //
  // This class keeps a LONG range option
  class LongOption    : public Option {
    //
    LONG                    Setting;
    LONG                    Default;
    LONG                    Min,Max;
  public:
    LongOption(const char *name,const char *help,LONG def,LONG min,LONG max);
    virtual ~LongOption(void);
    //
    LONG SettingOf(void);
    //
    // Create a suitable gadget for this option
    virtual class Gadget *BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,
					    LONG le,LONG te,LONG width);    
    //    
    // Read the gadget created for this option and return a flag whether the
    // setting has changed and the options have to be re-read by the argument
    // parser.
    virtual bool ParseGadget(void);    
    //
    // Re-install the default to fix a configuration error.
    virtual void InstallDefault(void)
    {
      Setting = Default;
    }
    //
    // Save the state of the option to a configuration file.
    virtual void SaveOption(FILE *to);
  };
  //
  // This class keeps a string option
  class StringOption  : public Option {
    //
    char                   *Setting;
    char                   *Default;
  public:
    StringOption(const char *name,const char *help,const char *def);
    virtual ~StringOption(void);
    //
    char *SettingOf(void);
    //
    // Create a suitable gadget for this option
    virtual class Gadget *BuildOptionGadget(class RenderPort *RPort,List<class Gadget> &glist,
					    LONG le,LONG te,LONG width);    
    //
    // Read the gadget created for this option and return a flag whether the
    // setting has changed and the options have to be re-read by the argument
    // parser.
    virtual bool ParseGadget(void);    
    //
    // Re-install the default to fix a configuration error.
    virtual void InstallDefault(void)
    {
      delete[] Setting;
      Setting = NULL;
      Setting = new char[strlen(Default) + 1];
      strcpy(Setting,Default);
    }
    //
    // Save the state of the option to a configuration file.
    virtual void SaveOption(FILE *to);
  };
  //
  // This keeps a file name option
  class FileOption    : public Option {
    //
    char                   *Setting;
    char                   *Default;
    bool                    ForSave;
    bool                    FilesOnly;
    bool                    DirsOnly;
  public:
    FileOption(const char *name,const char *help,const char *def,
	       bool forsave,bool filesonly,bool dirsonly);
    virtual ~FileOption(void);
    //
    char *SettingOf(void);
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
    virtual bool ParseGadget(void);    
    //
    // The same again, but for parsing off the menu item. This is a non-standard
    // call since it requires a file requester.
    bool RequestFile(class FileRequester *requester);
    //
    // Re-install the default to fix a configuration error.
    virtual void InstallDefault(void)
    {
      delete[] Setting;
      Setting = NULL;
      Setting = new char[strlen(Default) + 1];
      strcpy(Setting,Default);
    }
    //
    // Save the state of the option to a configuration file.
    virtual void SaveOption(FILE *to);
  };
  //
  // This keeps a list of selectables, i.e. it represents an array of
  // radio buttons.
  class RadioOption  : public Option {
    //
    LONG                    Setting;
    LONG                    Default;
    struct ArgParser::SelectionVector *Names;
    //
  public:
    RadioOption(const char *name,const char *help,const struct ArgParser::SelectionVector *selections,
		LONG def);
    virtual ~RadioOption(void);
    //
    LONG SettingOf(void);
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
    virtual bool ParseGadget(void);
    //   
    // The same again, but for parsing off the menu item.
    virtual bool ParseMenu(void);
    // 
    // Re-install the default to fix a configuration error.
    virtual void InstallDefault(void)
    {
      Setting = Default;
    }
    //
    // Save the state of the option to a configuration file.
    virtual void SaveOption(FILE *to);
  };
  //
  // List of all the topics is kept here:
  List<Topic>            Topics;  
  // Current option we are working with. This should also be the head
  // of the OptionList
  class OptionTopic     *Current;
  //
  // The following boolean defines whether we are collecting topics and
  // options from the configurables - then it is true - or whether we
  // are passing options out to the configurables - then it is false.
  bool                   ConfigTime;
  //
  // Find an option from its name in the argument list from the current
  // topic.
  class Option *FindOption(const char *name);
  // On re-definition of a title: Find a topic by its name.
  // This does never return any non-preferences related topics.
  class OptionTopic *FindTopic(const char *name);
  // A virtual callback that must deliver an option topic. This
  // is here to allow a derived class to enhance the OptionTopic by
  // something smarter if required. This method is called to 
  // generate new topics on argument parsing.
  virtual class OptionTopic *BuildTopic(const char *title) = 0;
  //
  // *******************************************************************************
  // Implementation of the argument parser methods follows below
  // *******************************************************************************
  //
  // Print help text over the required output stream.
  virtual void PrintHelp(const char *fmt,...) PRINTF_STYLE;
  //
  // Print a parsing error over the output stream.
  virtual void PrintError(const char *fmt,...) PRINTF_STYLE;
  //
  // Argument registering methods
  //
  // Define the module this and the following arguments
  // are good for.
  virtual void DefineTitle(const char *title);
  //
  // Define a boolean argument, resp. parse it.
  virtual void DefineBool(const char *argname,const char *help,bool &var);
  //
  // Define a string argument, resp. parse it. This function
  // releases the old string and allocate a new one.
  virtual void DefineString(const char *argname,const char *help,char *&var);
  //
  // A specialized version of the above, reads file names, the boolean
  // option specifies whether the file must exist (then false) or is
  // a target file name to save something to (then true)
  virtual void DefineFile(const char *argname,const char *help,char *&var,
			  bool forsave,bool filesonly,bool dirsonly);
  //
  // Define a LONG argument and its range given by min and max.
  // Both are inclusive.
  virtual void DefineLong(const char *argname,const char *help,
			  LONG min,LONG max,LONG &var);
  //
  // Define a radio switch type argument given by an array of options and
  // values.
  virtual void DefineSelection(const char *argname,const char *help,
			       const struct SelectionVector selections[],
			       LONG &var);
  //
  //
  // Interface methods beyond the argument parser interface.
  //
  // Use all the configurables and collect all the topics we can get hands of.
  // This builds all the menu structures for the user interface.
  void CollectTopics(void);
  //
  // After the user changed all settings, re-parse them here. Note that this
  // requires the re-seating of the RPort.
  void InstallTopics(void);
  //
  // Re-install the defaults into all topics and options in case a setting
  // is invalid. This should hopefully fix it.
  void InstallDefaults(void);
  //
  // Save options to a named file
  void SaveOptions(const char *filename);
  //
  // Load options from a named file.
  void LoadOptions(const char *filename);
  //
  // Save the machine state to a named file
  void SaveState(const char *name);
  //
  // Load the machine state from a named file
  void LoadState(const char *name);
  //
public:
  OptionCollector(class Machine *mach);
  virtual ~OptionCollector(void);
};
///

///
#endif
