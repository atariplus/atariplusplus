/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: hdevice.cpp,v 1.55 2021/07/03 16:16:44 thor Exp $
 **
 ** In this module: H: emulated device for emulated disk access.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "hdevice.hpp"
#include "directory.hpp"
#include "string.hpp"
#include "unistd.hpp"
#include "mmu.hpp"
#include "adrspace.hpp"
#include "cpu.hpp"
#include "new.hpp"
#include <ctype.h>
#include <errno.h>
///

/// HDevice::HDevice
// The H device replaces the C: handler we have no use in.
HDevice::HDevice(class Machine *mach,class PatchProvider *p,char *const *dirbase,char id)
  : Device(mach,p,id,'C'), CallBackPatch(new class BinaryLoadCallbackPatch(mach,p))
{
  int i;

  BaseDir = dirbase;
  
  for(i=0;i<9;i++) {
    Buffer[i]         = NULL;
  }
}
///

/// HDevice::~HDevice
// The H device replaces the C: handler we have no use in.
HDevice::~HDevice(void)
{
  int i;

  for(i=0;i<9;i++) {
    delete Buffer[i];
  }
  delete CallBackPatch;
}
///

/// HDevice::BinaryLoadCallbackPatch::BinaryLoadCallbackPatch
// The callback patch of the binary load interface.
HDevice::BinaryLoadCallbackPatch::BinaryLoadCallbackPatch(class Machine *mach,class PatchProvider *p)
  : Patch(mach,p,1), State(Idle), CPU(mach->CPU()), machine(mach)
{
}
///

/// HDevice::BinaryLoadCallbackPatch::~BinaryLoadCallbackPatch
HDevice::BinaryLoadCallbackPatch::~BinaryLoadCallbackPatch(void)
{
}
///

/// HDevice::BinaryLoadCallbackPatch::Reset
// Reset the callback patch to idle to be prepared for
// the next go
void HDevice::BinaryLoadCallbackPatch::Reset(void)
{
  State = Idle;
}
///

/// HDevice::BinaryLoadCallbackPatch::InstallPatch
// Install the callback patch. This patch is the "bogus" return
// address when performing a binary load from the H: device.
void HDevice::BinaryLoadCallbackPatch::InstallPatch(class AdrSpace *adr,UBYTE code)
{
  // Hijack the C: bootstrap vector, it is not used since
  // we overwrite the H: handler anyhow.
  InsertESC(adr,0xe47a,code);
}
///

/// HDevice::BinaryLoadCallbackPatch::RunPatch
void HDevice::BinaryLoadCallbackPatch::RunPatch(class AdrSpace *,class CPU *,UBYTE)
{
  //
  // All this depends on the state of the patch callback
  // state machine now.
  switch(State) {
  case Open:
    // Returned from the CIO open. Check whether the open was performed
    // fine. If so, continue, otherwise return with the error code
    // to the caller.
    if ((ErrorCode = CPU->Y()) == 0x01) {
      // Worked. Now prepare to read the header. We therefore
      // mis-use the bootstrap vectors.
      State = ReadHeader;
      ReadBlock(0x240,0x02);
      break;
    }
    // Otherwise, return to the caller immediately,
    // leaving the error code where it is.
    CloseChannel();
    break;
  case ReadHeader:
    // Reaturned from the header reading. Now analyze the header
    // to check whether it is really a binary load file. 
    // Both bytes must be 0xff,0xff.
    if ((ErrorCode = CPU->Y()) == 0x01) {
      if (BaseSpace->ReadByte(0x240) == 0xff && BaseSpace->ReadByte(0x241) == 0xff) {
	// Initialize init and run addresses.
	BaseSpace->WriteByte(0x2e0,0);
	BaseSpace->WriteByte(0x2e1,0);
	BaseSpace->WriteByte(0x2e2,0);
	BaseSpace->WriteByte(0x2e3,0);
	// All fine. Now load the start address.
	State = ReadStart;
	ReadBlock(0x240,0x02);
	break;
      } 
      ErrorCode = 175; // not a binary load file.
    }
    // Close the channel, return to caller.
    CloseChannel();
    break;
  case ReadStart:
    // Reading the start address returned here. Keep it,
    // and start the end address.
    if ((ErrorCode = CPU->Y()) == 0x01) {
      Start = BaseSpace->ReadWord(0x240);
      // Check whether we already have a run address.
      // If not, then keep this initial address as
      // run address for OS/A+ compatibility.
      /* BAD idea, not Atari compatible
      BaseSpace->WriteByte(0x2e0,Start & 0xff); // lo
      BaseSpace->WriteByte(0x2e1,Start >> 8  ); // hi
      */
      // Fine. Keep it.
      State = ReadEnd;
      ReadBlock(0x240,0x02);
      break;
    }
    CloseChannel();
    break;
  case ReadEnd:
    // Reading the end address returned here. Keep it,
    // and check for consistency.
    if ((ErrorCode = CPU->Y()) == 0x01) {
      End  = BaseSpace->ReadWord(0x240);
      if (End >= Start) {
	// That seems valid, great!
	State  = ReadBody;
	ReadBlock(Start,End - Start + 1);
	break;
      } 
      ErrorCode = 175; // not a binary load file.
    }
    // Close the channel, return to caller.
    CloseChannel();
    break;
  case ReadBody:
    // The data block has been read off now.
    // If everything is fine, and we have an init
    // address, and we shall call the init vector, then
    // do. Otherwise, continue loading.
    if ((ErrorCode = CPU->Y()) == 0x01) {
      UWORD init;
      State = Init;
      // At the end, call us again.
      PushReturn(0xe47a);
      // Worked. Check the init vector.
      init = BaseSpace->ReadWord(0x2e2);
      if (init && (RunMask & 0x80)) {
	// Call it.
	PushReturn(init);
      }
      break;
    } else if (ErrorCode == 0x03) {
      UWORD init;
      UWORD run;
      // We are at the EOF. That's fine and no
      // error.
      ErrorCode = 0x01;      
      // Keep the channel
      // open, and jump thru the run address
      // should we have been asked for. 
      run = BaseSpace->ReadWord(0x2e0);
      if (run && (RunMask & 0x40)) {
	// Read it from memory for maximum
	// compatibility.
	PushReturn(run);
      }
      //
      // Call the init vector first if we have one.
      init = BaseSpace->ReadWord(0x2e2);
      if (init && (RunMask & 0x80)) {
	// Call it.
	PushReturn(init);
      }
      //
      // But even before that, close the channel.
      CloseChannel();
      break;
    }
    // Otherwise, close the channel and return.
    CloseChannel();
    break;
  case Init:
    // Init vector returned. First, cleanup the
    // init vector for the next go.    
    BaseSpace->WriteByte(0x2e2,0);
    BaseSpace->WriteByte(0x2e3,0);
    //
    // Now re-read the header. Could be 0xff 0xff for
    // compound file, or again the start address.
    State = ReReadStart;
    ReadBlock(0x240,0x02);
    break;
  case ReReadStart:
    // We are here in case we read the start
    // address or a 0xff 0xff from CIO again.
    if ((ErrorCode = CPU->Y()) == 0x01) {
      // Everything's fine. Check what we
      // read. If this is a 0xff 0xff, continue
      // with this step, otherwise continue
      // with the end address.
      Start = BaseSpace->ReadWord(0x240);
      if (Start != 0xffff) {
	State = ReadEnd; // Been there again.
      }    
      ReadBlock(0x240,0x02);
      break;
    } 
    // Otherwise, a real error we shall report
    CloseChannel();
    break;
  case Run:
    // Returned from run vector. Not much
    // to do except to close the channel.
    CloseChannel();
    break;
  case Close:
    // Returned from closing the channel.
    // Just install the error code back
    // again, then finally let the CPU
    // go free.
    State = Idle;
    // Set the Y register to the result code.
    CPU->Y() = ErrorCode;
    if (ErrorCode >= 0x80) {
      CPU->P() |= CPU::N_Mask;
    } else {
      CPU->P() &= ~CPU::N_Mask;
    }
    break;
  default:
#if CHECK_LEVEL > 0
    // We shouldn't be here...
    Throw(PhaseError,"HDevice::BinaryLoadCallbackPatch::RunPatch",
	  "invalid state of the binary call back patch detected");
#endif   
    //
    // Run into the following.
  case Idle:
    // Generate a warning.
    machine->PutWarning("A program is currently trying to boot "
			"from the tape, however the tape is currently "
			"unavailable due to the HDevice patch. Disable the "
			"HDevice patch in the OsRom menu to allow booting.");
    // If we are called here, then a program called the cassette init vector
    // without really knowing what is going on. Return with an error code.
    CPU->Y()  = 0x90;
    CPU->P() |= CPU::N_Mask;
    break;
  }
}
///

/// HDevice::CloseChannel
// Close the channel we recorded, then deliver
// the error code.
void HDevice::BinaryLoadCallbackPatch::CloseChannel(void)
{  
  BaseSpace->WriteByte(0x342 + Channel,12); // Write a close command
  State = Close; // modify the state machine
  RunCIO(); // and go.
}
///

/// HDevice::BinaryLoadCallbackPatch::PushReturn
// Push the indicated return address onto the
// stack of the emulated CPU to make it call the
// given routine.
void HDevice::BinaryLoadCallbackPatch::PushReturn(ADR where)
{
  ADR stackptr = 0x100 + CPU->S(); // stack pointer.
  // We need to push the return address -1 since the
  // RTS adds one to the return PC.
  where--;
  BaseSpace->WriteByte(stackptr--,where >> 8  ); // Hi goes first here.
  BaseSpace->WriteByte(stackptr--,where & 0xff); // Then Lo
  //
  // Now install the stack pointer back.
  CPU->S() = stackptr - 0x100;
}
///

/// HDevice::BinaryLoadCallbackPatch::RunCIO
// Run the CIO routine, then return to this
// patch.
void HDevice::BinaryLoadCallbackPatch::RunCIO(void)
{
  // We need to push the return address to our
  // patch entry code onto the stack, then push the CIO callback 
  // onto the stack.
  CPU->X() = Channel;
  PushReturn(0xe47a); // the (new) callback vector
  PushReturn(0xe456); // CIO entry point.
}
///

/// HDevice::BinaryLoadCallbackPatch::ReadBlock
// Read a data block over CIO into the indicated
// buffer area, then return to this patch.
void HDevice::BinaryLoadCallbackPatch::ReadBlock(ADR position,UWORD len)
{
  BaseSpace->WriteByte(0x342 + Channel,0x07); // read characters.
  BaseSpace->WriteByte(0x344 + Channel,position & 0xff); // buffer, lo address
  BaseSpace->WriteByte(0x345 + Channel,position >> 8);   // buffer, hi address
  BaseSpace->WriteByte(0x348 + Channel,len & 0xff);      // size, lo
  BaseSpace->WriteByte(0x349 + Channel,len >> 8);        // size, hi
  //
  // And now launch the CIO, come back to our patch code.
  RunCIO();
}
///

/// HDevice::BinaryLoadCallbackPatch::LaunchBinaryLoad
// Initialize the binary load interface, i.e. initialize the
// state machine.
void HDevice::BinaryLoadCallbackPatch::LaunchBinaryLoad(class AdrSpace *adr,UBYTE channel,UBYTE auxflag)
{
  // Store the data we need to perform the task.
  Channel   = channel << 4;
  RunMask   = auxflag;
  BaseSpace = adr;
  //
  // Initialize CIO to perform the task. We need to setup
  // aux1,aux2 and the command.
  BaseSpace->WriteByte(0x342 + Channel,3); // write the command: CMD_OPEN
  BaseSpace->WriteByte(0x34a + Channel,4); // open for read
  BaseSpace->WriteByte(0x34b + Channel,0); // clean aux2
  State    = Open;
  RunCIO();                     // launch a CIO call and call me back when done.
}
///

/// HDevice::HandlerChannel::MatchPattern
// Check whether a file name matches a pattern
bool HDevice::HandlerChannel::MatchPattern(const char *filename,char *pattern)
{
  const char *ch = filename;
  bool havedot   = false;
  //
  // If the file name contains a '.', we never match. Hidden files
  // are never shown here.
  if (*filename == '.')
    return false;
  //
  // If the filename contains something that is not printable, do not
  // match either.
  while(*ch) {
    if (*ch <= 0x20 || 
	(*ch >= 0x21 && *ch <= 0x40 && *ch != '.' && !(*ch >= '0' && *ch <= '9')) ||
	(*ch >= 0x5b && *ch <= 0x60) || (*ch >= 0x7b)) {
      if (havedot == false || *ch != ' ') {
	return false;
      }
    }
    if (*ch == '.') {
      if (havedot)
	return false;
      havedot = true;
    }
    ch++;
  }
  // If the pattern contains a '-', we always match. (Thor-Dos)
  // We do not check trailing characters here.
  if (*pattern == '-')
    return true;
  //
  //
  return MatchRecursive(filename,pattern);
}
///

/// HDevice::HandlerChannel::MatchRecursive
// Check whether a file name matches a pattern
bool HDevice::HandlerChannel::MatchRecursive(const char *filename,char *pattern)
{
  // Dependent on the current pattern character, check whether we match here.
  switch(*pattern) {
  case '-':
    // The - ignores the remaining pattern and matches always
    return true;
  case '*':
    {
      const char *sub = filename;
      // This is hardest to handle: The '*' matches if some sub-expression of
      // the filename matches.
      while(true) {
	if (MatchRecursive(sub,pattern+1))
	  return true;
	if (*sub == 0)
	  break;
	sub++;
      }
    }
    // If there is no subexpression, we cannot match.
    return false;
  case '?':
    // Matches any character except the NUL.
    if (*filename)
      return MatchRecursive(filename+1,pattern+1);
    return false;
  case '\0':
  case '/':
    // End of pattern. Matches the end of the file.
    if (*filename == '\0')
      return true;
    // Additional rule to work against badly transformed
    // file names. If the file name ends on ., also match
    // here.
    if (*filename == '.')
      filename++;
    while(*filename++ == ' '){};
    if (*--filename == '\0')
      return true;
    return false;
  default:
    // These are the only two wildcards we have.
    if (toupper(*filename) == toupper(*pattern))
      return MatchRecursive(filename+1,pattern+1);
    return false;
  }
}
///

/// HDevice::HandlerChannel::AtariError
// Translate an Unix error code to something Atari like
UBYTE HDevice::HandlerChannel::AtariError(int error)
{
  switch(error) {
  case EACCES:
  case EEXIST:
  case EROFS:
#if HAS_ETXTBSY_DEFINE
  case ETXTBSY:
#endif
    return 0xa7;  // file locked.
  case ENOENT:
#if HAS_ELOOP_DEFINE
  case ELOOP:
#endif
    return 0xaa;  // File not found on access error (yikes!)
  case EMFILE:
  case ENFILE:
    return 0xa1;  // actually the same for atari.
  case ENOMEM:    
    return 0x93;  // memory failure
  case ENOTDIR:
  case EISDIR:
    return 0x92;
  case ENAMETOOLONG:
  case EFAULT:
    return 0xa5;  // invalid filename
  case ENXIO:
  case ENODEV:
    return 0xa8;
  case ENOSPC:
    return 0xa2;
  }
  return 0xa3; // Unknown error
}
///

/// HDevice::HandlerChannel::MatchFirst
// Start a pattern match for a given pattern.
UBYTE HDevice::HandlerChannel::MatchFirst(const char *pat)
{
  UBYTE res;
  //
  if (dirstream) {
    return 0x81; // channel is open
  }
  // Check wether the pattern is valid. Abort if not.
  if (pat && !IsValid(pat)) {
    return 0xa5;
  }
  //
  // Open the directory.
  if ((dirstream = opendir(basedir))) {
    // Opening the directory worked. Now run MatchNext to get
    // access to a match.
    if (pattern == NULL) {
      pattern = new char[strlen(pat) + 1];
      strcpy(pattern,pat);
    }
    res = MatchNext();
    if (res == 0x88) {
      // triggered EOF: No single match -> file not found error
      return 0xaa;
    }
    return res;
  } else {
    return AtariError(errno);
  }
}
///

/// HDevice::HandlerChannel::MatchNext
// Match the next file in the directory or return an error otherwise.
UBYTE HDevice::HandlerChannel::MatchNext(void)
{
  do {
    errno = 0;
    fib   = readdir(dirstream);
    if (fib) {
      // Found an entry. Check whether it matches.
      if (MatchPattern(de_name(fib),pattern))
	return 0x01; // found a matching entry
    } else {
      if (errno == 0) // Just the end of the directory
	return 0x88;
      return AtariError(errno);
    }
  } while(true);
}
///

/// HDevice::HandlerChannel::IsWild
// Service: Check whether we have a wild card in here
bool HDevice::HandlerChannel::IsWild(const char *pattern)
{
  while(*pattern) {
    if (*pattern == '-' || *pattern == '*' || *pattern == '?')
      return true;
    pattern++;
  }
  return false;
}
///

/// HDevice::HandlerChannel::IsValid
// Service: Check whether a file name is valid.
bool HDevice::HandlerChannel::IsValid(const char *pattern)
{
  char c;
  int cnt = 0;
  bool dot = false;
  bool dash = false;
  
  while((c = *pattern)) {
    if (!isalpha(c)) {
      if (cnt > 0 && c == '.') {   // Found extender separator
	if (dot) return false;     // not two dots
	if (cnt > 8) return false; // not more than eight character in front of the dot
	dot = true;
	cnt = 0;
	pattern++;
	continue;
      } else if (c == '-') {
	dash = true;
      } else if (!((dot || cnt > 0) && isdigit(c)) && c != '?' && c != '*')
	return false;              // all others must be digits or wildcards
    }
    cnt++;
    pattern++;
  }
  if (dash) return true;
  if (dot) {
    if (cnt > 3) return false;
  } else {
    if (cnt > 8) return false;
  }
    
  return true;
}
///

/// HDevice::HandlerChannel::ToDirEntry
// Convert the entry in direct to an atari readable format
// into the buffer, reset the buffer pointer.
UBYTE HDevice::HandlerChannel::ToDirEntry(void)
{
  size_t i,len;
  struct stat info;
  char fullname[257];
  const char *name = de_name(fib);
  const char *dot;

  if (strlen(basedir) + 3 + strlen(name) > 256) {
    return 0xa5;
  }
  if (snprintf(fullname,sizeof(fullname),"%s/%s",basedir,name) > int(sizeof(fullname)))
    return 0xa5; // invalid file name
  if (stat(fullname,&info) == -1) {
    return AtariError(errno);
  }
  if (info.st_mode & S_IWUSR) {
    // File is read and writable. Mark it unmarked.
    buffer[0] = ' ';
  } else {
    // Mark file as locked.
    buffer[0] = '*';
  }
  buffer[1] = ' ';
  // Copy the first eight characters up to the dot into the array.
  memset(buffer+2,' ',8+3+1);
  dot = strchr(name,'.');
  if (dot) {
    len = dot-name;
  } else {
    len = strlen(name);
  }
  if (len > 8)
    len = 8;
  for(i=0;i<len;i++) {
    buffer[i+2] = (char)toupper(name[i]);
  }
  //
  // Check wether there is anything beyond the dot. If so, copy this in as well.
  if (dot) {
    len = strlen(dot+1);
    if (len > 3)
      len = 3;
    for(i=0;i<len;i++) {
      buffer[i+2+8] = (char)toupper(dot[i+1]);
    }
  }
  //
  // Now compute the size of the file in sectors and fill it in as well.
  // We assume sectors to 125 bytes each to get the same wierd numbers as
  // Dos 2 does.
  len = (info.st_size + 124) / 125;
  // Limit the size to 999 to avoid filling in more characters here.
  if (len > 999)
    len = 999;
  sprintf((char *)buffer+2+8+3+1,"%03d\x9b",int(len)); // plus terminating EOL
  bufptr = buffer;         // reset the buffer pointer

  return 0x01;
}
///

/// HDevice::FilterAux1
// Extract/modify AUX1 with the /x file name modifiers
void HDevice::FilterAux1(char *name,UBYTE &aux1)
{
  char *modifier;
  //
  // Check whether there is a comma in the name, remove it.
  modifier = strchr(name,',');
  if (modifier) // cut off.
    *modifier = 0;
  //
  // Scan the file name/pattern for the "/" modifier indicator.
  modifier = strchr(name,'/');
  if (modifier) {
    // We have it. Check all possibilities. We do not support
    // the Dos 2.XL /1../9 modifiers since duplicate file names
    // are not supported by any sane Os.
    // Cut off here.
    *modifier = 0;
    do {
      switch(*++modifier) {
      case 'a':
      case 'A':
	// Append mode. Set bit 0
	aux1 |= 0x01;
	break;
      case 'd':
      case 'D':
	// Directory mode. Set bit 1
	aux1 |= 0x02;
	break;
      case 'n':
      case 'N':
	// Disable run mode for binary load.
	aux1 &= 0x8f;
	break;
      case '\0':
	// End of string, return.
	return;
      }
    } while(true);
  }
}
///

/// HDevice::Open
// Open a channel towards the emulated disk access handler
UBYTE HDevice::Open(UBYTE channel,UBYTE unit,char *name,UBYTE aux1,UBYTE)
{
  char fullname[256];
  UBYTE result = 0x01;
  struct HandlerChannel *ch;
  const char *mode = NULL; // shut up
  const char *newname;

  // We only support four units here.
  if (unit > 4 || unit < 1) {
    return 0x82;
  }
  if (BaseDir[unit-1] == NULL) {
    return 0x82;
  }
    
  ch = Buffer[channel];
  if (ch) {
    return 0x81; // channel is open
  }  
  FilterAux1(name,aux1);
  if (!HandlerChannel::IsValid(name)) {
    return 0xa5; // invalid file name
  }
  strcpy(fullname,BaseDir[unit-1]);
  strcat(fullname,"/");
  
  ch = new struct HandlerChannel(aux1,BaseDir[unit-1]);
  Buffer[channel] = ch;   // install it so it doesn't get lost
  switch(aux1) {
  case 4: 
  case 12:
  case 13:
    switch(aux1) {
    case 4:
    case 5:
      mode = "rb";
      break;
    case 12:
    case 13:
      mode = "r+b";
      break;
    }
    // Open for read. Find the file, then open.
    if ((result = ch->MatchFirst(name)) == 0x01) {
      // Found a match, now try to open the file.
      strcat(fullname,de_name(ch->fib));
      if (!(ch->stream = fopen(fullname,mode))) {
	result = ch->AtariError(errno);
      }
    }
    break;
  case 6:
  case 7:
    // Just keep the pattern here.
    ch->pattern = new char[strlen(name) + 1];
    strcpy(ch->pattern,name);
    result = 0x01; // ignore unknown matches
    break;
  case 8:
  case 9:
    // Open for write. Check whether we have a wildcard. If so, open this
    // file. Otherwise, open the plain file directly.
    switch(aux1) {
    case 8:
      mode = "wb";
      break;
    case 9:
      mode = "ab";
      break;
    }
    newname = name;
    if (ch->IsWild(newname)) {
      if ((result = ch->MatchFirst(newname) == 0x01)) {
	newname = de_name(ch->fib);
      } 
    }
    if (result == 0x01) {
      strcat(fullname,newname);
      if (!(ch->stream = fopen(fullname,mode))) {
	result = 0xaa;
      }
    }
    break;
  }
  //
  // Check whether the result is fine. If not, dispose the channel.
  if (result != 0x01) {
    delete ch;
    Buffer[channel] = NULL;
  }
  return result;
}
///

/// HDevice::Close
// Close a device from open
UBYTE HDevice::Close(UBYTE channel)
{
  struct HandlerChannel *ch;
  
  ch = Buffer[channel];
  if (ch == NULL) {
    return 0x01; // channel is closed: ignore
  }
  //
  // Closing the stream and all that is done in the handler channel.
  delete ch;
  Buffer[channel] = NULL;
  return 0x01;
}
///

/// HDevice::Get
// Read a value, return an error or fill in the value read
UBYTE HDevice::Get(UBYTE channel,UBYTE &value)
{
  UBYTE result = 0x01;
  struct HandlerChannel *ch;
  // Read a value from the channel should we have one.
  ch = Buffer[channel];
  if (ch == NULL) {
    return 0x85; // channel not open
  }
  // Check whether we may read at all.
  if ((ch->openmode & 0x04) == 0) {
    return (ch->lasterror = 0x83); // only open for output
  }
  //
  // Now check whether this is a plain read or a read from the directory. The latter
  // is more complicated.
  if (ch->openmode & 0x02) {
    // Is directory read. Check whether we need to refill the buffer.
    if (ch->buffer == NULL || *ch->bufptr == 0) {
      if (ch->buffer == NULL) {
	// Is a directory scan start
	ch->buffer = new UBYTE[32];
	// Find the first match. Passing NULL here is fine as we
	// stored the pattern before.
	result     = ch->MatchFirst(NULL);
	// Convert a "not found" error into the EOF
	if (result == 0xaa)
	  result = 0x88;
	// Convert the direntry contents to the atari readable format and return it.
      } else {
	// End of string reached, match the next entry. For that, check
	// whether we have a next entry to deliver. If not, signal an EOF.
	if (ch->dirstream == NULL) { 
	  // EOF reached last time. Abort now.
	  result = 0x88; // EOF;
	} else {
	  result = ch->MatchNext();
	}
      }
      // End buffer refill. Now check whether the refill worked. If so, convert
      // to directory entry.
      // Check whether we could countinue the search. If so, fill in the entry now.
      if (result == 0x01) {
	// Still valid, fill in some data.
	result = ch->ToDirEntry();
      } else if (ch->dirstream && (result == 0x88 || result == 0xaa)) {
	// If the result is an EOF, return the free sector count.
	// There is still plenty of it.
	strcpy((char *)ch->buffer,"999 FREE SECTORS\x9b");
	ch->bufptr = ch->buffer;
	closedir(ch->dirstream);
	ch->dirstream = NULL;
	result     = 0x01;
      }
    }
    // Still something to read. Return it.
    if (result == 0x01)
      value  = *ch->bufptr++;
  } else {
    int c;
    // Regular read. 
    c = getc(ch->stream);
    if (c == EOF) {
      // Problem: How are we able to tell an error case from
      // an EOF case apart?
      result = 0x88;
    } else {
      value  = (UBYTE)c;
      //
      // Check whether the next read is an EOF. If so, use a
      // different result.
      c = getc(ch->stream);
      if (c == EOF) {
	result = 0x03;
      } else {
	result = 0x01;
      }
      ungetc(c,ch->stream);
    }
  }

  return (ch->lasterror = result);
}
///

/// HDevice::Put
// Write a byte into a file, buffered.
UBYTE HDevice::Put(UBYTE channel,UBYTE value)
{
  struct HandlerChannel *ch;
  // Read a value from the channel should we have one.
  ch = Buffer[channel];
  if (ch == NULL) {
    return 0x85; // channel not open
  }
  // Check whether we may write at all.
  if ((ch->openmode & 0x08) == 0) {
    return (ch->lasterror = 0x87); // only open for input
  }
  //
  // This is fairly simple, just put the byte.
  if (putc(value,ch->stream) == EOF) {
    return (ch->lasterror = ch->AtariError(errno));
  }
  return (ch->lasterror = 0x01);
}
///

/// HDevice::Status
// Get the status of the HDevice
// Write a byte into a file, buffered.
UBYTE HDevice::Status(UBYTE channel)
{
  struct HandlerChannel *ch;
  // Read a value from the channel should we have one.
  ch = Buffer[channel];
  if (ch == NULL) {
    return 0x85; // channel not open
  }
  // If we had a failure before, return it.
  if (ch->lasterror != 0x01)
    return ch->lasterror;
  //
  // If the channel is open for reading, check whether we may detect an EOF.
  if (ch->openmode & 0x02) {
    // Open for directory. This is a bit hairy. Check whether we are at
    // the end of the directory.
    if (ch->buffer) {
      // Is already open. Check now if we're at the end of the buffer.
      if (*ch->bufptr == 0) {
	// End of the scan reached.
	if (ch->dirstream == 0)
	  return 0x03; // an EOF will happen
      }
    }
    return 0x01;
  }
  if (ch->openmode & 0x04) {
    // Open for reading. Check whether we'll get an EOF with ungetc.
    int c;
    c = getc(ch->stream);
    ungetc(c,ch->stream);
    if (c == EOF)
      return 0x03; // EOF will happen next.
    return 0x01;   // will be fine.
  }
  return 0x01;  // is fine.
}
///

/// HDevice::Special
// A lot of miscellaneous commands in here.
UBYTE HDevice::Special(UBYTE channel,UBYTE unit,class AdrSpace *adr,UBYTE cmd,
		       ADR mem,UWORD,UBYTE aux[6])
{
  UBYTE result = 0x01;
  struct HandlerChannel *ch;
  char path[256];
  char *comma;
  //
  // Whether we need a channel here or not depends on the specific CIO command.
  switch (cmd) {
  case 0x20: // Rename
  case 0x21: // Delete
  case 0x22: // Validate pathname
  case 0x23: // Protect
  case 0x24: // Unprotect 
  case 0x28: // Resolve wildcard
    // All these require a temporary channel.
    if ((ch = Buffer[8])) {
      // This shouldn't actually happen
      return 0x81;
    }
    // Make sure the unit is valid if we need a new channel.
    if (unit > 4 || unit < 1) {
      return 0x82;
    }  
    if (BaseDir[unit-1] == NULL) {
      return 0x82;
    }
 
    //
    ch          = new struct HandlerChannel(0x00,BaseDir[unit-1]);
    Buffer[8]   = ch;
    channel     = 8;
    ExtractFileName(adr,mem,path,sizeof(path));
    comma       = strchr(path,',');
    FilterAux1(path,aux[0]);
    switch(cmd) {
    case 0x20:
      if (comma)
	*comma = ',';
      result  = Rename(ch,path);
      break;
    case 0x21:
      result  = Delete(ch,path);
      break;
    case 0x22:
      result  = Validate(ch,path);
      break;
    case 0x23:
      result  = Protect(ch,path);
      break;
    case 0x24:
      result  = Unprotect(ch,path);
      break;
    case 0x28:
      // Special hack: If AUX1 indicates directory
      // access, do not list all directories.
      if (aux[0] & 0x02) {
	result = 0xa0;
      } else {
	result  = Resolve(ch,path,aux[1]); // the second aux counts the files
      }
      break;
    }
    delete ch;
    Buffer[8] = NULL;
    return result;
  case 0x29: // Binary load
    // This is special in the sense that it does require
    // a temporary channel, but we don't allocate it but
    // rather leave this to the implicit open of the
    // channel.    
    ExtractFileName(adr,mem,path,sizeof(path));
    FilterAux1(path,aux[0]);
    if ((ch = Buffer[8])) {
      // This shouldn't actually happen
      return 0x81;
    }
    // Make sure the unit is valid if we need a new channel.
    if (unit > 4 || unit < 1) {
      return 0x82;
    }  
    if (BaseDir[unit-1] == NULL) {
      return 0x82;
    }
    return BinaryLoad(adr,channel,aux[0]);
  case 0x25: // Point
  case 0x26: // Note
    // These two require an open XIO to operate on.
    if (!(ch = Buffer[channel])) {
      return 0x85;
    }
    ch        = Buffer[channel];
    // We must ensure that this is not a directory as we can't point/note
    // here.
    if (ch->openmode & 0x02) {
      // This won't work!
      return 0xa6;
    } else {
      LONG sector = 0; // shut up the compiler
      //
      switch(cmd) {
      case 0x25:
	// Point. Get the offset from AUX3..AUX5. We use the more orthogonal
	// DOS 3 addressing here.
	sector = aux[2] | (aux[3] << 8L) | (aux[4] << 16L);
	result = Point(ch,sector);
	break;
      case 0x26:
	// Note. Operate similarly, but place the result into aux[3]..aux[5]
	result = Note(ch,sector);
	aux[2] = UBYTE(sector);
	aux[3] = UBYTE(sector >> 8);
	aux[4] = UBYTE(sector >> 16);
	break;
      }
      return result;
    }
    // DOS supports more commands, but H: does not.
    // There is no binary load, and no Format
    //
  }
  return 0xa8; /* Unknown command */
}
///

/// HDevice::Point
// Set the file pointer
UBYTE HDevice::Point(struct HandlerChannel *ch,LONG sector)
{
  if (fseek(ch->stream,sector,SEEK_SET) < 0) {
    return ch->AtariError(errno);
  }
  return 0x01;
}
///

/// HDevice::Note
// Return the file pointer
UBYTE HDevice::Note(struct HandlerChannel *ch,LONG &sector)
{
  LONG off;

  off = ftell(ch->stream);
  if (off < 0) {
    return ch->AtariError(errno);
  } 
  sector = off;

  return 0x01;
}
///

/// HDevice::Rename
// Rename a file or a couple of files
UBYTE HDevice::Rename(struct HandlerChannel *ch,char *pattern)
{
  UBYTE result;
  struct stat info;
  char sourcepath[256],destpath[256];
  char *dst;
  //
  // Get the target file name: This can be found behind the "," in the
  // path.
  dst = strchr(pattern,',');
  // If there is none, the rename is invalid.
  if (dst == NULL) {
    return 0xa5; // Invalid file name
  }
  *dst++ = '\0';   // Separate the file names here.
  if (!ch->IsValid(pattern) || !ch->IsValid(dst)) {
    return 0xa5; // Invalid
  }
  // Check whether the target is a wild card. This cannot possibly work.
  if (ch->IsWild(dst))
    return 0xa5;
  //
  // Build the destination name
  if (snprintf(destpath,sizeof(destpath),"%s/%s",ch->basedir,dst) > int(sizeof(destpath)))
    return 0xa5; // Invalid file name

  result = ch->MatchFirst(pattern);
  while(result == 0x01) {
    // Check whether the target exists already. If so, abort.
    if (stat(destpath,&info) == 0) {
      // Result is fine. Hence, the file exists.
      return 0xa5;
    } else {
      if (errno != ENOENT) {
	// Some other error.
	return ch->AtariError(errno);
      }
    }
    // Worked so far. Now run the rename.
    if (snprintf(sourcepath,sizeof(sourcepath),"%s/%s",ch->basedir,de_name(ch->fib)) > int(sizeof(sourcepath)))
      return 0xa5; // invalid file name
    if (rename(sourcepath,destpath) < 0) {
      return ch->AtariError(errno);
    }
    result = ch->MatchNext();
  }
  return UBYTE((result == 0x88)?(1):(result));
}
///

/// HDevice::Delete
// Delete (unlink) a file
UBYTE HDevice::Delete(struct HandlerChannel *ch,char *pattern)
{  
  struct stat info;
  char target[256];
  UBYTE result;

  if (!ch->IsValid(pattern)) {
    return 0xa5;
  }

  result = ch->MatchFirst(pattern);
  while (result == 0x01) {
    if (snprintf(target,sizeof(target),"%s/%s",ch->basedir,de_name(ch->fib)) > int(sizeof(target)))
      return 0xa5; // Invalid file name
    // Now check whether this file is "protected". If so,
    // deliver an error and do not delete it.
    if (stat(target,&info) == -1) {
      return ch->AtariError(errno);
    }
    if ((info.st_mode & S_IWUSR) == 0) {
      // File is "locked". Actually, under Linux,
      // we could write to it. But we shouldn't
      // for the atari rules.
      return 0xa7; // is write protected.
    }

    if (remove(target) < 0) {
      return ch->AtariError(errno);
    }
    result = ch->MatchNext();
  }
  return UBYTE((result == 0x88)?(1):(result));
}
///

/// HDevice::Validate
UBYTE HDevice::Validate(struct HandlerChannel *ch,char *pattern)
{
  // Check whether the name is valid.
  if (!ch->IsValid(pattern)) {
    return 0xa5;
  }

  return 0x01;
}
///

/// HDevice::Resolve
// Resolve a wildcard
UBYTE HDevice::Resolve(struct HandlerChannel *ch,char *pattern,UBYTE counter)
{
  UBYTE result;
  // Resolve the filename. Advance if the counter is positive.
  if ((result = ch->MatchFirst(pattern)) == 0x01) { 
    const char *name;
    class AdrSpace *adr; 
    ADR pat;
    //
    // Get where the source data should be
    adr = Machine->MMU()->CPURAM();
    // Counter counts from one
    while(counter > 1) {
      if ((result = ch->MatchNext()) != 0x01) {
	return result;
      }
      counter--;
    }
    // Now replace the file name in the source buffer
    pat = adr->ReadWord(0x24); 
    // IOCB Zeropage data buffer, advance to the colon
    while(adr->ReadByte(pat) != ':') {
      pat++;
    }
    // Now insert the resolved filename here.
    name = de_name(ch->fib);
    while(*name) {
      // Advance to the next character
      pat++;
      adr->WriteByte(pat,toupper(*name));
      name++;
    }
    pat++;
    adr->WriteByte(pat,0x9b); // Terminate the sequence
    // 
    return 0x01;
  }
  return result;
}
///

/// HDevice::Protect
// Make a file un-writeable
UBYTE HDevice::Protect(struct HandlerChannel *ch,char *pattern)
{
  struct stat info;
  char target[256];
  UBYTE result;

  if (!ch->IsValid(pattern)) {
    return 0xa5;
  }

  result = ch->MatchFirst(pattern);
  while (result == 0x01) {
    if (snprintf(target,sizeof(target),"%s/%s",ch->basedir,de_name(ch->fib)) > int(sizeof(target)))
      return 0xa5; // Invalid file name
    // Get the info for this file.
    if (stat(target,&info) < 0) {
      return ch->AtariError(errno);
    }
#if HAVE_CHMOD
    if (chmod(target,info.st_mode & (~S_IWUSR)) < 0) {
      return ch->AtariError(errno);
    }
#endif
    result = ch->MatchNext();
  } 
  return UBYTE((result == 0x88)?(1):(result));
}
///

/// HDevice::Unprotect
// Make a file writeable again
UBYTE HDevice::Unprotect(struct HandlerChannel *ch,char *pattern)
{
  struct stat info;
  char target[256];
  UBYTE result;

  if (!ch->IsValid(pattern)) {
    return 0xa5;
  }

  result = ch->MatchFirst(pattern);
  while (result == 0x01) {
    if (snprintf(target,sizeof(target),"%s/%s",ch->basedir,de_name(ch->fib)) > int(sizeof(target)))
      return 0xa5; // Invalid file name
    // Get the info for this file.
    if (stat(target,&info) < 0) {
      return ch->AtariError(errno);
    }
#if HAVE_CHMOD
    if (chmod(target,info.st_mode | S_IWUSR) < 0) {
      return ch->AtariError(errno);
    }
#endif
    result = ch->MatchNext();
  }
  return UBYTE((result == 0x88)?(1):(result));
}
///

/// HDevice::BinaryLoad
// Initialize the binary load
UBYTE HDevice::BinaryLoad(class AdrSpace *adr,UBYTE channel,UBYTE aux)
{
  // Deliver this to the callback patch.
  CallBackPatch->LaunchBinaryLoad(adr,channel,aux);
  //
  // This return code won't matter either...
  return 0x01;
}
///

/// HDevice::Reset
// Close all open streams and flush the contents
void HDevice::Reset(void)
{
  int channel;

  for(channel = 0;channel <= 8;channel++) {
    // Closing the streams and all that happens in the destructor of
    // the buffer.
    delete Buffer[channel];
    Buffer[channel] = NULL;
  }
  //
  // Ditto for the callback.
  CallBackPatch->Reset();
}
///

