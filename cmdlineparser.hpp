/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cmdlineparser.hpp,v 1.10 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Parser subclass that reads data from the command line
 **********************************************************************************/

#ifndef CMDLINEPARSER_HPP
#define CMDLINEPARSER_HPP

/// Includes
#include "argparser.hpp"
///

/// class CmdLineParser
class CmdLineParser : public ArgParser {
  // A singly linked list of arguments
  struct Argument {
    struct Argument *next;
    char *name,*value;
    //
    Argument(void)
      : next(NULL), name(NULL), value(NULL)
    { };
    //
    ~Argument(void)
    {
      delete[] name;
      delete[] value;
    }
  }          *args;
  //
  // Length of a line for helper output. This is zero until we get the size.
  LONG        Columns;
  //
  // Name of the source of the argument parser. This is used for printing error messages
  // and for informing the user.
  const char *ParseSource;
  //
  //
  // Temporary buffer used for keeping the helper output.
  char       *HelpBuffer;
  LONG        HelpBufferSize;
  //
  // Current number of indention for the help printer
  int         Indent;
  // Remaining characters on the line
  int         Remains;
  //
  // Get the length of a output line in characters
  void        GetWidth(void);
  //
  // Read a named argument from the argument list. 
  const char *FindArgument(const char *name);
  //  
public:
  CmdLineParser(void);
  virtual ~CmdLineParser(void);
  //  
  // Check whether the current invocation is just to give help
  // or for the real thing
  bool IsHelpOnly(void) const
  {
    return givehelp;
  }
  //
  // Run an argument parser from the command line arguments
  bool PreParseArgs(int argc,char **argv,const char *info);
  //
  // Run an argument parser from a configuration file
  bool PreParseArgs(FILE *file,const char *info);
  //
  // Print help text over the required output stream.
  void PrintHelp(const char *fmt,...) PRINTF_STYLE;
  //
  // Print a parsing error over the output stream.
  void PrintError(const char *fmt,...) PRINTF_STYLE;
  //
  // Argument registering methods
  //
  // Define the module this and the following arguments
  // are good for.
  void DefineTitle(const char *title);
  //
  // Define a boolean argument, resp. parse it.
  void DefineBool(const char *argname,const char *help,bool &var);
  //
  // Define a string argument, resp. parse it. This function
  // releases the old string and allocate a new one.
  void DefineString(const char *argname,const char *help,char *&var);
  //
  // A specialized version of the above, reads file names.
  void DefineFile(const char *argname,const char *help,char *&var,
		  bool forsave,bool filesonly,bool dirsonly);
  //
  // Define a LONG argument and its range given by min and max.
  // Both are inclusive.
  void DefineLong(const char *argname,const char *help,LONG min,LONG max,LONG &var);
  //
  // Define a radio switch type argument given by an array of options and
  // values.
  void DefineSelection(const char *argname,const char *help,
		       const struct SelectionVector selections[],
		       LONG &var);
};
///

///
#endif
