/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: osrom.cpp,v 1.53 2020/03/28 14:05:58 thor Exp $
 **
 ** In this module: Administration/loading of Os ROMs
 **********************************************************************************/

/// Includes
#include "argparser.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"
#include "osrom.hpp"
#include "patch.hpp"
#include "siopatch.hpp"
#include "deviceadapter.hpp"
#include "hdevice.hpp"
#include "edevice.hpp"
#include "pdevice.hpp"
#include "rdevice.hpp"
#include "mathpackpatch.hpp"
#include "romxlchecksum.hpp"
#include "exceptions.hpp"
#include "osdist.hpp"
#include "mmu.hpp"
#include "new.hpp"
#include "stdio.hpp"
///

/// OsROM::OsROM
OsROM::OsROM(class Machine *mach)
  : Chip(mach,"OsROM"), PatchProvider(mach)
{
  cpu        = NULL;
  cpuram     = NULL;
  devices    = NULL;

  siopatch   = true;
  ppatch     = true;
  hpatch     = true;
  epatch     = false;
  rpatch     = false;
  hAsd       = false;
  mppatch    = false;

  osapath    = NULL;
  osbpath    = NULL;
  osxlpath   = NULL;
  os1200path = NULL;
  os5200path = NULL;
  os_type    = Os_Auto;

  hdir[0]    = NULL;
  hdir[1]    = NULL;
  hdir[2]    = NULL;
  hdir[3]    = NULL;
}
///

/// OsROM::~OsROM
OsROM::~OsROM(void)
{
  PatchProvider::DisposePatches();
  
  delete[] osapath;
  delete[] osbpath;
  delete[] os1200path;
  delete[] osxlpath;
  delete[] os5200path;

  delete[] hdir[0];
  delete[] hdir[1];
  delete[] hdir[2];
  delete[] hdir[3];
}
///

/// OsROM::LoadFromFile
// Load one or several pages from a file into the Os ROM
int OsROM::LoadFromFile(const char *path,int pages)
{
  FILE *fp;
  int rc = 0;
  class RomPage *page = rom;

  fp = fopen(path,"rb");
  if (fp) {
    do {
      if (!page->ReadFromFile(fp))
	break;
      page++;
    } while(--pages);
    if (pages) rc = errno;
    fclose(fp);
    return rc;
  }
  return errno;
}
///

/// OsRom::CheckRomFile
// Check the given OS rom, must contain the given number of pages.
// Throws on error.
void OsROM::CheckROMFile(const char *path,int pages)
{ 
  if (path && *path) {
    FILE *fp;
    fp = fopen(path,"rb");
    if (fp) {
      char page[256];
      do {
	if (fread(page,1,256,fp) != 256) {
	  int err = errno;
	  fclose(fp);
	  if (err) {
	    throw AtariException(strerror(err),"OsROM::CheckROMFile","Unable to read ROM file %s",path);
	  } else {
	    throw AtariException("unexpected end of file","OsROM::CheckROMFile","ROM file %s is too short",path);
	  }
	}
      } while(--pages);
      fclose(fp);
    } else {
      throw AtariException(strerror(errno),"OsROM::CheckROMFile","Unable to open ROM file %s",path);
    }
  }
}
///

/// OsROM::PatchFromDump
// Special service for the built-in ROM: Patch the ROM contents
// from a static image
void OsROM::PatchFromDump(const unsigned char *dump,int pages)
{
  class RomPage *page = rom;
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

/// OsROM::KillMathPack
// Erase the math pack, replace by HLT instructions.
void OsROM::KillMathPack(void)
{
  class RomPage *page = rom + ((0xd800 - 0xc000) >> PAGE_SHIFT);
  int pages           = (0xe000 - 0xd800) >> PAGE_SHIFT;
  UBYTE offset;
  //
  do {
    offset = 0;
    do {
      page->PatchByte(offset,0x02); // Patch in a HLT instruction
      offset++;
    } while(offset);
    page++;
  } while(--pages);
}
///

/// OsROM::FindRomIn
// Check whether the ROM image can be found at the specified path. In case
// it can, return true and set the string variable to the given
// path. Return false otherwise.
bool OsROM::FindRomIn(char *&rompath,const char *suggested,UWORD requiredsize)
{
  FILE *rom;

  // Try to open the ROM under the suggested name.
  rom = fopen(suggested,"rb");
  if (rom) {
    // Fine then. Now check whether it has the proper size.
    if (fseek(rom,0,SEEK_END) == 0) {
      // Now check the current position: This is at the EOF, and
      // hence must be the size of the file
      if (ftell(rom) == (LONG)requiredsize) {
	// Yup, that fits.
	fclose(rom);
	delete rompath;
	rompath = NULL;
	rompath = new char[strlen(suggested) + 1];
	strcpy(rompath,suggested);
	return true;
      }
    }
    fclose(rom);
  }
  // Otherwise, we could either not seek, or could not find the file.
  // In either case, we bail out.
  return false;
}
///

/// OsROM::RomType
// Return the currently active ROM type and return it.
OsROM::OsType OsROM::RomType(void) const
{
  OsType type = os_type;  
  //
  // First check whether the type is automatic. If so,
  // try to auto-detect the type.
  if (type == Os_Auto) {
    // Detect the machine type and decide which Os to use.
    switch(machine->MachType()) {
    case Mach_Atari800:
      if (osapath && *osapath) {
	type = Os_RomA;
      } else {
	type = Os_RomB;
      }
      break;
    case Mach_Atari1200:
      type = Os_Rom1200;
      break;
    case Mach_AtariXL:
    case Mach_AtariXE:
      if (osxlpath && *osxlpath) {
	type = Os_RomXL;
      } else {
	type = Os_Builtin;
      }
      break;
    case Mach_5200:
      type = Os_5200;
      break;
    default:
      Throw(InvalidParameter,"OsROM::RomType","invalid or unknown machine type specified");
      break;
    }
  }
  return type;
}
///

/// OsROM::LoadROM
// Load the selected ROM from disk
void OsROM::LoadROM(void)
{
  int error; 
  OsType type = RomType();

  switch(type) {
  case Os_RomA:
    // Ok, try to load the OsA ROM.
    if (!osapath) {
      if (!FindRomIn(osapath,"roms/atariosa.rom",10240)) {
	Throw(ObjectDoesntExist,"OsROM::LoadROM",
	      "Path to OsA ROM unspecified. This ROM is not available. "
	      "Pick a suitable ROM path in the OsROM topic of the user menu");
      }
    }
    if ((error = LoadFromFile(osapath,40))) 
      throw(AtariException(strerror(error),"OsROM::LoadROM",
			   "Failed to load OsA ROM from %s.",osapath));
    break;
  case Os_RomB:
    if (!osbpath) {      
      if (!FindRomIn(osbpath,"roms/atariosb.rom",10240)) {
	Throw(ObjectDoesntExist,"OsROM::LoadROM",
	      "Path to OsB ROM unspecified. This ROM is not available."
	      "Pick a suitable ROM path in the OsROM topic of the user menu");
      }
    }
    if ((error = LoadFromFile(osbpath,40)))
      throw(AtariException(strerror(error),"OsROM::LoadROM",
			   "Failed to load OsB ROM from %s.",osbpath));
    break;
  case Os_Rom1200:
    if (!os1200path) {      
      if (!FindRomIn(osbpath,"roms/atari1200.rom",16384)) {
	Throw(ObjectDoesntExist,"OsROM::LoadROM",
	      "Path to Atari 1200 XL Os ROM unspecified. This ROM is not available."
	      "Pick a suitable ROM path in the OsROM topic of the user menu");
      }
    }
    if ((error = LoadFromFile(os1200path,64)))
      throw(AtariException(strerror(error),"OsROM::LoadROM",
			   "Failed to load Atari 1200 XL ROM from %s.",os1200path));
    break;
  case Os_RomXL:
    if (!osxlpath) {      
      if (!FindRomIn(osbpath,"roms/atarixl.rom",16384)) {
	Throw(ObjectDoesntExist,"OsROM::LoadROM",
	      "Path to OsXL ROM unspecified. This ROM is not available."
	      "Pick a suitable ROM path in the OsROM topic of the user menu");
      }
    }
    if ((error = LoadFromFile(osxlpath,64)))
      throw(AtariException(strerror(error),"OsROM::LoadROM",
			   "Failed to load OsXL ROM from %s.",osxlpath));
    break;
  case Os_5200:
    if (!os5200path) {      
      Throw(ObjectDoesntExist,"OsROM::LoadROM",
	    "Path to Os5200 ROM unspecified. This ROM is not available."
	    "Pick a suitable ROM path in the OsROM topic of the user menu");
    } else {
      if ((error = LoadFromFile(os5200path,8)))
	throw(AtariException(strerror(error),"OsROM::LoadROM",
			     "Failed to load Os5200 ROM from %s.",os5200path));
    }
    break;
  case Os_Builtin:
    // This is special as it doesn't require a source file.
    PatchFromDump(osdist,64);
    //KillMathPack();
    break;
  default:
    Throw(InvalidParameter,"OsROM::LoadROM","invalid Os ROM type specified");
    break;
  }
}
///

/// OsROM::Initialize
// Pre-Coldstart-Phase: Load the selected ROM
// then install the patches.
void OsROM::Initialize(void)
{
  devices = NULL; // The old one is part of the patch list and thus now gone.
  DisposePatches();
  LoadROM();

  if (machine->MachType() != Mach_5200) {
    // Install a new device adapter. Device patches will go
    // into this guy here.
    if (ppatch || hpatch || epatch || rpatch)
      devices = new class DeviceAdapter(machine,this);
    //
    // We only have to construct the patches. They link itself into
    // the patch provider and stay there, ready to go.
    if (siopatch) 
      new class SIOPatch(machine,this,machine->SIO());
    
    if (ppatch)
      new class PDevice(machine,this);
    
    if (rpatch)
      new class RDevice(machine,this);

    //
    // This must go in last because it modifies the HATABS init
    // table that is searched for by all other devices.
    if (hpatch)
      new class HDevice(machine,this,hdir,hAsd?('D'):('H'));

    if (epatch) {
      new class EDevice(machine,this,'E');
      new class EDevice(machine,this,'K');
    }

    if (mppatch)
      new class MathPackPatch(machine,this);
    //
    // We must adjust the checksum in case:
    // a) we installed any patch and there is a rom checksum 
    // or
    // b) the Rom is the built-in ROM that requires the patch without
    // asking for it.
    if (RomType() == Os_Builtin || 
	((siopatch || ppatch || hpatch || epatch || rpatch || mppatch) && 
	 (RomType() == Os_RomXL || RomType() == Os_Builtin || RomType() == Os_Rom1200)))
      new class RomXLChecksum(machine,this);
    
    // Now allocate the ESC codes and hack the patches in.
    InstallPatchList();
  }
}
///

/// OsROM::WarmStart
// Run a warmstart. Nothing happens here
void OsROM::WarmStart(void)
{
  // Reset the patches we provide.
  Reset();
}
///

/// OsROM::ColdStart
// Run the Coldstart. Just get all the links.
void OsROM::ColdStart(void)
{
  cpu    = machine->CPU();
  mmu    = machine->MMU();
  cpuram = mmu->CPURAM();
  Reset();
}
///

/// OsROM::ParseArgs
// The argument parser: Pull off arguments specific to this class.
void OsROM::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector ostypevector[] = 
    { {"Auto"   ,Os_Auto   },
      {"OsA"    ,Os_RomA   },
      {"OsB"    ,Os_RomB   },
      {"Os1200" ,Os_Rom1200},
      {"OsXL"   ,Os_RomXL  },
      {"BuiltIn",Os_Builtin},
      {NULL    ,0}
    };
  LONG ostype;
  bool oldspatch  = siopatch;
  bool oldppatch  = ppatch;
  bool oldhpatch  = hpatch;
  bool oldepatch  = epatch;
  bool oldrpatch  = rpatch;
  bool oldhasd    = hAsd;
  bool oldmppatch = mppatch;
  
  ostype = os_type;
  args->DefineTitle("OsROM");
  args->OpenSubItem("Os");
  args->DefineFile("OsAPath",   "path to Os revision A ROM image",osapath,false,true,false);
  args->DefineFile("OsBPath",   "path to Os revivion B ROM image",osbpath,false,true,false);
  args->DefineFile("Os1200Path","path to Atari 1200XL ROM image",os1200path,false,true,false);
  args->DefineFile("OsXLPath",  "path to OsXL image",osxlpath,false,true,false);
  args->DefineFile("Os5200Path","path to 5200 image",os5200path,false,true,false);
  if (machine->MachType() == Mach_5200) {
    ostype = Os_5200; // No other chance
  } else {
    if (ostype == Os_5200)
      ostype = Os_Auto;
    args->DefineSelection("OsType","Os type to use",ostypevector,ostype);
  }
  if (ostype != Os_5200) {
    int i;
    args->DefineFile("H1Dir","path to the H1 handler directory",hdir[0],false,false,true);
    args->DefineFile("H2Dir","path to the H2 handler directory",hdir[1],false,false,true);
    args->DefineFile("H3Dir","path to the H3 handler directory",hdir[2],false,false,true);
    args->DefineFile("H4Dir","path to the H4 handler directory",hdir[3],false,false,true);
    args->DefineBool("SIOPatch","install SIO speedup os patch",siopatch);
    args->DefineBool("InstallPDevice","install P: CIO patch",ppatch);
    args->DefineBool("InstallHDevice","install H: CIO patch",hpatch);
    args->DefineBool("InstallEDevice","install E: CIO patch",epatch);
    args->DefineBool("InstallRDevice","install R: CIO patch",rpatch);
    args->DefineBool("InstallHAsDisk","install host handler as D: device",hAsd); 
#if HAVE_MATH
    args->DefineBool("InstallMathPatch","install fast math pack patch",mppatch);
#endif

    // Fix up the HDevice directories a bit.
    for(i=0;i<4;i++) {
      if (hdir[i]) {
	size_t len;
	// Fine. We have a base directory.
	// Check whether the base directory ends on a slash. If so, remove it because
	// we assume that we must add it.
	len = strlen(hdir[i]);
	if (len > 0 && hdir[i][len-1] == '/')
	  hdir[i][len-1] = '\0'; 
      } else {
	hdir[i] = new char[2];
	strcpy(hdir[i],".");
      }
    }
  }
  if (os_type    != (OsROM::OsType)ostype  || 
      oldspatch  != siopatch               ||
      oldppatch  != ppatch                 ||
      oldhpatch  != hpatch                 ||
      oldepatch  != epatch                 ||
      oldrpatch  != rpatch                 ||
      oldhasd    != hAsd                   ||
      oldmppatch != mppatch) {
    args->SignalBigChange(ArgParser::ColdStart);
  }
  os_type = (OsROM::OsType)ostype;


  switch(RomType()) {
  case Os_RomA:    
    if (osapath == NULL || *osapath == 0) {
      args->PrintError("OsA selected, but OsAPath not given. "
		       "Please pick a suitable ROM path in the OsROM topic of the user menu "
		       "and save the changes.");
    }
    CheckROMFile(osapath,40);
    break;
  case Os_RomB:    
    if (osbpath == NULL || *osbpath == 0) {
      args->PrintError("OsB selected, but OsBPath not given. "
		       "Please pick a suitable ROM path in the OsROM topic of the user menu "
		       "and save the changes.");
    }
    CheckROMFile(osbpath,40);
    break;
  case Os_Rom1200:
    if (os1200path == NULL || *os1200path == 0) {
      args->PrintError("Atari 1200 XL Os selected, but Os1200Path not given. "
		       "Please pick a suitable ROM path in the OsROM topic of the user menu "
		       "and save the changes.");
    }
    CheckROMFile(os1200path,64);
    break;
  case Os_RomXL:
    if (osxlpath == NULL || *osxlpath == 0) {
      args->PrintError("OsXL selected, but OsXLPath not given. "
		       "Please pick a suitable ROM path in the OsROM topic of the user menu "
		       "and save the changes.");
    }
    CheckROMFile(osxlpath,64);
    break;
  case Os_5200:
    if (os5200path == NULL || *os5200path == 0) {
      args->PrintError("Os5200 selected, but Os5200Path not given. "
		       "Please pick a suitable ROM path in the OsROM topic of the user menu "
		       "and save the changes.");
    }
    CheckROMFile(os5200path,8);
    break;
  case Os_Builtin:
    break;
  default:
    os_type = Os_Auto;
    Throw(InvalidParameter,"OsROM::ParseArgs","found invalid ROM type active");
  }
  args->CloseSubItem();
}
///

/// OsROM::DisplayStatus
void OsROM::DisplayStatus(class Monitor *mon)
{
  const char *osname;
  
  switch (os_type) {
  case Os_RomA:
    osname = "OsA";
    break;
  case Os_RomB:
    osname = "OsB";
    break;
  case Os_Rom1200:
    osname = "Os1200";
    break;
  case Os_RomXL:
    osname = "OsXL";
    break;
  case Os_Builtin:
    osname = "Builtin";
    break;
  default:
    osname = "unknown";
  }

  mon->PrintStatus("OsROM Status:\n"
		   "\tSIOPatch:       %s\n"
		   "\tInstallPDevice: %s\tInstallHDevice: %s"
		   "\tInstallEDevice: %s\tInstallRDevice: %s\n"
		   "\tInstallHAsDHandler: %s\tMathPackPatch: %s\n"
		   "\tOsType        : %s\n"
		   "\tOsAPath       : %s\n"
		   "\tOsBPath       : %s\n"
		   "\tOs1200Path    : %s\n"
		   "\tOsXLPath      : %s\n"
		   "\tH1 directory  : %s\n"
		   "\tH2 directory  : %s\n"
		   "\tH3 directory  : %s\n"
		   "\tH4 directory  : %s\n",
		   (siopatch)?("on"):("off"),
		   (ppatch)?("on"):("off"),
		   (hpatch)?("on"):("off"),
		   (epatch)?("on"):("off"),
		   (rpatch)?("on"):("off"),
		   (hAsd)?("on"):("off"),
		   (mppatch)?("on"):("off"),
		   osname,
		   (osapath)?(osapath):("(none)"),
		   (osbpath)?(osbpath):("(none)"),
		   (os1200path)?(os1200path):("(none)"),
		   (osxlpath)?(osxlpath):("(none)"),
		   (hdir[0])?(hdir[0]):("(none)"),
		   (hdir[1])?(hdir[1]):("(none)"),
		   (hdir[2])?(hdir[2]):("(none)"),
		   (hdir[3])?(hdir[3]):("(none)")
		   );
}
///

/// OsROM::MightColdstart
// Check whether a following reset might cause a full
// reset. If so, then returns true.
bool OsROM::MightColdstart(void)
{
  bool cold           = false;
  class AdrSpace *ram = machine->MMU()->CPURAM();
  //
  // Try to guess whether this is a coldstart, as
  // a service for the keyboard handler.
  // The reason why we're performing this complicated
  // here is that we should not try to overload the
  // console keys on a warmstart since a program might
  // want to read it here, but we also want a 
  // consistent behaivour in case a simple reset
  // generates a coldstart.
  switch(RomType()) {
  case OsROM::Os_RomA:
  case OsROM::Os_RomB:  
    if (ram->ReadByte(0x244)) {
      // This will end up as a coldstart, prepare
      // to re-read the console keys.
      cold = true;
    }
    break;
  case OsROM::Os_5200:
    // No further checks
    cold = true;
    break;
  case OsROM::Os_RomXL:
  case OsROM::Os_Rom1200:
  case OsROM::Os_Builtin:
    // More checks to be done here.
    if (ram->ReadByte(0x244)         ||
	ram->ReadByte(0x33d) != 0x5c ||
	ram->ReadByte(0x33e) != 0x93 ||
	ram->ReadByte(0x33f) != 0x25 ||
	ram->ReadByte(0x3fa) != ram->ReadByte(0xd013)) {
      // also a coldstart.
      cold = true;
    }
    break;
  case OsROM::Os_Auto:
    // never returned by the above.
    break;
  }
  //
  return cold;
}
///
