/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: basicrom.cpp,v 1.31 2020/03/28 14:05:58 thor Exp $
 **
 ** In this module: Administration/loading of the Basic ROM
 **********************************************************************************/

/// Includes
#include "types.h"
#include "argparser.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"
#include "basicrom.hpp"
#include "basicmathpatch.hpp"
#include "basdist.hpp"
#include "cartridge.hpp"
#include "exceptions.hpp"
#include "cartrom.hpp"
#include "stdio.hpp"
///

/// BasicROM::BasicOffsets
// These are the entry points for the math functions
// of basic++ 1.07. Should it be recompiled,
// make sure the offsets are adjusted accordingly.
const ADR BasicROM::BasicOffsets[6] = {
  0xB5E9, // SQRT
  0xB42A, // POW
  0xB3F7, // INT
  0xB57A, // COS
  0xB571, // SIN
  0xB507  // ATAN
};
///

/// BasicROM::BasicROM
BasicROM::BasicROM(class Machine *mach)
  : Chip(mach,"BasicROM"), PatchProvider(mach), basic_type(Basic_Auto),
    basicapath(NULL), basicbpath(NULL), basiccpath(NULL),
    mppatch(false)
{
}
///

/// BasicROM::~BasicROM
BasicROM::~BasicROM(void)
{
  DisposePatches();
  
  delete[] basicapath;
  delete[] basicbpath;
  delete[] basiccpath;
}
///

/// BasicROM::PatchFromDump
// Patch the built-in basic from the hexdump into the rom.
void BasicROM::PatchFromDump(const unsigned char *dump,int pages)
{
  class RomPage *page = Rom;
  UBYTE offset;
  //
  do {
    offset = 0;
    do {
      page->PatchByte(offset,*dump);
      dump++,offset++;
    } while(offset);
    page++;
  } while(--pages);
}
///

/// BasicROM::CheckROMFile
// Check whether a given file is valid and contains a valid basic ROM
// Throws on error.
void BasicROM::CheckROMFile(const char *path)
{
  FILE *fp = NULL;

  if (path && *path) {
    try {
      fp = fopen(path,"rb");
      //
      // First check whether this is possibly a cartridge ROM. If
      // so, then check the header.
      if (fp) {
	char page[256];
	int pages = 32; // Number of pages in a basic ROM dump.
	bool withheader = false;;
	LONG size = 0;
	CartTypeId type;
	//
	type = Cart8K::GuessCartType(machine,fp,withheader,size);
	if (type != Cart_8K) {
	  throw AtariException("not a valid ROM file",
			       "BasicROM::CheckRomFile",
			       "The file %s is not an 8K ROM dump and hence not a valid Basic ROM image",path);
	}
	if (fseek(fp,0,SEEK_SET) == 0) {
	  do {
	    if (fread(page,1,256,fp) != 256) {
	      int err = errno;
	      if (err) {
		throw AtariException(strerror(err),"BasicROM::CheckROMFile","Unable to read Basic ROM file %s",path);
	      } else {
		throw AtariException("unexpected end of file","OsROM::CheckROMFile","Basic ROM file %s is too short",path);
	      }
	    }
	  } while(--pages);
	} else throw AtariException(strerror(errno),"BasicROM::CheckROMFile","Unable to rewind Basic ROM file %s",path);
      } else {
	throw AtariException(strerror(errno),"BasicROM::CheckROMFile","Unable to open Basic ROM file %s",path);
      }
      if (fp)
	fclose(fp);
    } catch(...) {
      if (fp)
	fclose(fp);
    }
  }
}
///

/// BasicROM::FindRomIn
// Check whether the Basic image can be found at the specified path. In case
// it can, return true and set the string variable to the given
// path. Return false otherwise.
bool BasicROM::FindRomIn(char *&rompath,const char *suggested)
{
  FILE *rom;
  
  // Try to open the ROM under the suggested name.
  rom = fopen(suggested,"rb");
  if (rom) {
    bool withheader = false;
    LONG size = 0;
    //
    // Check whether the type fits.
    if (Cart8K::GuessCartType(machine,rom,withheader,size) == Cart_8K) {
      fclose(rom);
      delete rompath;
      rompath = NULL;
      rompath = new char[strlen(suggested) + 1];
      strcpy(rompath,suggested);
      return true;
    }
    fclose(rom);
  }
  // Otherwise, we could either not seek, or could not find the file.
  // In either case, we bail out.
  return false;
}
///

/// BasicROM::BasicType
// Return the currently active Basic ROM type and return it.
BasicROM::BasicType BasicROM::ROMType(void) const
{
  BasicType type = basic_type;  
  //
  // First check whether the type is automatic. If so,
  // try to auto-detect the type.
  if (type == Basic_Auto) {
    // Detect the machine type and decide which Os to use.
    // For original machines, select None because basic came separately.
    switch(machine->MachType()) {
    case Mach_Atari800:
      type = Basic_Disabled;
      break;
    case Mach_Atari1200:
      // This machine also did not include a basic.
      type = Basic_Disabled;
      break;
    case Mach_AtariXL:
    case Mach_AtariXE:
      // These machines came either with Rev.B or Rev.C. Because Rev.B
      // is buggy, try RevC first.
      if (basiccpath && *basiccpath) {
	type = Basic_RevC;
      } else if (basicbpath && *basicbpath) {
	type = Basic_RevB;
      } else {
	type = Basic_Builtin;
      }
      break;
    case Mach_5200:
      // This machine did not have a basic.
      type = Basic_Disabled;
      break;
    default:
      Throw(InvalidParameter,"BasicROM::RomType","invalid or unknown machine type specified");
      break;
    }
  }
  return type;
}
///

/// BasicROM::LoadROM
// Load the selected ROM from disk
void BasicROM::LoadROM(void)
{
  BasicType type = ROMType();

  switch(type) {
  case Basic_RevA:
    // Ok, try to load Basic RevA.
    if (!basicapath) {
      if (!FindRomIn(basicapath,"roms/basica.rom")) {
	Throw(ObjectDoesntExist,"BasicROM::LoadROM",
	      "Path to Basic Rev.A ROM unspecified. This ROM is not available. "
	      "Pick a suitable ROM path in the BasicROM topic of the user menu");
      }
    }
    LoadFromFile(basicapath,"Basic Rev.A");
    break;
  case Basic_RevB:
    if (!basicbpath) {      
      if (!FindRomIn(basicbpath,"roms/basicb.rom")) {
	Throw(ObjectDoesntExist,"BasicROM::LoadROM",
	      "Path to Basic Rev.B ROM unspecified. This ROM is not available."
	      "Pick a suitable ROM path in the BasicROM topic of the user menu");
      }
    }
    LoadFromFile(basicbpath,"Basic Rev.B");
    break;
  case Basic_RevC:
    if (!basiccpath) {      
      if (!FindRomIn(basiccpath,"roms/basicc.rom")) {
	Throw(ObjectDoesntExist,"BasicROM::LoadROM",
	      "Path to Basic Rev.C ROM unspecified. This ROM is not available."
	      "Pick a suitable ROM path in the BasicROM topic of the user menu");
      }
    }
    LoadFromFile(basiccpath,"Basic Rev.C");
    break;
  case Basic_Builtin:
    // This is special as it doesn't require a source file.
    PatchFromDump(basdist,32);
    //
    // Potentially install the basic math patch
    if (mppatch)
      new class BasicMathPatch(machine,this,BasicOffsets);
    //
    break;
  case Basic_Disabled:
    // Nothing...
    break;
  default:
    Throw(InvalidParameter,"BasicROM::LoadROM","invalid Basic ROM type specified");
    break;
  }
}
///


/// BasicROM::LoadFromFile
// Load one or several pages from a file into the Basic ROM,
// return an indicator of whether the basic could be loaded from the
// given file name.
void BasicROM::LoadFromFile(const char *path,const char *name)
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
	throw AtariException("not a valid ROM file",
			     "BasicROM::LoadFromFile",
			     "The file %s for %s is not an 8K ROM dump and hence not a valid Basic ROM image",
			     path,name);
      }
      Cart8K::LoadFromFile(path,withheader);
      fclose(fp);
    } else {
      throw AtariException(strerror(errno),"BasicROM::LoadFromFile",
			   "Unable to open the source file %s for %s.",
			   path,name);
    }
  } catch(...) {
    // Dispose the file pointer, at least.
    if (fp)
      fclose(fp);
    throw;
  }     
}
///

/// BasicROM::PatchByte
// Patch a byte of the ROM image.
void BasicROM::PatchByte(ADR where,UBYTE value)
{
  if (where >= 0xa000 && where < 0xc000) {
    int page = (where - 0xa000) >> 8;
    
    Rom[page].PatchByte(where,value);
  }
}
///

/// BasicROM::Initialize
// Pre-Coldstart-Phase: Load the selected ROM
// then install the patches.
void BasicROM::Initialize(void)
{
  DisposePatches();
  
  try {
    LoadROM();
    //
    // Now allocate the ESC codes and hack the patches in.
    // Since the MMU does not map basic at this time,
    // create a private address space, map the cart in there
    // and then install the patches there.
    // The address space can go away as soon as we are done.
    if (mppatch) {
      class AdrSpace adr;
      //
      for(int i = 0;i < 32;i++) {
	adr.MapPage(0xa000 + (i << 8),Rom + i);
      }
      InstallPatchList(&adr);
    }
    //
  } catch(...) {
    basic_type = Basic_Disabled;
    throw;
  }
}
///

/// BasicROM::WarmStart
// Run a warmstart. Nothing happens here
void BasicROM::WarmStart(void)
{
  // This is the reset method of the patch provider.
  Reset();
}
///

/// BasicROM::ColdStart
// Nothing happens here.
void BasicROM::ColdStart(void)
{
  Reset();
}
///

/// BasicROM::ParseArgs
// The argument parser: Pull off arguments specific to this class.
void BasicROM::ParseArgs(class ArgParser *args)
{
 static const struct ArgParser::SelectionVector basictypevector[] = 
    { {"Auto"    ,Basic_Auto    },
      {"RevA"    ,Basic_RevA    },
      {"RevB"    ,Basic_RevB    },
      {"RevC"    ,Basic_RevC    },
      {"BuiltIn" ,Basic_Builtin },
      {"Disabled",Basic_Disabled},
      {NULL    ,0}
    };
 LONG basictype = basic_type;
 bool oldmpp    = mppatch;

 if (machine->MachType() != Mach_5200) {
   args->DefineTitle("Basic ROM");
   args->DefineFile("BasicAPath","path to Basic Rev.A image",basicapath,false,true,false);
   args->DefineFile("BasicBPath","path to Basic Rev.B image",basicbpath,false,true,false);
   args->DefineFile("BasicCPath","path to Basic Rev.C image",basiccpath,false,true,false);
   args->DefineSelection("BasicType","Basic type to use",basictypevector,basictype);
   //
   if (basictype != basic_type)
     args->SignalBigChange(ArgParser::ColdStart);
   basic_type = (BasicType)basictype;
#if HAVE_MATH
   if (basic_type == Basic_Builtin)
     args->DefineBool("InstallMathPatch","install fast math pack patch",mppatch);
   if (mppatch != oldmpp)
     args->SignalBigChange(ArgParser::ColdStart);
#endif   
   //
   // Now check whether the requirements for the selection are satisfied.
   switch(ROMType()) {
   case Basic_RevA:
     if (basicapath == NULL || *basicapath == 0) {
      args->PrintError("Basic Rev.A selected, but BasicAPath not given. "
		       "Please pick a suitable Basic ROM path in the BasicROM topic of the user menu "
		       "and save the changes.");
     }
     CheckROMFile(basicapath);
     break;
   case Basic_RevB:
     if (basicbpath == NULL || *basicbpath == 0) {
       args->PrintError("Basic Rev.B selected, but BasicBPath not given. "
			"Please pick a suitable Basic ROM path in the BasicROM topic of the user menu "
			"and save the changes.");
     }
     CheckROMFile(basicbpath);
     break;
   case Basic_RevC:
     if (basiccpath == NULL || *basiccpath == 0) {
       args->PrintError("Basic Rev.C selected, but BasicCPath not given. "
			"Please pick a suitable Basic ROM path in the BasicROM topic of the user menu "
			"and save the changes.");
     }
     CheckROMFile(basiccpath);
     break;
   default:
     // Built-in and disabled do not require checks. Auto is never returned by RomType()
     break;
   }
 }
}
///

/// BasicROM::DisplayStatus
void BasicROM::DisplayStatus(class Monitor *mon)
{
  const char *type;
  
  switch(ROMType()) {
  case Basic_RevA:
    type = "Basic Rev.A";
    break;
  case Basic_RevB:
    type = "Basic Rev.B";
    break;
  case Basic_RevC:
    type = "Basic Rev.C";
    break;
  case Basic_Builtin:
    type = "Built-In";
    break;
  case Basic_Disabled:
    type = "Disabled";
    break;
  default:
    type = "(invalid)"; // this should not happen.
    break;
  }
  
  mon->PrintStatus("BasicROM Status:\n"
		   "\tBasic Type    : %s\n"
		   "\tBasicAPath    : %s\n"
		   "\tBasicBPath    : %s\n"
		   "\tBasicCPath    : %s\n"
		   "\tMathPackPatch : %s\n",
		   type,
		   basicapath,
		   basicbpath,
		   basiccpath,
		   (mppatch)?("on"):("off"));
}
///
