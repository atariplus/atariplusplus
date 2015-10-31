/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cmdlineparser.cpp,v 1.25 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Parser subclass that reads data from the command line
 **********************************************************************************/

/// Includes
#include "types.h"
#include "cmdlineparser.hpp"
#include "exceptions.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "stdlib.hpp"
#include "curses.hpp"
#include "unistd.hpp"
#include "new.hpp"
#include <ctype.h>
///

/// CmdLineParser::CmdLineParser
CmdLineParser::CmdLineParser(void)
  : args(NULL), Columns(0), ParseSource(NULL),
    HelpBuffer(NULL), HelpBufferSize(0), Indent(-1), Remains(0)
{ 
}
///

/// CmdLineParser::~CmdLineParser
CmdLineParser::~CmdLineParser(void)
{
  struct Argument *next,*arg = args;
  //
  while(arg) {
    next = arg->next;
    delete arg;
    arg = next;
  }
  delete[] HelpBuffer;
}
///

/// CmdLineParser::GetWidth
// Get the length of a output line in characters
void CmdLineParser::GetWidth(void)
{
#if HAVE_GETENV
  const char *colwidth;
  char *endptr;
  LONG value;
  //
  // Evaluate the COLUMNS environment variable and suppose this
  // is the width of the terminal in characters.
  colwidth = getenv("COLUMNS");
  if (colwidth) {
    value    = strtol(colwidth,&endptr,10);
    // Check whether this value is legal.
    if (*endptr == '\0' && value >= 10 && value <= 512) {
      // Seems reasonable
      Columns = value;
      return;
    }
  }
#endif  
  //
  // Set this to the default.
  Columns = 80;
  //
#if USE_CURSES && HAVE_NEWTERM && HAS_COLS && HAVE_ISATTY
 if (isatty(1)) {
   SCREEN *s = newterm(NULL,stdout,stdin);
   if (s) {
     if (COLS >= 10 && COLS <= 512) {
       Columns = COLS;
     }
     endwin();
     delscreen(s);
   }
 }
#endif
}
///

/// CmdLineParser::PrintHelp
// Print a single line to the output with a given intention
// Insert spaces, insert intention as required.
void CmdLineParser::PrintHelp(const char *fmt,...)
{
  va_list arglist;
  char buffer[256];
  static const char blanks[9] = "        "; // must be eight blanks for intention
  char *text;
  int size;

  va_start(arglist,fmt);
#if MUST_OPEN_CONSOLE
  OpenConsole();
#endif
  //
  // Compute the size of the required buffer.
  // Ensure NUL-termination.
  buffer[255] = '\0';
  size = vsnprintf(buffer,255,fmt,arglist);
  va_end(arglist);
  va_start(arglist,fmt);
  // Check whether we need to work around some earlier
  // glibc bugs.
  if (size < 0 || size < 255) {
    text = buffer; // pointer to the buffer text. Enough data here.
  } else {
    // Otherwise, do not truncate but allocate enough room.
    size++;
    if (HelpBufferSize < size) {
      // Enlarge the input buffer of the helper.
      delete[] HelpBuffer;
      HelpBuffer     = NULL;
      HelpBufferSize = size;
      HelpBuffer     = new char[HelpBufferSize];
    }
    vsnprintf(HelpBuffer,HelpBufferSize,fmt,arglist);
    text = HelpBuffer;
  }
  //
  // Next step is to get the terminal size.
  if (Columns <= 0) {
    GetWidth();
    Remains = Columns;
  }
  //
  // Now print the text
  do {
    size_t len;
    char *tmp;
    bool lf = false; // print a line feed at the end?
    //
    // Check whether we need to re-set the indention.
    if (Indent < 0) {
      int i;
      Indent  = 0;
      Remains = Columns;
      while(*text == '\t') {
	if (Remains >= 10) {
	  // Sets maximal indention
	  Indent++;
	  Remains -= 8;
	}
	text++;
      }
      // Start a new line. For that, print the indention.
      // If the first character is a "-", remove one
      // indention to have an alignment to the next character.
      i = Indent;
      while(i) {
	if (i == 1 && *text == '-') {
	  printf(blanks+1);
	} else {
	  printf(blanks);
	}
	i--;
      }
    }
    //
    // Calculate how many characters we may print. This is
    // the number of characters up to the next \n or the
    // end of the string.
    tmp = text;
    while(*tmp && *tmp != '\n') {
      tmp++;
    }
    len = tmp - text;
    if (len >= size_t(Remains)) {
      // Oops, the line is too LONG. Try to find a canonical
      // truncation position by looking for a blank.
      tmp = text + Remains;
      // tmp now points behind the last position.
      do {
	tmp--;
	if (isspace(*tmp)) {
	  // This would be a canonical position, for example.
	  break;
	}
	// Until we reach the start of the string. If so, then
	// we haven't found a truncation position and we need
	// to cut the string somewhere in the middle.
      } while(tmp > text);
      // If here tmp == text, then there is no blank where we
      // could truncate.
      if (tmp >= text) {
	// Do not truncate in the middle, get the length from
	// tmp now. Tmp points now to the blank itself.
	len = tmp - text;
      } else {
	len = Remains;
      }
      // Remember that we need to print a lf afterwards.
      lf = true;
    }
    // Print the string up to the blank
    if (len > 0) {
      printf("%1.*s",int(len),text);
      text    += len;
      Remains -= (int)len;
    }
    if (lf || *text == '\n') {
      printf("\n");
      Remains  = Columns;
      if (lf) {
	int i;
	Remains -= Indent << 3;      
	// Start a new line. For that, print the indention.
	for(i=0;i<Indent;i++) {
	  printf(blanks);
	}
	// Skip blanks on the next line.
	while(*text && isspace(*text))
	  text++;
      }
    }
    // If the last line ended with a /n, then skip it here
    // because we didn't in the last go and get the next indention
    // value
    if (*text == '\n') {
      text++;
      // Need to remember that we need to recompute the indention
      Indent = -1;
    }
    //
  } while(*text);
  va_end(arglist);
}
///

/// CmdLineParser::FindArgument
const char *CmdLineParser::FindArgument(const char *name)
{
  struct Argument *arg = args;
  // Find an argument by name
  while(arg) {
    if (!strcasecmp(arg->name,name)) {
      // Found me!
      return arg->value;
    }
    arg = arg->next;
  }
  //
  // Return NULL if not found
  return NULL;
}
///

/// CmdLineParser::PrintError
void CmdLineParser::PrintError(const char *fmt,...)
{
  if (givehelp == false) {
    char buffer[256];
    va_list arglist;
    
    /// This should be really replaced by something fancier later on...
    va_start(arglist,fmt);
    vsnprintf(buffer,255,fmt,arglist);
    Throw(BadPrefs,"CmdLineParser::PrintError",buffer);
    va_end(arglist);
  }
}
///

/// CmdLineParser::PreParseArgs
// Run an argument parser from the command line arguments
// This also filters for --help or -h or -help
bool CmdLineParser::PreParseArgs(int argc,char **argv,const char *info)
{
  struct Argument **last;
  //
  // Find the argument we attach more arguments to.
  last        = &args;
  while (*last) {
    last      = &((*last)->next);
  }
  ParseSource = info; // Note where these arguments came from
  //
  // drop the command name
  argc--;
  argv++;
  while(argc > 0) {
    size_t len;
    char  *s = *argv; // source where to copy from
    //
    // Get the length of the source.
    len = strlen(s);
    if (len >= 256) {
      PrintError("Argument length overflow in %s, must be smaller than 256 characters.\n",ParseSource);
      return false;
    }
    // Check whether this argument starts with a '-'. If not, something is wrong.
    if (s[0] != '-') {
      PrintError("Missing value for argument in %s, or missing '-' in front "
		 "of argument %s.\n",ParseSource,*argv);
      return false;
    }
    s++;
    // Now check whether we have a double '-'. If so, drop it as well.
    if (s[0] == '-') {
      s++;
    }
    // This name has been successfully scanned off.
    argv++;
    argc--;
    // Check whether we have a "help" or "h" (hence --help, -help, -h or --h) If so,
    // ignore this argument, and notice that we should get help.
    if (!strcasecmp(s,"help") || !(strcasecmp(s,"h"))) {
      givehelp = true;
    } else {
      struct Argument *arg = args;
      // Now scan whether we have this argument already.
      while(arg) {
	if (!strcasecmp(s,arg->name)) {
	  // Yup. The one that is scanned last has the highest priority, hence replace this one.
	  break;
	}
	arg = arg->next;
      }
      if (arg == NULL) {
	// We do not have an argument of this type yet, so allocate it and link it in.
	arg   = new struct Argument;
	*last = arg;
	last  = &(arg->next); // continue to link in here.
	// Now allocate the name. We must have dropped a single minus, 
	// hence len bytes are LONG enough.
	arg->name = new char[len];
	strcpy(arg->name,s);
      } else {
	// Now replace the value of this argument with the next argument.
	delete[] arg->value;
	arg->value = NULL; // ensure exception savety.
      }
      // Attach the new value if there is one.
      if (argc) {
	len = strlen(*argv);
	if (len >= 256) {
	  PrintError("Value of argument -%s in %s is too LONG,\n"
		     "must be smaller than 256 characters.\n",
		     arg->name,ParseSource);
	  return false;
	}
	// Copy it over.
	arg->value = new char[len + 1];
	strcpy(arg->value,*argv);
	// Drop this argument as well.
	argc--;
	argv++;
      } else {
	// We run out of arguments. Yuck.
	PrintError("Missing value for argument -%s in %s.",arg->name,ParseSource);
	return false;
      }
    }
  }
  //
  return true;
}
///

/// CmdLineParser::PreParseArgs
// Run an argument parser from a configuration file
bool CmdLineParser::PreParseArgs(FILE *file,const char *info)
{
  struct Argument **last;
  LONG lineno = 0;
  char line[512];
  //
  // Find the argument where to link further arguments to.
  last        = &args;
  while (*last) {
    last      = &((*last)->next);
  }
  //
  ParseSource = info;
  while(!feof(file)) {
    size_t len;
    struct Argument *next;
    char *sep,*value,*arg,*end;
    //
    errno = 0;
    if (fgets(line,512,file) == NULL) {
      if (errno) {
	PrintError("Failed reading the configuration file %s because: %s\n",
		   ParseSource,strerror(errno));
	return false;
      }
      return true;
    }
    lineno++;
    len = strlen(line);
    // Check whether the line overflows
    if (len >= 512) {
      PrintError("Configuration file %s line # " LD " too LONG.\n",ParseSource,lineno);
      return false;
    }
    // Check whether this is a comment or an empty line. If so, skip
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
      continue;
    //
    // scan for the "=" separating argument from value (hopefully)
    sep = strchr(line,'=');
    if (sep == NULL) {
      PrintError("Configuration file %s line # " LD " misses an '=' sign to\n"
		 "separate argument from value:\n%s\n",ParseSource,lineno,line);
      return false;
    }
    *sep = '\0';
    // Get pointer to the value string, erase leading and trailing blanks
    value = sep+1;
    while(*value != '\0' && isspace(*value)) 
      value++;
    // Now advance towards the end of the argument
    end = value;
    while(*end)
      end++;
    // Now remove trailing blanks
    do {
      *end = '\0';
      end--;
    } while(end >= value && isspace(*end));
    // same for the argument
    do {
      *sep = '\0';
      sep--;
    } while(sep >= line && isspace(*sep));
    arg = line;
    while(*arg != '\0' && isspace(*arg))
      arg++;
    //
    // Check against overLONG arguments.
    len = strlen(arg);
    if (len >= 256) {
      PrintError("Configuration file %s line # " LD " argument is too LONG,\n"
		 "must be smaller than 256 characters.\n",ParseSource,lineno);
      return false;
    }
    //
    // Now check whether we have this argument already set in the database. If so,
    // we must replace it as the last scanned source has priority.
    next  = args;
    while(next) {
      if (!strcasecmp(next->name,arg))
	break;
      next = next->next;
    }
    if (next == NULL) {
      // We don't. Now get a new argument and link it in.
      next  = new struct Argument;
      *last = next;
      last  = &(next->next); // next has to be attached here.
      //
      // Build the name string.
      next->name = new char[len + 1];
      strcpy(next->name,arg);
    } else {
      // Dispose the old value string as we have to replace it.
      delete[] next->value;
      next->value = NULL;
    }
    // Now check again for the argument value
    len = strlen(value);
    if (len >= 256) {
      PrintError("Configuration file %s line # " LD " argument value of argument %s\n"
		 "is too LONG, must be smaller than 256 characters.\n",ParseSource,lineno,arg);
      return false;
    }
    next->value = new char[len + 1];
    strcpy(next->value,value);
  }
  //
  return true;
}
///

/// CmdLineParser::DefineTitle
// Define the module this and the following arguments
// are good for.
void CmdLineParser::DefineTitle(const char *title)
{
  if (IsHelpOnly()) {
    // Currently, only required if some help
    // is requested.
    PrintHelp("\n\n%s specific options:\n",title);
  }
}
///

/// CmdLineParser::DefineBool
// Define a boolean argument, resp. parse it.
void CmdLineParser::DefineBool(const char *argname,const char *help,bool &var)
{
  const char *arg;
  
  if (IsHelpOnly()) {
    PrintHelp("\t-%s <bool> [Default=%s] : %s\n",argname,(var)?("on"):("off"),help);
  }
  if ((arg = FindArgument(argname))) {
    if (!MatchesBool(arg,var)) {
      PrintError("%s argument %s in %s is not boolean.\n",argname,arg,ParseSource);
      Throw(InvalidParameter,"CmdLineParser::DefineBool","argument is not boolean");
    }
  }
}
///

/// CmdLineParser::DefineString
// Define a string argument, resp. parse it. This function
// releases the old string and allocate a new one.
void CmdLineParser::DefineString(const char *argname,const char *help,char *&var)
{
  const char *arg;

  if (IsHelpOnly()) {
    PrintHelp("\t-%s <string> [Default=%s] : %s\n",
	      argname,(var)?(var):("(none)"),help);
  }
  if ((arg = FindArgument(argname))) {
    delete[] var;
    var = NULL;
    var = new char[strlen(arg) + 1];
    strcpy(var,arg);
  }
}
///

/// CmdLineParser::DefineFile
// Define a file name argument, resp. parse it. This function
// releases the old string and allocate a new one. It also
// resolves some common expressions, i.e. ~ for $HOME
void CmdLineParser::DefineFile(const char *argname,const char *help,char *&var,bool,bool,bool)
{
  const char *arg;

  if (IsHelpOnly()) {
    PrintHelp("\t-%s <path> [Default=%s] : %s\n",
	      argname,(var)?(var):("(none)"),help);
  }
  if ((arg = FindArgument(argname))) {
    char *home = NULL;
    size_t len;
    delete[] var;
    var = NULL;
    len = strlen(arg) + 1;
#if HAVE_GETENV
    if (arg[0] == '~') {
      home = getenv("HOME");
      if (home) {
	len += strlen(home) - 1; // remove the ~ as well.
      }
    }
#endif
    var = new char[len];
    if (arg[0] == '~' && home) {
      sprintf(var,"%s%s",home,arg+1);
    } else {
      strcpy(var,arg);
    }
  }
}
///

/// CmdLineParser::DefineLong
// Define a LONG argument and its range given by min and max.
// Both are inclusive.
void CmdLineParser::DefineLong(const char *argname,const char *help,
			       LONG min,LONG max,LONG &var)
{
  LONG value;
  const char *arg;

  if (IsHelpOnly()) {
    PrintHelp("\t-%s <" LD ".." LD "> [Default=" LD "] : %s\n",
	      argname,min,max,var,help);
  } 
  if ((arg = FindArgument(argname))) {
    if (!MatchesLong(arg,value)) {
      PrintError("%s argument %s in %s is not numeric.\n",argname,arg,ParseSource);
      Throw(InvalidParameter,"ArgParser::DefineLong","argument is not numeric");
    }
    if (value < min || value > max) {
      PrintError("%s argument " LD " in %s is out of range. Must be >= " LD " and <= " LD ".\n",
		 argname,value,ParseSource,min,max);
      Throw(OutOfRange,"ArgParser::DefineLong","argument is out of range");
    }
    var = value;
  }
}
///

/// CmdLineParser::DefineSelection
// Define a radio switch type argument given by an array of options and
// values.
void CmdLineParser::DefineSelection(const char *argname,const char *help,
				    const struct SelectionVector selections[],
				    LONG &var)
{
  const struct SelectionVector *sec;
  const char *arg;
  
  if (IsHelpOnly()) {
    PrintHelp("\t-%s <",argname);
    for(sec = selections;sec->Name;sec++) {
      PrintHelp("%s%c",sec->Name,(sec[1].Name)?('|'):('>'));
    }
    for(sec = selections;sec->Name;sec++) {
      if (sec->Value == var) 
	PrintHelp(" [Default=%s] : %s\n",sec->Name,help);
    }
  }
  if ((arg = FindArgument(argname))) {
    for(sec = selections;sec->Name;sec++) {
      if (!strcasecmp(sec->Name,arg)) {
	var = sec->Value;
	return;
      }
    }
    PrintError("%s argument %s in %s is not a valid option.\n",argname,arg,ParseSource);
    Throw(InvalidParameter,"CmdLineParser::DefineSelection",
	  "argument is not on the available option list");
  }
}
///
