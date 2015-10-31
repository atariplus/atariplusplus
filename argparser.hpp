/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: argparser.hpp,v 1.26 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Generic argument parser superclass
 **********************************************************************************/

#ifndef ARGPARSER_HPP
#define ARGPARSER_HPP

/// Includes
#include "types.hpp"
#include "stdio.hpp"
///

/// Class ArgParser
class ArgParser {
  //  
public:
  // Definition for argparser status flags: Once an argument has several
  // side effects as changing the meaning or the existance of other arguments
  // then the following masks must be signaled:
  enum ArgumentChange {
    NoChange,  // did not change at all. Why did you signal?
    Reparse,   // requires an argument pre-parse since some other arguments changed
    ColdStart  // requires a coldstart to initialize the new components
  };
  //
protected:
  //
  // The following is set in case we found that we must give help
  bool           givehelp;
  //
  // Collection of changes found during argument parsing
  ArgumentChange ArgChangeFlag;
  //
  // The following *should* be actually protected, but due to a bug in the
  // g++ 2.95.x series, we cannot allow this. Hence, make this public for
  // now. *sigh*
public:
  // Returns true if the string passed in matches the other string
  // Note that this is differnent to what strcmp does.
  static bool Matches(const char *match,const char *str);
  //
  // Evaluate a boolean true/false condition. Returns false
  // if the result is invalid.
  static bool MatchesBool(const char *str,bool &result);
  //
  // Evaluate an integer argument. Returns false if the result
  // is invalid.
  static bool MatchesLong(const char *str,LONG &result);
  //
  //
protected:
  //
  // Check whether the current invocation is just to give help
  // or for the real thing
  bool IsHelpOnly(void) const
  {
    return givehelp;
  }
  //  
  // Never build my kind directly, but extend my interface and
  // implement me:
  ArgParser(bool help = false);
  //
public:
  virtual ~ArgParser(void);
  //
  // Print help text over the required output stream.
  virtual void PrintHelp(const char *fmt,...) PRINTF_STYLE = 0;
  //
  // Print a parsing error over the output stream.
  virtual void PrintError(const char *fmt,...) PRINTF_STYLE = 0;
  //
  // Argument registering methods
  //
  // Define the module this and the following arguments
  // are good for.
  virtual void DefineTitle(const char *title) = 0;
  //
  // Define a boolean argument, resp. parse it.
  virtual void DefineBool(const char *argname,const char *help,bool &var) = 0;
  //
  // Define a string argument, resp. parse it. This function
  // releases the old string and allocate a new one.
  virtual void DefineString(const char *argname,const char *help,char *&var) = 0;
  //
  // A specialized version of the above, reads file names. The option
  // defines wether the file must exist or not.
  virtual void DefineFile(const char *argname,const char *help,char *&var,
			  bool forsave,bool filesonly,bool dirsonly) = 0;
  //
  // Define a LONG argument and its range given by min and max.
  // Both are inclusive.
  virtual void DefineLong(const char *argname,const char *help,
			  LONG min,LONG max,LONG &var) = 0;
  //
  // Definition for selection types
  struct SelectionVector {
    const char *Name;    // name and
    LONG        Value;   // value. Last one has name set to NULL
  };
  // Define a radio switch type argument given by an array of options and
  // values.
  virtual void DefineSelection(const char *argname,const char *help,
			       const struct SelectionVector selections[],
			       LONG &var) = 0;
  //
  // The following is actually only useful if we want to define a 
  // hierarchical menu. If called, a (super) item of the given name is
  // created and all following argument parser calls create the sub-items
  // of this super-item.
  // By default, the/a argument parser need not to implement this, but
  // the title menu will.
  virtual void OpenSubItem(const char *)
  {
    // does nothing by default.
  }
  //
  // Pairs with the above and closes a sub-item
  virtual void CloseSubItem(void)
  {
    // does nothing by default
  }
  //
  // Signal a change in the argument change flag, i.e. prepare to re-read
  // some arguments if this is required. This method is here for client
  // purposes that may require to enforce an argument re-parsing.
  void SignalBigChange(ArgumentChange changeflag);
  //
  // Return the reparse flag and reset it.
  ArgumentChange ReparseState(void)
  {
    ArgumentChange current = ArgChangeFlag;
    ArgChangeFlag          = NoChange;
    return current;
  }
};
///

///
#endif
