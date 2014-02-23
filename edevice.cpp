/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: edevice.cpp,v 1.3 2013/11/29 11:27:55 thor Exp $
 **
 ** In this module: H: emulated device for emulated disk access.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "edevice.hpp"
#include "string.hpp"
#include "unistd.hpp"
#include "stdio.hpp"
#include <ctype.h>
#include <errno.h>
#include "cpu.hpp"
#include "mmu.hpp"
#include "adrspace.hpp"
#if HAVE_SELECT && HAVE_FILENO && HAVE_READ
# ifndef HAS_STRUCT_TIMEVAL
struct timeval {
  int tv_sec;       /* Seconds.  */
  int tv_usec;      /* Microseconds.  */
};
# else
#  ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#  else
#   ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#   else
#    include <time.h>
#   endif
#  endif
# endif
#endif
///

/// EDevice::EDevice
// The H device replaces the C: handler we have no use in.
EDevice::EDevice(class Machine *mach,class PatchProvider *p,UBYTE dev)
  : Device(mach,p,dev,dev), device(dev)
{
}
///

/// EDevice::~EDevice
// The H device replaces the C: handler we have no use in.
EDevice::~EDevice(void)
{
}
///

/// EDevice::AtariError
// Translate an Unix error code to something Atari like
UBYTE EDevice::AtariError(int error)
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

/// EDevice::JumpTo
// Jump to the given location after completing the code
void EDevice::JumpTo(ADR dest) const
{
  // nothing interesting happened. Do continue with the looping.
  class CPU *cpu = Machine->CPU();
  ADR stackptr   = 0x100 + cpu->S();  // stack pointer.
  AdrSpace *mem  = Machine->MMU()->CPURAM();
  ADR where      = dest - 1;
  mem->WriteByte(stackptr--,where >> 8  ); // Hi goes first here.
  mem->WriteByte(stackptr--,where & 0xff); // Then Lo
  //
  // Now install the stack pointer back.
  cpu->S()       = stackptr - 0x100;
}
///

/// EDevice::Open
// Open a channel towards the emulated disk access handler
UBYTE EDevice::Open(UBYTE,UBYTE,char *,UBYTE aux1,UBYTE)
{
  // This *should* support mode 13, "auto-read" from screen, but stdio
  // cannot support this.
  if (aux1 & 0x01)
    return 0xb1;

  // Continue at the original Os location.
  JumpTo(Original[0]);
  return 0x01;
}
///

/// EDevice::Close
// Close a device from open
UBYTE EDevice::Close(UBYTE)
{
  return 0x01;
}
///

/// EDevice::Get
// Read a value, return an error or fill in the value read
UBYTE EDevice::Get(UBYTE,UBYTE &value)
{
  int ch;
  //
  do {
#if HAVE_SELECT && HAVE_FILENO && HAVE_READ
    {
      struct timeval tv;
      int fd = fileno(stdin);
      int rc;
      char c;
      fd_set readers;
      FD_ZERO(&readers);
      FD_SET(fd,&readers);
      errno      = 0;
      tv.tv_sec  = 0;
      tv.tv_usec = 0;
      rc    = select(fd+1,&readers,NULL,NULL,&tv);
      if (rc == -1) { 
	// on error, error.
	return AtariError(errno);
      } else if (rc == 0) {
	// nothing interesting happened. Do continue with the looping.
	JumpTo(Machine->CPU()->PC() - 2); // address of the ESC code: Continue to come here...
	return 0x01;
      }
      rc = read(fd,&c,1);
      if (rc == 0)
	continue;
      if (rc < 0)
	return AtariError(errno);
      ch = c;
    }
#else
    errno = 0;
    ch = getchar();
    if (errno)
      return AtariError(errno);
#endif
    //
    // Interpret the result accordingly.
    if (ch == -1) {
      return 0x88; // EOF
    }
    switch(ch) {
    case '\n':
      value = 0x9b; // EOL
      return 0x01;
    case '\t':
      value = 0x7f; // TAB
      return 0x01;
    case '\b':
      value = 0x7e; // BS
      return 0x01;
    case '\f':
      value = 0x7d; // FF=clear screen
      return 0x01;
    case '\a':
      value = 0xfd; // BEL
      return 0x01;
    default:
      if (ch >= ' ' && ch <= 0x7c) {
	// valid ASCII...hopefully
	value = ch;
	return 0x01;
      }
    }
  } while(true);
}
///

/// EDevice::SavePut
// Print the character c safely to screen, return an
// error code.
UBYTE EDevice::SavePut(char c)
{
  errno = 0;
  putchar(c);
  fflush(stdout);
  if (errno) {
    return AtariError(errno);
  }
  return 0x01;
}
///

/// EDevice::Put
// Write a byte into a file, buffered.
UBYTE EDevice::Put(UBYTE,UBYTE value)
{
  if (device == 'K')
    return 0xa8;
  // First translate the character, if possible, then put it on the screen.
  switch(value) {
  case 0x7d:
    SavePut('\f'); // CLS = FF
    break;
  case 0x7e:
    SavePut('\b'); // BS
    break;
  case 0x7f:
    SavePut('\t'); // TAB
    break;
  case 0xfd:
    SavePut('\a'); // BEL
    break;
  case 0x9b:
    SavePut('\n'); // NL
    break;
  default:
    if (value >= ' ' && value <= 0x7c) {
      SavePut(value);
      break;
    } else if (value >= ' ' + 0x80 && value <= 0x7c + 0x80) {
      SavePut(value - 0x80);
      break;
    }
  }
  //
  // All others, including ESC, are ignored. ESC-sequences are considerably different,
  // and terminal dependending, thus would require curses. This is what the curses
  // front-end is good for.
  //
  // Jump to the Os-put to make the result visible.
  JumpTo(Original[3]);
  return 0x01;
}
///

/// EDevice::Status
// Get the status of the EDevice
// Write a byte into a file, buffered.
UBYTE EDevice::Status(UBYTE)
{
  return 0x01;  // is fine.
}
///

/// EDevice::Special
// A lot of miscellaneous commands in here.
UBYTE EDevice::Special(UBYTE,UBYTE,class AdrSpace *,UBYTE,
		       ADR,UWORD,UBYTE [6])
{
  // None supported
  return 0xa8;
}
///

/// EDevice::Reset
// Close all open streams and flush the contents
void EDevice::Reset(void)
{
}
///

