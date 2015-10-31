/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: deviceadapter.hpp,v 1.3 2015/05/21 18:52:38 thor Exp $
 **
 ** In this module: Device maintainment
 **********************************************************************************/

#ifndef DEVICEADAPTOR_HPP
#define DEVICEADAPTOR_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "patch.hpp"
///

/// Forwards
class Device;
class AdrSpace;
///

/// Class DeviceAdapter
// This is the maintainence structure for devices to be installed
// into the OS Rom. It either patches them in by replacing an entry
// in HATABS, or by using the dedicated handler area of the emulator
// at 0xd700..0xd7ff and installing the patch into Hatabs manually on
// reset.
class DeviceAdapter : public Patch {
  //
  // Pointer to the machine
  class   Machine *Machine;
  //
  // This boolean is set as soon as our own code is patched
  // in.
  bool    PatchedHook;
  //
  // Base address in the handler ROM to patch over.
  ADR     NextPatchEntry;
  //
  // Patch locations: Only located once, then re-used.
  ADR     OsAHatabs;
  ADR     OsBHatabs;
  ADR     OsXLHatabs;
  ADR     Os1200Hatabs;
  ADR     OsBuiltinHatabs;
  //
  // The patch code assigned to our own patch routine.
  UBYTE   PatchCode;
  //
  // The layout of the HATABS installed by the os. This is used to
  // locate this init string.
  static const UBYTE HInit[];
  // The jump to the CIO init. This is also patched over to install
  // additional handlers we haven't found any place for.
  static const UBYTE CIOInit[];
  //
  // Implemenation of the patch interface:
  // We can only provide the RunPatch interface. 
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
  //
  // Find the ROM entry point for the device given its device letter,
  // and replace it. Returns true in case this was successfull, false
  // otherwise (as the device must then be installed into the handler ROM)
  bool ReplaceDevice(class AdrSpace *adr,UBYTE patchcode,UBYTE deviceslot,UBYTE deviceletter,ADR *old);
  //
  // Find a specified string in an address space given a range. 
  // Returns either 0 if not found, or the address.
  // This is used to locate the Hatabs init table
  ADR FindString(class AdrSpace *adr,ADR from,ADR to,const UBYTE needle[],size_t size);
  //
  // Find a binary string in the OS rom, return its address (if any).
  ADR FindOsString(class AdrSpace *adr,const UBYTE needle[],size_t size);
public:
  //
  // Constructor and destructor
  DeviceAdapter(class Machine *mach,class PatchProvider *p);
  virtual ~DeviceAdapter(void)
  {
  }
  //
  // To be called by a device to get registered.
  void InstallDevice(class AdrSpace *adr,UBYTE patchcode,UBYTE slot,UBYTE letter,ADR *old);
};
///

///
#endif
