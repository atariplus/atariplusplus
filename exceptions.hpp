/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: exceptions.hpp,v 1.22 2003/12/20 11:12:38 thor Exp $
 **
 ** In this module: Definition of emulator specific exceptions
 **********************************************************************************/

#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

/// Includes
#include "types.hpp"
#include "string.hpp"
#include "stdlib.hpp"
#include <errno.h>
#include <stdarg.h>
///

/// Class ExceptionPrinter
// This defines a class that can print output to somewhere
class ExceptionPrinter {
  //
  //
public:
  ExceptionPrinter(void)
  {}
  //
  virtual ~ExceptionPrinter(void)
  {}
  //
  //
  // The following method will be called to print out the exception.
  virtual void PrintException(const char *fmt,...) PRINTF_STYLE = 0;
  //
};
///

/// Class AsyncEvent
// These "exceptions" are thrown to leave the CPU internal immediately
class AsyncEvent {
public:
  enum EventType {
    Ev_Exit,              // Leave emulator immediately, no exception
    Ev_WarmStart,         // WarmStart the emulator, no exception
    Ev_ColdStart,         // ColdStart the emulator, no exception
    Ev_EnterMenu          // Run the menu, no exception
  };
private:
  EventType  Type;
  //
public:
  AsyncEvent(const class AsyncEvent &ev)
    : Type(ev.Type)
  { }
  //
  AsyncEvent(EventType type)
    : Type(type)
  { }
  //
  ~AsyncEvent(void)
  { }
  //
  //
  EventType TypeOf(void) const
  {
    return Type;
  }
};
///

/// AsyncEvent Macros you might find useful
#define EXIT      throw AsyncEvent(AsyncEvent::Ev_Exit)
#define WARMSTART throw AsyncEvent(AsyncEvent::Ev_WarmStart)
#define COLDSTART throw AsyncEvent(AsyncEvent::Ev_ColdStart)
///

/// Class AtariException
// These classes are "thrown" on an exception
class AtariException {
public:
  enum ExType {
    //
    // Real exceptions start
    Ex_IoErr,             // IoError. The system specific errnum below says more
    Ex_InvalidParameter,  // a parameter was invalid
    Ex_OutOfRange,        // a parameter was out of range
    Ex_ObjectExists,      // created an object that exists already
    Ex_ObjectDoesntExist, // object did not yet exist
    Ex_NotImplemented,    // failed because method is not available (internal)
    Ex_NoMem,             // run out of memory
    Ex_PhaseError,        // two phases returned an unconsistent result
    Ex_BadPrefs,          // thrown only by the menu class
    Ex_BadSnapShot        // generated by the snapshot reader/writer on error
  };
private:
  //
  const char *file;       // name of the source file affected
  const char *object;     // name of the object that threw
  const char *reason;     // further textual description
  int         line;       // line number that caused the exception
  ExType      type;       // type of the exception
  const char *ioerr;      // system specific IO exception
  char       *buffer;     // for complex error messages: An allocated buffer
  //
  // For debugging purposes: This gets called 
  // whenever an exception happens.
#if CHECK_LEVEL > 0 || DEBUG_LEVEL > 0
  void DebugHook(void);
#endif
  //
public:
  // Exception cloner: We must also duplicate the buffer as we release it
  // within the destructor of the origin
  AtariException(const AtariException &origin);
  //
  //
  // Generate an exception by an emulator internal cause
  AtariException(char *,ExType why,const char *who,const char *source,
		 int where,const char *when);
  //
  // The same, but without a constant source we need to copy
  AtariException(char *,ExType why,const char *who,const char *source,
		 int where,char *when);
  //
  // Generate an exception by an io error
  AtariException(int,const char *io,const char *who,const char *when);
  //
  // The same, but with non-constant reason argument that must be copied.
  AtariException(int,const char *io,const char *who,char *when);
  //
  // Generate a complex IoError exception
  AtariException(const char *io,const char *who,const char *fmt,...) LONG_PRINTF_STYLE(4,5);
  //
  // Destructor
  ~AtariException(void);
  //
  // Print an exception reason into a string for informing the user
  void PrintException(class ExceptionPrinter &to) const;
  //
  // Return the type of the exception for selective handling
  ExType TypeOf(void) const
  {
    return type;
  }
};
///

/// Exception macros
#define Throw(why,object,txt) throw(AtariException(NULL,AtariException::Ex_ ## why,object,__FILE__,__LINE__,txt))
#define ThrowIo(object,desc)  throw(AtariException(0,strerror(errno),object,desc))
///

///
#endif
    
