/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: osrom.hpp,v 1.27 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Administration/loading of Os ROMs
 **********************************************************************************/

#ifndef OSROM_HPP
#define OSROM_HPP

/// Includes
#include "patch.hpp"
#include "chip.hpp"
#include "memcontroller.hpp"
#include "argparser.hpp"
#include "rompage.hpp"
#include "patchprovider.hpp"
///

/// Forwards
class Monitor;
class ArgParser;
class HDevice;
class DeviceAdapter;
///

/// Class OsROM
class OsROM : public Chip, public PatchProvider, public MemController {
  //
public:
  // Os Types
  // Various types of Os ROM releases
  enum OsType {
    Os_Auto,              // automatic decision on the machine type
    Os_RomA,              // revision A
    Os_RomB,              // revision B
    Os_Rom1200,           // 1200 Os
    Os_RomXL,             // XL Os
    Os_Builtin,           // The built-in ROM image
    Os_5200               // the 5200 ROM
  };
  //
  // Links to other Os resources needed.
  class CPU            *cpu;
  class MMU            *mmu;
  class AdrSpace       *cpuram;
  class DeviceAdapter  *devices;
  //
  OsType                os_type;  // the type of Os to use
  //
  // The Os ROM pages we administrate. 32 for OsA,B, 64 for Os XL
  class RomPage         rom[64];
  //
  // Settings for this class:
  bool                  siopatch; // install SIO/DISKINTERFpatch?
  bool                  ppatch;   // install P: replacement
  bool                  hpatch;   // install H: replacement
  bool                  epatch;   // install E: replacement
  bool                  rpatch;   // install R: replacement
  bool                  hAsd;     // install H: as D: handler
  bool                  mppatch;  // install math pack patch?
  //
  // names of the ROM image
  char                 *osapath;
  char                 *osbpath;
  char                 *os1200path;
  char                 *osxlpath;
  char                 *os5200path;
  //
  // names of the target directories for the H: handler
  char                 *hdir[4];
  //
  // private: Load the Os ROM.
  void LoadROM(void);
  // Load one or several pages from a file into the Os ROM
  int LoadFromFile(const char *path,int pages);
  // Special service for the built-in ROM: Patch the ROM contents
  // from a static image
  void PatchFromDump(const unsigned char *dump,int pages);
  // Erase the math pack, replace by HLT instructions.
  void KillMathPack(void);
  //
  // Check whether a ROM file is well-formed.
  void CheckROMFile(const char *path,int pages);
  // 
  // Check whether the ROM image can be found at the specified path. In case
  // it can, return true and set the string variable to the given
  // path. Return false otherwise.
  bool FindRomIn(char *&rompath,const char *suggested,UWORD requiredsize);
  //
public:
  OsROM(class Machine *mach);
  ~OsROM(void);
  //
  //
  // We include here the interface routines imported from the chip class.
  //
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // The argument parser: Pull off arguments specific to this class.
  virtual void ParseArgs(class ArgParser *args);
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Service for MMU: Return the ROM to map it in
  class RomPage *OsPages(void)
  {
    return rom;
  }
  //
  // Return the currently active ROM
  OsType RomType(void) const;
  //
  // Pre-coldstart-phase: Load ROMs, install patches
  virtual void Initialize(void);
  //
  // Return the device adapter new devices are installed in.
  class DeviceAdapter *DeviceAdapter(void) const
  {
    return devices;
  }
  //
  // Check whether a following reset might cause a full
  // reset. If so, then returns true.
  bool MightColdstart(void);
};
///

///
#endif
  
