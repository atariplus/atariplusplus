/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: device.cpp,v 1.28 2020/07/18 16:32:40 thor Exp $
 **
 ** In this module: CIO device interface
 **********************************************************************************/

/// Includes
#include "device.hpp"
#include "cpu.hpp"
#include "adrspace.hpp"
#include "exceptions.hpp"
#include "osrom.hpp"
#include "deviceadapter.hpp"
///

/// Device::Device
Device::Device(class Machine *mach,class PatchProvider *p,UBYTE name,UBYTE slot)
  : Patch(mach,p,6), Machine(mach), 
    DeviceLetter(name), DeviceSlot(slot) // We need to install six patches
{ 
}
///

/// Device::SetResult
void Device::SetResult(class CPU *cpu,class AdrSpace *adr,UBYTE result)
{
  //
  // If break was pressed, revert this to an error 128
  if (result < 0x80 && adr->ReadByte(0x11) == 0)
    result = 128;
  //
  // Set the Y register to the result code.
  cpu->Y() = result;
  if (result >= 0x80) {
    cpu->P() |= CPU::N_Mask;
  } else {
    cpu->P() &= ~CPU::N_Mask;
  }
}
///

/// Device::ExtractFileName
// Service routine to peek a file name: This includes
// NUL-termination and abortion on invalid characters.
void Device::ExtractFileName(class AdrSpace *adr,ADR mem,char *buf,int bufsize)
{
  bool founddevice = false;
  bool founddot    = false;
  char *start = buf;
  char c;
  int len;
  int dlen = 0;
  
  bufsize--;
  len = 0;
  do {
    // Get the next character from the buffer address
    c = ValidCharacter(char(adr->ReadByte(mem++)));
    if (c == 0)
      break;
    if (founddot) {
      dlen++;
      if (dlen > 3)
	break;
    }
    if (c == ':' && !founddevice) { // Cut-off the device specifier
      buf = start;
      len = 0;
      founddevice = true;
      // We do not check whether we found a ":". We must have, as
      // CIO could not operate otherwise.
      continue;
    }
    if (c == '.' && !founddot) {
      if (founddot)
	break;
      founddot = true;
    }
    *buf++ = c; // insert the character
    len++;
  } while(len<bufsize);

  *buf++  = '\0'; // terminate the string.
}
///

/// Device::RunPatch
// Implemenation of the patch interface:
// We can only provide the RunPatch interface. InstallPatch has
// to be implemented by the corresponding device.
void Device::RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code)
{
  // Dispatch now the code
  switch(code) {
  case 0:
    Open(cpu,adr);
    break;
  case 1:
    Close(cpu,adr);
    break;
  case 2:
    Get(cpu,adr);
    break;
  case 3:
    Put(cpu,adr);
    break;
  case 4:
    Status(cpu,adr);
    break;
  case 5:
    Special(cpu,adr);
    break;
  }
}
///

/// Device::InstallPatch
// Install the series of patches into Hatabs
void Device::InstallPatch(class AdrSpace *adr,UBYTE code)
{
  class DeviceAdapter *apt = Machine->OsROM()->DeviceAdapter();
  //
  // Get us installed with the device adapter.
  apt->InstallDevice(adr,code,DeviceSlot,DeviceLetter,Original);
}
///

/// Device::Open
void Device::Open(class CPU *cpu,class AdrSpace *adr)
{
  UBYTE channel = UBYTE(cpu->X() >> 4);       // get IOCB address
  UBYTE unit    = UBYTE(adr->ReadByte(0x21)); // get the unit #
  UBYTE aux1    = UBYTE(adr->ReadByte(0x2a));
  UBYTE aux2    = UBYTE(adr->ReadByte(0x2b));
  ADR   mem     = ADR(adr->ReadWord(0x24));   // name of the game
  UBYTE result;
  char  buf[256];                // names goes into here.
  
  if (channel < 8 && (cpu->X() & 0x0f) == 0) {
    ExtractFileName(adr,mem,buf,sizeof(buf));
    
    // Let the real handler do the job for us.
    result = Open(channel,unit,buf,aux1,aux2);
    //
    // Install the result and return
    SetResult(cpu,adr,result);
  } else {
    SetResult(cpu,adr,0x86);
  }
}
///

/// Device::Close
void Device::Close(class CPU *cpu,class AdrSpace *adr)
{
  UBYTE channel = UBYTE(cpu->X() >> 4);       // get IOCB address
  UBYTE result;

  if (channel < 8 && (cpu->X() & 0x0f) == 0) {
    // Let the real handler do the job for us.
    result = Close(channel);
    //
    // Install the result and return
    SetResult(cpu,adr,result);
  } else {
    SetResult(cpu,adr,0x86);
  }
}
///

/// Device::Get
void Device::Get(class CPU *cpu,class AdrSpace *adr)
{
  UBYTE channel = UBYTE(cpu->X() >> 4);       // get IOCB address
  UBYTE result,data = 0x9b; // Shut up the compiler

  if (channel < 8 && (cpu->X() & 0x0f) == 0) {
    // Let the real handler do the job for us.
    result = Get(channel,data);
    //
    // Install the result and return
    SetResult(cpu,adr,result);
    cpu->A() = data;
  } else {
    SetResult(cpu,adr,0x86);
  }
}
///

/// Device::Put
void Device::Put(class CPU *cpu,class AdrSpace *adr)
{
  UBYTE channel = UBYTE(cpu->X() >> 4);       // get IOCB address
  UBYTE result,data;

  if (channel < 8 && (cpu->X() & 0x0f) == 0) {
    // Let the real handler do the job for us.
    data   = cpu->A();
    result = Put(channel,data);
    //
    // Install the result and return
    SetResult(cpu,adr,result);
  } else {
    SetResult(cpu,adr,0x86);
  }
}
///

/// Device::Status
void Device::Status(class CPU *cpu,class AdrSpace *adr)
{
  UBYTE channel = UBYTE(cpu->X() >> 4);       // get IOCB address
  UBYTE result;
  
  if (channel < 8 && (cpu->X() & 0x0f) == 0) {
    // Let the real handler do the job for us.
    result = Status(channel);
    //
    // Install the result and return
    SetResult(cpu,adr,result);
  } else {
    SetResult(cpu,adr,0x86);
  }
}
///

/// Device::Special
void Device::Special(class CPU *cpu,class AdrSpace *adr)
{
  UBYTE channel = UBYTE(cpu->X() >> 4);// get IOCB address
  UBYTE unit    = UBYTE(adr->ReadByte(0x21)); // get the unit #
  UBYTE cmd     = UBYTE(adr->ReadByte(0x22)); // get the IOCB cmd
  ADR   mem     = ADR(adr->ReadWord(0x24));   // address of the memory we operate on
  UWORD len     = UWORD(adr->ReadWord(0x28)); // number of bytes
  UWORD iocb    = UWORD(0x340+(channel << 4));// base offset of the IOCB block
  UBYTE aux[6];                        // AUX registers
  UBYTE result;
  int i;

  if (channel < 8 && (cpu->X() & 0x0f) == 0) {  
    for(i=0;i<6;i++) {
      aux[i]     = adr->ReadByte(iocb + 0x0a + i);
    }
    // Let the real handler do the job for us.
    result = Special(channel,unit,adr,cmd,mem,len,aux);
    //
    // Point&Note return results in aux, need to put it back.
    // The XIO 41 (binary load) of the H: device does
    // not want to overwrite the AUX1
    for(i=2;i<6;i++) {
      adr->WriteByte(iocb + 0x0a + i,aux[i]);
    }
    //
    // Install the result and return
    SetResult(cpu,adr,result);
  } else {
    SetResult(cpu,adr,0x86);
  }
}
///
