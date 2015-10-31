/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: optioncollector.cpp,v 1.11 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Specialization of an argument parser that collects its
 ** arguments in sub-classes for further processing
 **********************************************************************************/

/// Includes
#include "types.h"
#include "new.hpp"
#include "exceptions.hpp"
#include "optioncollector.hpp"
#include "cmdlineparser.hpp"
#include "machine.hpp"
#include "stdio.hpp"
#include <stdarg.h>
#include <errno.h>
///

/// OptionCollector::OptionCollector
// Build up a new collector for parse-able options
OptionCollector::OptionCollector(class Machine *mach)
  : Machine(mach), Current(NULL), ConfigTime(false)
{
}
///

/// OptionCollector::~OptionCollector
// Disposes all topics, and therefore recursively
// all options.
OptionCollector::~OptionCollector(void)
{
  class Topic *node;
  //
  // Delete all topics. The topic will then dispose all its options.
  while((node = Topics.RemHead())) {
    delete node;
  }
}
///

/// OptionCollector::OptionExceptionPrinter::OptionExceptionPrinter
// to the machine warning printer.
OptionCollector::OptionExceptionPrinter::OptionExceptionPrinter(class Machine *mach)
  : Machine(mach)
{ }
///

/// OptionCollector::OptionExceptionPrinter::PrintException
// The following method is called to print out the exception,
// we forward it directly to the warning printer
void OptionCollector::OptionExceptionPrinter::PrintException(const char *fmt,...)
{	  
  va_list args;
  va_start(args,fmt);
  //
  // Print out the exception as a warning.
  Machine->VPutWarning(fmt,args);

  va_end(args);
}
///

/// OptionCollector::FindOption
// Read a named argument from the argument list: Find an option by its
// name.
class Option *OptionCollector::FindOption(const char *name)
{
  // Scan thru the list of the current topic and find the option that matches.
  // Then return the result as a string.
  if (Current) {
    return Current->FindOption(name);
  }
  return NULL;
}
///

/// OptionCollector::FindTopic
// On re-definition of a title: Find a topic by its name.
class OptionTopic *OptionCollector::FindTopic(const char *name)
{
  class Topic *topic;
  // Scan thru the list of all topics and find the option that matches.
  // Then return the result as a string.
  for(topic = Topics.First();topic;topic=topic->NextOf()) {
    if (topic->Matches(name))
      return (class OptionTopic *)topic;
  }
  return NULL;
}
///

/// OptionCollector::PrintHelp
// Print help text over the required output stream.
// As we never implement any output here, and there is no help
// except what we collect by "defining" them.
void OptionCollector::PrintHelp(const char *,...)
{
}
///

/// OptionCollector::PrintError
// Print a parsing error over the output stream.
void OptionCollector::PrintError(const char *fmt,...)
{
  // Problem case: We do not have good arguments to begin with. If this
  // is the case, but we are still configuring, then we silently ignore
  // this error here... still.
  if (ConfigTime == false) {
    char error[256];
    va_list args;
    va_start(args,fmt);
    // This should not happen here as we should always pass proper arguments.
    // In case we don't, we better generate some kind of error condition.
    // Unfortunately, this is not completely correct...
    vsnprintf(error,255,fmt,args);
#if DEBUG_LEVEL > 0
    fprintf(stderr,"%s\n",error);
#endif
    if (!Machine->Quit())
      Throw(BadPrefs,"OptionCollector::PrintError",error);
    //
    va_end(args);
  }
}
///

/// OptionCollector::DefineTitle
// Define the module this and the following arguments
// are good for.
void OptionCollector::DefineTitle(const char *title)
{
  class OptionTopic *topic;
  //
  // Nothing to do if we are not at config time
  if (ConfigTime) {
    // Create a new topic of the given name.
    topic = BuildTopic(title);
    // And attach it to the topic list of the menu 
    Topics.AddTail(topic);
    //
    // Note that this is the current node
    Current = topic;
  } else {
    // Otherwise check for the selected topic. This might not exist if
    // our configuration added or removed a topic.
    Current = FindTopic(title);
  }
}
///

/// OptionCollector::DefineBool
// Add a boolean option to the current topic.
void OptionCollector::DefineBool(const char *argname,const char *help,bool &var)
{
  class BooleanOption *option;
  //
  if (ConfigTime) {
    // Define a boolean option and attach it to the current topic.
    if (Current == NULL)
      Throw(ObjectDoesntExist,"OptionCollector::DefineBool","boolean option has no topic");
    //
    option = new class BooleanOption(argname,help,var);
    Current->AddOption(option);
  } else {
    option = (class BooleanOption *)FindOption(argname);
    if (option)
      var    = option->SettingOf();
  }
}
///

/// OptionCollector::DefineString
// Define a string argument, resp. parse it. This function
// releases the old string and allocate a new one.
void OptionCollector::DefineString(const char *argname,const char *help,char *&var)
{
  class StringOption *option;
  //
  if (ConfigTime) {
    // Define a new string option and attach it to the current topic.
    if (Current == NULL)
      Throw(ObjectDoesntExist,"OptionCollector::DefineString","string option has no topic");
    //
    option = new class StringOption(argname,help,var);
    Current->AddOption(option);
  } else {
    option = (class StringOption *)FindOption(argname);
    if (option) {
      char *newopt = option->SettingOf();
      delete[] var;
      var = NULL;
      var = new char[strlen(newopt) + 1];
      strcpy(var,newopt);
    }
  }
}
///

/// OptionCollector::DefineFile
// A specialized version of the above, reads file names, or rather,
// records them.
void OptionCollector::DefineFile(const char *argname,const char *help,char *&var,
				 bool forsave,bool filesonly,bool dirsonly)
{
  class FileOption *option;
  //
  if (ConfigTime) {
    // Define a new file option, otherwise ditto.
    if (Current == NULL)
      Throw(ObjectDoesntExist,"OptionCollector::DefineString","file option has no topic");
    //
    option = new class FileOption(argname,help,var,forsave,filesonly,dirsonly);
    Current->AddOption(option);
  } else {
    option = (class FileOption *)FindOption(argname);    
    if (option) {
      char *newopt = option->SettingOf();
      delete[] var;
      var = NULL;
      var = new char[strlen(newopt) + 1];
      strcpy(var,newopt);
    }
  }
}
///

/// OptionCollector::DefineLong
// Define a LONG argument and its range given by min and max.
// Both are inclusive.
void OptionCollector::DefineLong(const char *argname,const char *help,
		      LONG min,LONG max,LONG &var)
{
  class LongOption *option;
  //
  // Configuring or collecting?
  if (ConfigTime) {
    // Define a new range option
    if (Current == NULL)
      Throw(ObjectDoesntExist,"OptionCollector::DefineLong","range option has no meaning");
    //
    option = new class LongOption(argname,help,var,min,max);
    Current->AddOption(option);
  } else {
    option = (class LongOption *)FindOption(argname);
    if (option)
      var    = option->SettingOf();
  }
}
///

/// OptionCollector::DefineSelection
// Define a radio switch type argument given by an array of options and
// values.
void OptionCollector::DefineSelection(const char *argname,const char *help,
			   const struct SelectionVector selections[],
			   LONG &var)
{
  class RadioOption *option;
  //
  if (ConfigTime) {
    // Define a new range option
    if (Current == NULL)
      Throw(ObjectDoesntExist,"OptionCollector::DefineSelection","selection option has no topic");
    //
    option = new class RadioOption(argname,help,selections,var);
    Current->AddOption(option);  
  } else {
    option = (class RadioOption *)FindOption(argname);
    if (option) {
      const struct SelectionVector *vec = selections;
      LONG current                      = option->SettingOf();
      // Check whether this option is available in the
      // selection vector. If not, the selection changed
      // under our feet and we must not install it.
      while(vec->Name) {
	if (vec->Value == current) {
	  var = current;
	  break;
	}
	vec++;
      }
    }
  }
}
///

/// OptionCollector::CollectTopics
// Use all the configurables and collect all the topics we can get hands of.
// This builds all the menu structures for the user interface.
void OptionCollector::CollectTopics(void)
{
  class Topic *node;
  //
  // First clean up all the current topics
  while((node = Topics.RemHead())) {
    delete node;
  }
  Current      = NULL;
  //
  ConfigTime  = true;
  // Now collect new configurables. The machine does this for us conveniently.
  // Note that we are an argument parser as well.
  SignalBigChange(Machine->ParseArgs(this));
}
///

/// OptionCollector::InstallTopics
// After the user changed all settings, re-parse them here
void OptionCollector::InstallTopics(void)
{
  Current    = NULL;
  ConfigTime = false;
  //
  // Now call the ParseArgs again to collect the settings
  SignalBigChange(Machine->ParseArgs(this));
}
///

/// OptionCollector::InstallDefaults
// Re-install the default configuration to fix a configuration error
// due to an invalid user selection.
void OptionCollector::InstallDefaults(void)
{
  class Topic *topic;
  //
  for(topic = Topics.First();topic;topic = topic->NextOf()) {
    topic->InstallDefaults();
  }
}
///

/// OptionCollector::SaveOptions
// Save options to a named file
void OptionCollector::SaveOptions(const char *filename)
{
  FILE *optionfile = NULL;
  class Topic *topic;

  try {
    optionfile = fopen(filename,"w");
    if (optionfile) {
      // Opening the file worked. Now write a header and write all topics we have.
      fprintf(optionfile,
	      "#Atari++ configuration file, saved options.\n"
	      "#Each line represents a setting-value pair, lines starting with a\n"
	      "#hash mark are comments and are hence ignored.\n"
	      "#All options set here are also reachable from the command line by\n"
	      "#placing a dash in front of the option and omitting the '=' sign, i.e.\n"
	      "#atari++ -option value -option value ...\n"
	      "#\n"
	      );
      for (topic = Topics.First();topic;topic = topic->NextOf()) {
	topic->SaveTopic(optionfile);
      }
      fclose(optionfile);
    } else {
      throw AtariException(strerror(errno),"OptionCollector::SaveOptions",
			   "Failed to open %s for writing",filename);
    }
  } catch(const AtariException &ex) {
    class OptionExceptionPrinter oep(Machine);
    if (optionfile)
      fclose(optionfile);
    // Generate a warning from it
    ex.PrintException(oep);
    throw;
  } catch(...) {
    // close at least the file before we bail out....
    if (optionfile)
      fclose(optionfile);
    throw;
  }
}
///

/// OptionCollector::LoadOptions
// Load options from a named file.
void OptionCollector::LoadOptions(const char *filename)
{
  class CmdLineParser *clp = NULL; // where we read commands from...
  FILE *optionfile = NULL;
  
  try {
    // Open the option file, or rather try to.
    optionfile = fopen(filename,"r");
    if (optionfile) {
      clp = new class CmdLineParser;
      // Try to parse off arguments from here. Note that this may fail...
      if (clp->PreParseArgs(optionfile,filename)) {
	// Ok, this worked so far. We are getting better. Now let's try whether the machine can successfully
	// make use of all these options. Since it will throw back at us in case of trouble, we try again.
	try {
	  SignalBigChange(Machine->ParseArgs(clp));
	} catch(const AtariException &ex) {
	  if (ex.TypeOf() == AtariException::Ex_BadPrefs || ex.TypeOf() == AtariException::Ex_IoErr) {
	    // Re-install the defaults again to fix the error.
	    InstallDefaults();
	    // If it still throws, bail out!
	    InstallTopics();
	  }
	  // hand the error description out in any case
	  throw;
	}
      }
      delete clp;
      clp = NULL;
      fclose(optionfile);
    } else {
      throw AtariException(strerror(errno),"OptionCollector::LoadOptions",
			   "Failed to open %s for reading",filename);
    }
  } catch(const AtariException &ex) {
    class OptionExceptionPrinter oep(Machine);
    delete clp;
    if (optionfile)
      fclose(optionfile);
    // Generate a warning from it
    ex.PrintException(oep);
    throw;
  } catch(...) {
    // Cleanup here.
    delete clp;
    if (optionfile) 
      fclose(optionfile);
    throw;
  }
}
///

/// OptionCollector::SaveState
// Save the current machine state to a file
void OptionCollector::SaveState(const char *filename)
{
  // all this is handled in the machine already.
  try {
    Machine->WriteStates(filename);
    //
  } catch(const AtariException &ex) {
    class OptionExceptionPrinter oep(Machine);
    // Generate a warning from it
    ex.PrintException(oep);
    // And pass it out to inform the caller.
    throw;
  }
}
///

/// OptionCollector::LoadState
// Load the current machine state from a file
void OptionCollector::LoadState(const char *filename)
{
  try {
    Machine->ReadStates(filename);
    //
    // Saving worked, just bail out.
  } catch(const AtariException &ex) {
    class OptionExceptionPrinter oep(Machine);
    // Generate a warning from it
    ex.PrintException(oep);
    // And pass it out to inform the caller.
    throw;
  }
}
///

