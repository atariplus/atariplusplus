/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: hdevice.hpp,v 1.20 2015/09/25 18:50:31 thor Exp $
 **
 ** In this module: H: emulated device for emulated disk access.
 **********************************************************************************/

#ifndef HDEVICE_HPP
#define HDEVICE_HPP

/// Includes
#include "device.hpp"
#include "stdio.hpp"
#include "directory.hpp"
///

/// Forwards
class PatchProvider;
class CPU;
class AdrSpace;
///

/// Class HDevice
// This device implements the CIO emulation layer for the H: device
// driver, an emulation layer for handler disk access.
class HDevice : public Device {
  //
  // The base directory of this H device driver
  char *const     *BaseDir;
  //
  // Each channel has its access buffer in here.
  struct HandlerChannel {
    FILE          *stream;    // if we opened for file I/O, this is the file
    DIR           *dirstream; // if we opened this for directory reading, get it here.
    struct dirent *fib;
    UBYTE         *buffer;    // IO buffer
    UBYTE         *bufptr;    // pointer into the above
    char          *pattern;
    UBYTE          openmode;  // this is AUX1 of the open parameter
    const char    *basedir;   // this is the unit number of this channel
    UBYTE          lasterror;
    //
  private:
    // Private subroutine for pattern matching
    static bool MatchRecursive(const char *file,char *pat);
    //
  public:
    HandlerChannel(UBYTE mode,const char *base)
      : stream(NULL), dirstream(NULL), 
	buffer(NULL), bufptr(NULL), pattern(NULL), 
	openmode(mode), basedir(base),
	lasterror(0x01)
    { }
    //
    ~HandlerChannel(void)
    {
      if (stream)   fclose(stream);
      if (dirstream) closedir(dirstream);
      delete[] buffer;
      delete[] pattern;
    }
    //    
    // Translate an Unix error code to something Atari like
    static UBYTE AtariError(int error);
    //
    // Service: Check whether we have a wild card in here
    static bool IsWild(const char *pattern);
    //
    // Service: Check whether the file name is valid
    static bool IsValid(const char *filename);
    //
    // Service: Find a match by pattern
    static bool MatchPattern(const char *filename,char *pattern);
    //
    // Convert the entry in direct to an atari readable format
    // into the buffer.
    UBYTE ToDirEntry(void);
    //
    // Service: Start a match-chain for a file name, return an
    // Atari error code on error or if there is not a single match.
    // On a match, dirent contains the proper name.
    UBYTE MatchFirst(const char *pattern);
    //
    // Continue a match, return 0x01 if there is one.
    UBYTE MatchNext(void);
  }                *Buffer[9]; // One additional for implicit open
  //
  // The following additional patch is required for the binary load
  // interface of the XIO 41 command.
  class BinaryLoadCallbackPatch : public Patch {
    //
    // States of the binary load return patch, i.e. the
    // state of the state machine we implement here.
    enum {
      Idle,       // nothing
      Open,       // CIO open command returned
      ReadHeader, // bootstrap 0xff,0xff header
      ReadStart,  // read address start
      ReadEnd,    // read the address end
      ReadBody,   // read data body
      Init,       // bootstrap thru the init vector
      Run,        // bootstrap thru the run vector
      ReReadStart,// read a start address, or the header
      Close       // close the channel down, return to caller.
    }               State;
    //
    // Pointer to the CPU we are serving.
    class CPU      *CPU;
    //
    // The address space we load/write data into. This
    // is of course (or should be) the CPU address space.
    class AdrSpace *BaseSpace;
    //
    // The machine, used here to generate a warning if necessary.
    class Machine  *machine;
    //
    // The IOCB channel (*16) we are using for the binary
    // load interface.
    UBYTE           Channel;
    //
    // The AUX1 flag given on the XIO 41 command. This
    // bit set flag mask describes whether we should
    // run/init the binary.
    UBYTE           RunMask;
    //
    // We place here a possible error code
    // to be delivered back.
    UBYTE           ErrorCode;
    //
    // Data pulled from the stream:
    // Start and end address.
    ADR             Start,End;
    //
    // Push the indicated return address onto the
    // stack of the emulated CPU to make it call the
    // given routine.
    void PushReturn(ADR where);
    //
    // Close the channel, then deliver the error
    // code back.
    void CloseChannel(void);
    //
    // Perform a CIO operation, call back this patch
    // when done. IOCB must have been setup prior
    // calling this.
    void RunCIO(void);
    //
    // Read a block from CIO from the currently
    // setup IOCB.
    void ReadBlock(ADR position,UWORD len);
    //    
    // Implementation of the patch interface.
    virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
    virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  public:
    // Constructor and destructor
    BinaryLoadCallbackPatch(class Machine *mach,class PatchProvider *p);
    ~BinaryLoadCallbackPatch(void);
    //
    // Initialize the binary load interface, i.e. initialize the
    // state machine.
    void LaunchBinaryLoad(class AdrSpace *adr,UBYTE channel,UBYTE auxflag);
    //    
    // Reset the patch, and thus the state-machine.
    virtual void Reset(void);
  } *CallBackPatch;
  //
  // Implementations of the device abstraction layer
  // The result code is placed in the Y register and the negative flag
  // is set accordingly.
  virtual UBYTE Open(UBYTE channel,UBYTE unit,char *name,UBYTE aux1,UBYTE aux2);
  virtual UBYTE Close(UBYTE channel);
  // Read a value, return an error or fill in the value read
  virtual UBYTE Get(UBYTE channel,UBYTE &value);
  virtual UBYTE Put(UBYTE channel,UBYTE value);
  virtual UBYTE Status(UBYTE channel);
  virtual UBYTE Special(UBYTE channel,UBYTE unit,class AdrSpace *adr,UBYTE cmd,
			ADR mem,UWORD len,UBYTE aux[6]);
  //
  virtual void Reset(void);
  //
  // Extract/modify AUX1 with the /x file name modifiers
  void FilterAux1(char *name,UBYTE &aux1);
  //
  // Various helpers for the implementation of Special. These perform
  // the XIO commands we know from the DOS.
  //
  UBYTE Point(struct HandlerChannel *ch,LONG sector);
  UBYTE Note(struct HandlerChannel *ch,LONG &sector);
  UBYTE Rename(struct HandlerChannel *ch,char *pattern);
  UBYTE Delete(struct HandlerChannel *ch,char *pattern);
  UBYTE Validate(struct HandlerChannel *ch,char *pattern);
  UBYTE Resolve(struct HandlerChannel *ch,char *pattern,UBYTE cnt);
  UBYTE Protect(struct HandlerChannel *ch,char *pattern);
  UBYTE Unprotect(struct HandlerChannel *ch,char *patten);
  UBYTE BinaryLoad(class AdrSpace *adr,UBYTE channel,UBYTE aux2);
public:
  HDevice(class Machine *mach,class PatchProvider *p,char *const *basedir,char id = 'H');
  virtual ~HDevice(void);
};
///

///
#endif
