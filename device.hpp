/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: device.hpp,v 1.19 2015/09/22 11:46:33 thor Exp $
 **
 ** In this module: CIO device interface
 **********************************************************************************/

#ifndef DEVICE_HPP
#define DEVICE_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "patch.hpp"
#if HAVE_ISASCII
#include <ctype.h>
#else
// Implement by hand otherwise
#include <ctype.h>
#define isascii(c) (((unsigned char)c) < 0x80)
#endif
///

/// Forwards
class CPU;
class AdrSpace;
class PatchProvider;
///

/// Class Device
// Generic interface towards patched-in CIO device drivers.
// This takes all the methods required to run device IO effectively.
class Device : public Patch {
  // 
protected:
  // Pointer to the machine
  class   Machine *Machine;
  //
  // Identifier of this device: This is the device letter as used for
  // CIO open and others. DeviceSlot is the slot the device will be
  // installed into. Hence, we may replace a device entry by a different
  // name.
  UBYTE        DeviceLetter,DeviceSlot;
  //
  // Addresses of the old entry points.
  ADR          Original[6];
  //
private:  
  //
  // Implemenation of the patch interface:
  // We can only provide the RunPatch interface. 
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
  //
  // Secondary helper functions called by the above dispatcher
  void Open(class CPU *cpu,class AdrSpace *adr);
  void Close(class CPU *cpu,class AdrSpace *adr);
  void Get(class CPU *cpu,class AdrSpace *adr);
  void Put(class CPU *cpu,class AdrSpace *adr);
  void Status(class CPU *cpu,class AdrSpace *adr);
  void Special(class CPU *cpu,class AdrSpace *adr);  
  // This method is called on a reset to close all open streams
  virtual void Reset(void) = 0;
  //
protected:  
  // Constructor and destructor
  Device(class Machine *mach,class PatchProvider *p,UBYTE devicename,UBYTE slot);
  virtual ~Device(void)
  {
  }
  // The following methods have to be defined in the specific implementatins
  // of the device handler. They are called from within this device
  // management.
  // The result code is placed in the Y register and the negative flag
  // is set accordingly.
  virtual UBYTE Open(UBYTE channel,UBYTE unit,char *name,UBYTE aux1,UBYTE aux2) = 0;
  virtual UBYTE Close(UBYTE channel) = 0;
  // Read a value, return an error or fill in the value read
  virtual UBYTE Get(UBYTE channel,UBYTE &value) = 0;
  virtual UBYTE Put(UBYTE channel,UBYTE value) = 0;
  virtual UBYTE Status(UBYTE channel) = 0;
  virtual UBYTE Special(UBYTE channel,UBYTE unit,class AdrSpace *adr,UBYTE cmd,
			ADR mem,UWORD len,UBYTE aux[6]) = 0;
  //
  //
  // Misc. routines
  //
  // Check whether a character is a valid component of a file name.
  // If so, return it in lower case. Otherwise, return NULL.
  static char ValidCharacter(unsigned char c)
  {
    if (c >= 128)
		return 0;
    if (c == ':' || c == '.' || c == '?' || c == '-' || c == '*' || c == ',' || c == '/')
      return c;
    if (isspace(c))
      return 0;
    if (isascii(c) && isalnum(c)) {
      return (char)tolower(c);
    }
    return 0;
  }
  //
  static void SetResult(class CPU *cpu,class AdrSpace *adr,UBYTE result);
  //
public:
  //
  // Service routine to peek a file name: This includes
  // NUL-termination and abortion on invalid characters.
  void ExtractFileName(class AdrSpace *adr,ADR mem,char *buf,int bufsize);
  //
};
///

///
#endif
