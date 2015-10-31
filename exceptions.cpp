/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: exceptions.cpp,v 1.26 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of emulator specific exceptions
 **********************************************************************************/

/// Includes
#include "types.h"
#include "exceptions.hpp"
#include "stdio.hpp"
#include "new.hpp"
// Check for vprintf function 
// There is no replacement for it. We must have it.
#if !HAVE_VPRINTF
#error "the system does not provide vprintf"
#endif
#if !HAVE_STDARG_H
#error "the system does not provide the stdarg header"
#endif
///

/// AtariException::PrintAtariException
// Print an exception reason into a string for informing the user
void AtariException::PrintException(class ExceptionPrinter &to) const
{
  if (type == Ex_IoErr) {
#if CHECK_LEVEL > 0
    to.PrintException("Failure: %s in %s:\n%s\n",
		      ioerr,object,reason);
#else
     to.PrintException("Failure: %s:\n%s\n",
		      ioerr,reason);
#endif
 } else {
    const char *exname;
    switch(type) {
    case Ex_InvalidParameter:
      exname = "invalid parameter";
      break;
    case Ex_OutOfRange:
      exname = "parameter out of range";
      break;
    case Ex_ObjectExists:
      exname = "object exists";
      break;
    case Ex_ObjectDoesntExist:
      exname = "object doesn't exist";
      break;
    case Ex_NotImplemented:
      exname = "not implemented";
      break;
    case Ex_NoMem:
      exname = "out of memory";
      break;
    case Ex_PhaseError:
      exname = "phase error";
      break;
    case Ex_BadPrefs:
      exname = "configuration error";
      break;
    case Ex_BadSnapShot:
      exname = "corrupt snapshot";
      break;
    default: // shut up compiler, IoErr is handled above
      exname = "unknown error";
      break;
    }
#if CHECK_LEVEL > 0
    to.PrintException("Failure: %s in %s, file %s line %d :\n%s\n",
		      exname,object,file,line,reason);
#else
    to.PrintException("Failure: %s:\n%s\n",
		      exname,reason);
#endif
  }
}
///

/// AtariException::AtariException (standard constructor)
// Generate an exception by an emulator internal cause
AtariException::AtariException(char *,ExType why,const char *who,const char *source,
			       int where,const char *when)
  : file(source), object(who), reason(when),
    line(where), type(why), ioerr(NULL), buffer(NULL)
{ 
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  DebugHook();
#endif
}
///

/// AtariException::AtariException
// Constructor without any data: No exception.
AtariException::AtariException(void)
  : file(NULL), object(NULL), reason(NULL),
    line(0), type(Ex_NoMem), ioerr(NULL), buffer(NULL)
{
}
///

/// AtariException::AtariException (from IO error)
// Generate an exception by an io error
AtariException::AtariException(int,const char *io,const char *who,const char *when)
  : file(NULL), object(who), reason(when), 
    line(0), type(Ex_IoErr), ioerr(io), buffer(NULL)
{ 
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  DebugHook();
#endif
}
///

/// AtariException::AtariException (varargs version)
AtariException::AtariException(const char *io,const char *who,const char *fmt,...)    
  : file(NULL), object(who), reason(NULL), 
    line(0), type(Ex_IoErr), ioerr(io), buffer(NULL)
{
  int size;
  char buf[4];
  va_list args;

#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  DebugHook();
#endif  
  va_start(args,fmt);
  size   = vsnprintf(buf,1,fmt,args) + 1;
  va_end(args);
  va_start(args,fmt);
  // some buggy versions of vsnprintf return -1 on error/overflow. In this case,
  // truncate to 256 characters here
  if (size <= 0) size = 256;
  buffer = new char[size];
  vsnprintf(buffer,size,fmt,args);
  reason = buffer;
  va_end(args);
}
///

/// AtariException::AtariException (non-const char)
AtariException::AtariException(char *,ExType why,const char *who,const char *source,
			       int where,char *when)
: file(source), object(who), reason(NULL),
  line(where), type(why), ioerr(NULL), buffer(new char[strlen(when) + 1])
{
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  DebugHook();
#endif 
  strcpy(buffer,when);
  reason = buffer;
}  
///

/// AtariException::AtariException (non-const char, IO)
AtariException::AtariException(int,const char *io,const char *who,char *when)
  : file(NULL), object(who), reason(NULL), 
    line(0), type(Ex_IoErr), ioerr(io), buffer(new char[strlen(when) + 1])
{ 
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  DebugHook();
#endif
  strcpy(buffer,when);
  reason = buffer;
}
///

/// AtariException::AtariException (copy constructor)
AtariException::AtariException(const AtariException &origin)
  : buffer(origin.buffer?(new char[strlen(origin.buffer) + 1]):NULL)
{
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  DebugHook();
#endif  
  file   = origin.file;
  object = origin.object;
  line   = origin.line;
  type   = origin.type;
  ioerr  = origin.ioerr;
  reason = origin.reason; // might be NULL or not
  if (buffer) {
    strcpy(buffer,origin.buffer);
    reason = buffer;
  }
}
///

/// AtariException::operator= (assignment constructor)
AtariException &AtariException::operator=(const AtariException &origin)
{
  delete[] buffer;
  buffer = NULL;

  buffer = origin.buffer?(new char[strlen(origin.buffer) + 1]):NULL;
  
  file   = origin.file;
  object = origin.object;
  line   = origin.line;
  type   = origin.type;
  ioerr  = origin.ioerr;
  reason = origin.reason; // might be NULL or not
  if (buffer) {
    strcpy(buffer,origin.buffer);
    reason = buffer;
  }

  return *this;
}
///

/// AtariException::~AtariException
// Destructor
AtariException::~AtariException(void)
{   
  delete[] buffer;
}
///

/// AtariException::DebugHook
// For debugging purposes: This gets called 
// whenever an exception happens.
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
void AtariException::DebugHook(void)
{
  
}
#endif
///
