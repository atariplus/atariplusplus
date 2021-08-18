/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mmu.cpp,v 1.57 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Definition of all MMU functions required for the Atari emulator
 **********************************************************************************/

/// Includes
#include "mmu.hpp"
#include "exceptions.hpp"
#include "monitor.hpp"
#include "rompage.hpp"
#include "rampage.hpp"
#include "gtia.hpp"
#include "pokey.hpp"
#include "pia.hpp"
#include "antic.hpp"
#include "cartctrl.hpp"
#include "snapshot.hpp"
#include "cartrom.hpp"
#include "basicrom.hpp"
#include "osrom.hpp"
#include "cartridge.hpp"
#include "new.hpp"
#include "string.hpp"
#include "ramextension.hpp"
#include "xeextension.hpp"
#include "axlonextension.hpp"
///

/// MMU::MMU Constructor
MMU::MMU(class Machine *machine)
  : Chip(machine,"MMU"), Saveable(machine,"MMU"),
    blank(new class RomPage[32]), handlers(new class RomPage),
    cpuspace(new class AdrSpace), anticspace(new class AdrSpace), 
    debugspace(new DebugAdrSpace(machine,cpuspace)),
    xeram(NULL), axlonram(NULL)
{
  // Set default mapping by defining various flags:
  // The default machine is an 800XL (hence, mine!)
  basic_mapped    = true;  // option not pressed
  rom_disabled    = false;
  selftest_mapped = false;
  mathpack_disable= false;
  extended_4K     = false;
  axlon           = false;
  //
  // Clear the blank page out now.
  blank->Blank();
}
///

/// MMU::~MMU Destructor
MMU::~MMU(void)
{
  delete xeram;
  delete axlonram;
  delete[] blank;
  delete handlers;
  delete debugspace;
  delete cpuspace;
  delete anticspace;
}
///

/// MMU::ColdStart
void MMU::ColdStart(void)
{
  class RamExtension *ext;
  // Forward the cold start to all RAM extensions.
  for(ext = Extensions.First();ext;ext = ext->NextOf()) {
    ext->ColdStart();
  }
  WarmStart();
}
///

/// MMU::WarmStart
void MMU::WarmStart(void)
{  
  class RamExtension *ext;
  // First reset the RAM extensions to get the default
  // mapping for them.
  for(ext = Extensions.First();ext;ext = ext->NextOf()) {
    ext->WarmStart();
  }
  // Build mapping 0x0000 to 0x4000 RAM
  BuildLowRam();
  // Build the mapping from 0x4000 to 0x8000
  BuildMedRam();
  // Build the mapping from 0x8000 to 0xc000
  BuildCartArea();
  //
  // Build all areas except selftest that
  // could be mapped by ROM. This is 0xc000 to 0xd000
  // and 0xd800 to 0x10000
  BuildOsArea();
}
///

/// MMU::BuildLowRam
// Build mapping 0x0000 to 0x4000 RAM
void MMU::BuildLowRam(void)
{
  class RamPage *rp = ram->RamPages();
  ADR i;
  
  // Map the zero page up to 0x4000 to plain RAM.
  for(i=0x0000;i<0x4000;i+=Page::Page_Length) {
    MapPage(i,rp + (i >> Page::Page_Shift));
  }
}
///

/// MMU::BuildMedRam
// Build the mapping from 0x4000 to 0x8000
void MMU::BuildMedRam(void)
{
  class RamPage *rp = ram->RamPages();
  ADR i;
  //
  // The 5200 does not have any memory in this area,
  // and neither extended RAM.
  if (machine->MachType() != Mach_5200) {
    bool mapped;  
    class RamExtension *ext;
    // Addresses 0x4000 to 0x8000 are mapped by 
    // various RAM extensions, including the 130XE page.
    // This does not go for the 5200 which doesn't allow
    // RAM extensions (at least not here, but which game
    // would use it?)
    //
    // First map the CPU.
    for(ext = Extensions.First(),mapped = false;ext;ext = ext->NextOf()) {
      if ((mapped = ext->MapExtension(cpuspace,false)))
	break;
    }
    if (!mapped) {
      // RAM disk not active. Map conservative RAM here.
      for(i=0x4000;i<0x8000;i+=Page::Page_Length) {
	MapCPUPage(i,rp + (i >> Page::Page_Shift));
      }
    }
    //
    // And the same again for antic.
    for(ext = Extensions.First(),mapped = false;ext;ext = ext->NextOf()) {
      if ((mapped = ext->MapExtension(anticspace,true)))
	break;
    }
    if (!mapped) {
      // RAM disk not active. Map conservative RAM here.
      for(i=0x4000;i<0x8000;i+=Page::Page_Length) {
	MapANTICPage(i,rp + (i >> Page::Page_Shift));
      }
    }
    //
    // Furthermore, 0x5000 to 0x5800 may contain the self test ROM.
    // This is mirrored from 0xd000 to 0xd800 in the ROM area. This
    // overlays the RAM disks and regular RAM.
    if (selftest_mapped && (osrom->RomType() == OsROM::Os_RomXL   || 
			    osrom->RomType() == OsROM::Os_Builtin ||
			    osrom->RomType() == OsROM::Os_Rom1200)) {
      class RomPage *rom = osrom->OsPages();
      for(i=0x5000;i<0x5800;i+=Page::Page_Length) {
	MapPage(i,rom + (16 + ((i - 0x5000) >> Page::Page_Shift)));
      }
    }
  }
}
///

/// MMU::BuildCartArea
// Build the mapping from 0x8000 to 0xc000
void MMU::BuildCartArea(void)
{
  class Cartridge *cart;
  class RamPage *rp;
  bool havebasic;
  ADR i;
  //
  // Get the type of the cartridge that is currently inserted
  havebasic = basicrom->BasicLoaded();
  cart      = cartrom->Cart();
  rp        = ram->RamPages();
  //
  if (!havebasic)
    basic_mapped = false;
  // The 5200 doesn't have any memory here if no cart is inserted
  // By default, map the area from 0x8000 to 0xc000 as RAM or blank. Insert this
  // mapping first, then modify later.
  if (machine->MachType() == Mach_5200) {
    for(i=0x8000;i<0xc000;i+=Page::Page_Length) {
      MapPage(i,blank);
    }
  } else {
    for(i=0x8000;i<0xc000;i+=Page::Page_Length) {
      MapPage(i,rp + (i>>Page::Page_Shift));
    }
  }
  //
  // Map in the basic
  // Must map basic or RAM.
  if (basic_mapped) {
    basicrom->MapBasic(this);
  }
  //
  // Perform the mapping within the cartridge.
  // Several carts might be active at a time since
  // the A800 has two slots.
  while(cart) {
    cart->MapCart(this);
    cart = cart->NextOf();
  }
}
///

/// MMU::BuildOsArea
// Build all areas except selftest that
// could be mapped by ROM. This is 0xc000 to 0xd000
// and 0xd800 to 0x10000
void MMU::BuildOsArea(void)
{
  class RamPage *rp;
  class RomPage *rom;
  class Page    *cfpage; // page from 0xcf00 to 0xcfff that might be hidden by Axlon RAM Disk Control
  ADR i;
  //  
  // Get the Os ROM area.
  rom = osrom->OsPages();
  rp  = ram->RamPages();
  //
  // Check whether we have a 5200. This has a 1K ROM from 0xf800 and up
  if (machine->MachType() == Mach_5200) {
    for(i=0xf800;i<0x10000;i+=Page::Page_Length) {
      MapPage(i,rom + ((i-0xf800)>>Page::Page_Shift));
    }
    //
    // No axlon RAM disk support here, obviously.
  } else {
    class RamExtension *ext;
    // 0xc000 to 0xd000 is either Os ROM, or RAM on extended 52K 800's
    // or blank, or RAM if the Os is disabled.      
    //
    // The default is RAM here, unless the Os overlays it.
    cfpage = rp + (0xcf00 >> Page::Page_Shift);
    //
    if (rom_disabled) {
      for(i=0xc000;i<0xd000;i+=Page::Page_Length) {
	MapPage(i,rp + (i>>Page::Page_Shift));
      }
    } else {
      if (osrom->RomType() == OsROM::Os_RomXL   || 
	  osrom->RomType() == OsROM::Os_Builtin ||
	  osrom->RomType() == OsROM::Os_Rom1200) {
	// map the XL rom here
	for(i=0xc000;i<0xd000;i+=Page::Page_Length) {
	  MapPage(i,rom + ((i-0xc000)>>Page::Page_Shift));
	}
	cfpage = rom + ((0xcf00 - 0xc000) >> Page::Page_Shift);
      } else if (extended_4K) {
	for(i=0xc000;i<0xd000;i+=Page::Page_Length) {
	  MapPage(i,rp + (i>>Page::Page_Shift));
	}
      } else {
	for(i=0xc000;i<0xd000;i+=Page::Page_Length) {
	  MapPage(i,blank);
	}
	cfpage = blank;
      }
    }
    // Now overlay 0xcf00 by RAM disk control if required.
    // This goes only into the CPU as ANTIC cannot perform
    // memory writes either.
    for(ext = Extensions.First();ext;ext = ext->NextOf()) {
      if (ext->MapControlPage(cpuspace,cfpage))
	break;
    }
    //
    // 0xd800 to 0xe000 is either math pack or RAM if math pack or
    // os is disabled.
    if (rom_disabled || mathpack_disable) {
      for(i=0xd800;i<0xe000;i+=Page::Page_Length) {
	MapPage(i,rp + (i>>Page::Page_Shift));
      }
    } else {
      switch(osrom->RomType()) {
      case OsROM::Os_RomA:
      case OsROM::Os_RomB:
	for(i=0xd800;i<0xe000;i+=Page::Page_Length) {
	  MapPage(i,rom + ((i-0xd800) >> Page::Page_Shift));
	}
	break;
      case OsROM::Os_Rom1200:
      case OsROM::Os_RomXL:
      case OsROM::Os_Builtin:
	for(i=0xd800;i<0xe000;i+=Page::Page_Length) {
	  MapPage(i,rom + ((i-0xc000) >> Page::Page_Shift));
	}
	break;
      case OsROM::Os_5200:
	for(i=0xd800;i<0xe000;i+=Page::Page_Length) {
	  MapPage(i,blank);
	}
	break;
      default:
	Throw(InvalidParameter,"MMU::BuildOsArea","found invalid ROM type");
      }
    }
    //
    // 0xe000 to 0x10000 is Os, or RAM
    if (rom_disabled) {
      for(i=0xe000;i<0x10000;i+=Page::Page_Length) {
	MapPage(i,rp + (i>>Page::Page_Shift));
      }
    } else {
      switch(osrom->RomType()) {
      case OsROM::Os_RomA:
      case OsROM::Os_RomB:
	for(i=0xe000;i<0x10000;i+=Page::Page_Length) {
	  MapPage(i,rom + ((i-0xd800) >> Page::Page_Shift));
	}
	break;
      case OsROM::Os_Rom1200:
      case OsROM::Os_Builtin:
      case OsROM::Os_RomXL:
	for(i=0xe000;i<0x10000;i+=Page::Page_Length) {
	  MapPage(i,rom + ((i-0xc000) >> Page::Page_Shift));
	}
	break;
      case OsROM::Os_5200:
	for(i=0xe000;i<0xf800;i+=Page::Page_Length) {
	  MapPage(i,blank);
	}
	for(i=0xf800;i<0x10000;i+=Page::Page_Length) {
	  MapPage(i,rom + ((i-0xf800) >> Page::Page_Shift));
	}
	break;
      default:
	Throw(InvalidParameter,"MMU::BuildOsArea","found invalid ROM type");
      }
    }
  }
}
///

/// MMU::BuildRamRomMapping
// Build the mapping for the complete RAM mapping
void MMU::BuildRamRomMapping(void)
{
  // Map the zero page up to 0x4000 to plain RAM.
  BuildLowRam();
  // Addresses 0x4000 to 0x8000 are mapped by the
  // 130XE extended bank switching logic and the
  // self-test ROM for XL's
  BuildMedRam();
  // Addresses 0x8000 to 0xc000 are mapped for 
  // cartridges or RAM. For the 5200, the cart area
  // starts at 0x4000.
  BuildCartArea();
  //
  //
  if (machine->MachType() == Mach_5200) {
    int i;
    // 0xc000 to 0xf800 is blank, with some IO blocks in the middle
    for(i = 0xc000;i < 0xf800;i += Page::Page_Length) {
      MapPage(i,blank);
    }
    // Map in GTIA, total range is C000 to D000.
    for(i = 0xc000;i < 0xd000;i += Page::Page_Length) 
      MapPage(i,machine->GTIA());  
    //
    // Antic starts here as in the main line.
    MapPage(0xd400,machine->Antic());
    //
    // Map in Pokey, again with a much larger address range
    for(i = 0xe800;i < 0xf000;i += Page::Page_Length) 
      MapPage(i,machine->PokeyPage());
  } else {
    // 0xd000 to 0xd800 is the IO space
    MapPage(0xd000,machine->GTIA());
    //MapPage(0xd100,machine->ParPort()); // FIXME: I do not yet know what to do about it
    MapPage(0xd100,blank);
    MapPage(0xd200,machine->PokeyPage());
    MapPage(0xd300,machine->PIA());
    MapPage(0xd400,machine->Antic());
    MapPage(0xd500,machine->CartCtrl());
    MapPage(0xd600,blank);
    MapPage(0xd700,handlers);
  }
  //
  BuildOsArea();
}
///

/// MMU::RemoveExtensions
// Remove MMU/RAM Extensions no longer required. This
// must happen after all options have been parsed or
// RAM contents is lost in repeated parsing.
void MMU::RemoveExtensions(void)
{  
  if (axlon == false && axlonram) {
    // Otherwise, if axlon is disabled, but we have it, get rid of it.
    axlonram->Remove();
    delete axlonram;
    axlonram = NULL;
  }
  //
  // Now the same again for the machine type and the XE Ram.
  if (machine->MachType() != Mach_AtariXE && xeram) {
    xeram->Remove();
    delete xeram;
    xeram = NULL;
  }
}
///

/// MMU::BuildExtensions
// Create all necessary RAM extensions for the MMU
bool MMU::BuildExtensions(void)
{  
  bool changed = false;
  //
  if (axlon && axlonram == NULL) {
    // Ok, the user wants the axlon RAM extension, but we don't have it,
    // so build it.
    Extensions.AddHead(axlonram = new class AxlonExtension(machine));
    changed = true;
  } 
  //
  // Now the same again for the machine type and the XE Ram.
  if (machine->MachType() == Mach_AtariXE && xeram == NULL) {
    // Build the XE RAM.
    Extensions.AddHead(xeram = new class XEExtension(machine));
    changed = true;
  }
  //
  return changed;
}
///

/// MMU::Initialize
void MMU::Initialize(void)
{
  // Find system linkage
  ram            = machine->RAM();
  osrom          = machine->OsROM();
  basicrom       = machine->BasicROM();
  cartrom        = machine->CartROM();
  cartctrl       = machine->CartCtrl();
  //  
  // Reset the MMU
  basic_mapped    = false;
  rom_disabled    = false;
  selftest_mapped = false;
  mathpack_disable= false;
  //
  // Build now the RAM extensions
  BuildExtensions();
  //
  // Remove extensions not required. Might be left
  // over since on config time, we never remove
  // extensions.
  RemoveExtensions();
  //
  // Setup the mapping
  BuildRamRomMapping();
}
///

/// MMU::Trig3CartLoaded
// Return whether GTIA TRIG3 will return a CART_LOADED flag here.
// Return false if no cart is loaded. Return true if so.
// NOTE! An disabled Oss super cart will show up here as
// no cart whatsoever.
bool MMU::Trig3CartLoaded(void)
{
  class Cartridge *cart;
  cart      = cartrom->Cart();
  // TRIG3 is set as soon as a single cart is mapped.
  while(cart) {
    if (cart->IsMapped())
      return true;
    cart = cart->NextOf();
  }
  // Otherwise, we are not mapped.
  return false;
}
///

/// MMU::ParseArgs
void MMU::ParseArgs(class ArgParser *args)
{
  class RamExtension *ext;
  bool extended = extended_4K;
  bool useaxlon = axlon;

  args->DefineTitle("MMU");
  args->DefineBool("4KExtended","Enable 0xc000 RAM for A400/A800",extended_4K);
  args->DefineBool("AxlonRam","Enable Axlon compatible RAM disk",useaxlon);
  if (extended_4K != extended) {
    extended = extended_4K;
    args->SignalBigChange(ArgParser::ColdStart);
  }
  // Check whether the axlon RAM disk mapping changed. If so, we need to perform
  // now some harder operations, namely a coldstart. This will also call the
  // initializer again and build the RAM for it.
  if (axlon != useaxlon) {
    axlon = useaxlon;
    args->SignalBigChange(ArgParser::ColdStart);
  }
  //
  // Build all required extensions. Should have been parsed off already,
  // so we can read their stats.
  if (BuildExtensions()) {
        args->SignalBigChange(ArgParser::ColdStart);
  }
  //
  // As RAM Extensions are not inherited from the configurable class (as to
  // avoid another topic in the user menu) configure them here manually.
  for(ext = Extensions.First();ext;ext = ext->NextOf()) {
    ext->ParseArgs(args);
  }
}
///

/// MMU::DisplayStatus
// Print the status of the MMU
void MMU::DisplayStatus(class Monitor *mon)
{
  class RamExtension *ext;
  //
  mon->PrintStatus("MMU status:\n"
		   "\tBasic     mapping       : %s\n"
		   "\tROM       mapping       : %s\n"
		   "\tSelfTest  mapping       : %s\n"
		   "\tMathPack  mapping       : %s\n"
		   "\tAtari400 52K            : %s\n",
		   (basic_mapped)?("on"):("off"),
		   (rom_disabled)?("off"):("on"),
		   (selftest_mapped)?("on"):("off"),
		   (mathpack_disable)?("off"):("on"),
		   (extended_4K)?("on"):("off")
		   );
  //
  // As RAM Extensions are not inherited from the chip class,
  // we must print their configuration here manually.
  for(ext = Extensions.First();ext;ext = ext->NextOf()) {
    ext->DisplayStatus(mon);
  }
}
///

/// MMU::State
// Read or set the internal status
void MMU::State(class SnapShot *sn)
{
  sn->DefineTitle("MMU");
  sn->DefineBool("BasicMapped","basic ROM mapped in flag",basic_mapped);
  sn->DefineBool("ROMDisabled","OS ROM disabled flag",rom_disabled);
  sn->DefineBool("SelfTestMapped","self-test mapped in flag",selftest_mapped);
  sn->DefineBool("MPDisable","MathPack disable flag",mathpack_disable);
  //
  // RAM Extensions are inherited from saveable and hence perform their own
  // state saving/restoring. No need to do that here.
  //
  // Now restore the complete machine setting
  BuildRamRomMapping();
}
///
