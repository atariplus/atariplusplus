/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: basicrom.cpp,v 1.21 2006-07-08 11:19:08 thor Exp $
 **
 ** In this module: Administration/loading of the Basic ROM
 **********************************************************************************/

/// Includes
#include "argparser.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"
#include "basicrom.hpp"
#include "cartridge.hpp"
#include "exceptions.hpp"
#include "cartrom.hpp"
#include "stdio.hpp"
///

/// BasicROM::BasicROM
BasicROM::BasicROM(class Machine *mach)
  : Chip(mach,"BasicROM"), basicpath(NULL), currentpath(NULL)
{
  loaded    = false;
  usebasic  = true;
}
///

/// BasicROM::~BasicROM
BasicROM::~BasicROM(void)
{
  delete[] basicpath;
  delete[] currentpath;
}
///

/// BasicROM::LoadFromFile
// Load one or several pages from a file into the Basic ROM
void BasicROM::LoadFromFile(const char *path)
{
  FILE *fp = NULL;
  CartTypeId type;
  bool withheader;
  LONG size       = 0;

  try {
    fp = fopen(path,"rb");
    //
    // First check whether this is possibly a cartridge ROM. If
    // so, then check the header.
    if (fp) {
      type = Cart8K::GuessCartType(machine,fp,withheader,size);
      if (type != Cart_8K) {
	machine->PutWarning("The given file %s does not contain a valid Basic ROM image.",path);      
	usebasic = false;
	loaded   = false;
      } else {
	Cart8K::LoadFromFile(path,withheader);
	loaded   = true;
      }
    } else {
      machine->PutWarning("Could not load basic ROM from %s, disabling it: %s\n",
			  path,strerror(errno));    
      usebasic = false;
      loaded   = false;
    }
  } catch(...) {
    // Dispose the file pointer, at least.      
    usebasic  = false;
    loaded    = false;
    machine->PutWarning("Could not load basic ROM from %s, disabling it.",
			path);
  }     
  if (fp)
    fclose(fp);
}
///

/// BasicROM::Initialize
// Pre-Coldstart-Phase: Load the selected ROM
// then install the patches.
void BasicROM::Initialize(void)
{
  if (machine->MachType() != Mach_5200) {
    // We just try to loaded the basic.
    // We do not return an error if we can't. We just
    // have to say so the MMU.
    if (basicpath && *basicpath && usebasic) {
      LoadFromFile(basicpath);
      loaded     = true;
    } else {
      usebasic   = false;
    }
  }
}
///

/// BasicROM::WarmStart
// Run a warmstart. Nothing happens here
void BasicROM::WarmStart(void)
{
}
///

/// BasicROM::ColdStart
// Nothing happens here.
void BasicROM::ColdStart(void)
{
}
///

/// BasicROM::ParseArgs
// The argument parser: Pull off arguments specific to this class.
void BasicROM::ParseArgs(class ArgParser *args)
{
  bool olduse = usebasic;
  
  if (machine->MachType() != Mach_5200) {
    args->DefineTitle("Basic ROM");
    args->DefineFile("BasicPath","path to Basic ROM image",basicpath,false,true,false);
    args->DefineBool("UseBasic","enable the Basic ROM",usebasic);
    if (olduse != usebasic || (basicpath && currentpath && strcmp(basicpath,currentpath))) {
      loaded = false;
      delete[] currentpath;
      currentpath = new char[strlen(basicpath) + 1];
      strcpy(currentpath,basicpath);
      args->SignalBigChange(ArgParser::ColdStart);
    }
    // Try wether the basic ROM is ok.
    if (basicpath && *basicpath && usebasic) {
      FILE *fp = NULL;
      CartTypeId type;
      LONG size;
      bool withheader;
      //
      // Enclose in try/catch to close the file on error.
      try {
	fp = fopen(basicpath,"rb");
	//
	// First check whether this is possibly a cartridge ROM. If
	// so, then check the header.
	if (fp) {
	  type = Cart8K::GuessCartType(machine,fp,withheader,size);
	  if (type != Cart_8K) {
	    usebasic = false;
	    args->SignalBigChange(ArgParser::ColdStart);
	    args->PrintError("The given file %s does not contain a valid Basic ROM image.",basicpath);      
	  }
	  fclose(fp);
	} else {
	  usebasic = false;	  
	  args->SignalBigChange(ArgParser::ColdStart);
	  args->PrintError("Unable to open the Basic ROM image at %s.",basicpath);
	}
      } catch(const class AtariException &exc) {
	if (fp)
	  fclose(fp);
	throw exc;
      } catch(...) {
	if (fp)
	  fclose(fp);
      }
    }
  }
}
///

/// BasicROM::DisplayStatus
void BasicROM::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("BasicROM Status:\n"
		   "\tUse Basic     : %s\n"
		   "\tBasicPath     : %s\n"
		   "\tBasic is      : %s\n",
		   (usebasic)?("on"):("off"),
		   basicpath,
		   (loaded)?("loaded"):("not loaded")
		   );
}
///
