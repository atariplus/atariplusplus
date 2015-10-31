/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: main.cpp,v 1.31 2015/05/21 18:52:40 thor Exp $
 **
 ** In this module: Main entrance point of the emulator, main loop.
 **********************************************************************************/

/// Includes
#include "main.hpp"
#include "machine.hpp"
#include "atari.hpp"
#include "cmdlineparser.hpp"
#include "exceptions.hpp"
#include "errorrequester.hpp"
#include "new.hpp"
#include "stdio.hpp"
///

/// Protos
static void ParseFromFile(class CmdLineParser *args,const char *filename);
///

/// MainExceptionPrinter
class MainExceptionPrinter : public ExceptionPrinter {
  //
public:
  virtual void PrintException(const char *fmt,...);
};
//
// avoid inlining the following. g++ 2.xx complains otherwise...
void MainExceptionPrinter::PrintException(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
#if MUST_OPEN_CONSOLE
  OpenConsole();
#endif
#if CHECK_LEVEL > 0
  fprintf(stderr,"*** Atari++ generated an exception ***\n");
#endif
  vfprintf(stderr,fmt,args);
  fprintf(stderr,"\n\n");
  va_end(args);
}
///

/// ParseFromFile
// Run the argument parser from a given file
static void ParseFromFile(class CmdLineParser *args,const char *filename)
{
  FILE *in = NULL;
  
  try {
    in = fopen(filename,"r");
    if (in) {
      if (!args->PreParseArgs(in,filename)) {
	throw AtariException(NULL,AtariException::Ex_InvalidParameter,filename,
			     "main.cpp",__LINE__,
			     "configuration file is invalid");
      }
      //
      fclose(in);
    }
  } catch(...) {
    if (in) fclose(in);
    throw;
  }
}
///

/// main
int main(int argc,char **argv)
{
  class MainExceptionPrinter printer; // use this to generate the debug output
  class Machine *mach       = NULL;
  class CmdLineParser *args = NULL;
  char *configname          = NULL;
  char *statename           = NULL;
  int rc = 0;

  try {
    bool retry;
    bool domenu;
    char *home;
    char confname[80];
    // First build the machine and all its subclasses.
    mach = new class Machine;
    args = new class CmdLineParser;
    //
    // This constructs all subclasses and links them into the
    // main emulator object
    mach->BuildMachine(args);  
    //
    // Are there any global arguments?
    ParseFromFile(args,"/etc/atari++/atari++.conf");
    //
    // Are there any user specific arguments in the home directory?
#if HAVE_GETENV
    if ((home = getenv("HOME"))) {
      snprintf(confname,79,"%s/.atari++.conf",home);
      ParseFromFile(args,confname);
    }
#endif
    //
    // Are there any local options in the current directory?
    ParseFromFile(args,".atari++.conf");
    //
    // Now parse off arguments from the command line
    if (!args->PreParseArgs(argc,argv,"command line")) 
      throw AtariException(NULL,AtariException::Ex_InvalidParameter,"command line",
			   "main.cpp",__LINE__,
			   "arguments are invalid");
    //    
    if (args->IsHelpOnly()) {
      args->PrintHelp("Atari++ Settings: Use these in the command line as "
		      "\"-option value\" and in the .atari++.conf as "
		      "\"option = value\"\n\n"
		      "-h or --help: print this command line help\n");
    }
    // Define global arguments here.
    args->DefineTitle("Global options");
    args->DefineFile("config","configuration file to load",configname,false,true,false);
    args->DefineFile("state","status snapshot file to load",statename,false,true,false);
    // Check whether we have a configuration file to load. If so, do now.
    if (configname && *configname)
      ParseFromFile(args,configname);
    //
    //
    domenu = false;
    do {
      retry  = false;
      // The following argument parsing may fail. All provided we get enough data
      // to open a display, then we open at least the user menu.
      try {
	// Now overload these settings by the command line argument.
	if (domenu) {
	  mach->EnterMenu();
	  domenu = false;
	} else {
	  mach->ParseArgs(args);
	  domenu = false;
	}
      } catch(const class AsyncEvent &ev) {
	switch (ev.TypeOf()) {
	case AsyncEvent::Ev_Exit:
	  rc = 10;
	  break;
	case AsyncEvent::Ev_ColdStart:
	case AsyncEvent::Ev_WarmStart:
	  domenu = false;
	  retry  = true;
	  break;
	case AsyncEvent::Ev_EnterMenu:
	  retry  = true;
	  domenu = true;
	  break;
	}
      } catch(const class AtariException &ex) {
	if (ex.TypeOf() == AtariException::Ex_BadPrefs) {
	  // Ok, something with the configuration isn't right. Maybe we have at least all 
	  // the required data to setup a requester. If so, then enter the user
	  // menu here. All other errors are forwarded directly to the front-end.
	  switch(mach->PutError(ex)) {
	  case ErrorRequester::ERQ_Cancel:
	  case ErrorRequester::ERQ_Nothing:
	    // Just forward the error to the catch below.
	    throw;
	    break;
	  case ErrorRequester::ERQ_Retry:
	    retry  = true;
	    domenu = false;
	    break;
	  case ErrorRequester::ERQ_Menu:
	    // Run the user menu.
	    retry  = true;
	    domenu = true;
	    break;
	  }
	} else {
	  // Not a configuration error. Forward the error to the catch below.
	  throw;
	}
      }
    } while(retry);
    //
    // Now that we parsed all arguments, check whether we should really
    // run the emulator (we do not in case the user just requested help
    // about the command line arguments)
    if (!args->IsHelpOnly()) {
      // Power-up the machine
      mach->ColdStart();
      // Check for a state name. Load now if available.
      if (statename && *statename)
	mach->ReadStates(statename);
      // Run the emulator
      mach->Atari()->EmulationLoop();
    }
#if MUST_OPEN_CONSOLE
    else {
      char buf[3];
      //
      printf("\nPress RETURN to continue...\n\n");
      fgets(buf,2,stdin);
    }
#endif
  } catch(const AtariException &ex) {
    // Uhoh, something went wrong somewhere. If so, print the
    // exception and exit. There's nothing else we could do here.
    ex.PrintException(printer);
    rc = 10;
  }

  delete[] configname;
  delete[] statename;
  delete args;
  // disposing the machine also disposes the rest of the emulator.
  delete mach;

  return rc;
}
///

/// MainBreakPoint
// This is purely for debugging purposes
#if DEBUG_LEVEL > 0
void MainBreakPoint(void)
{
}
#endif
///

///
