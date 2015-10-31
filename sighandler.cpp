/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sighandler.cpp,v 1.11 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: SigInt signal handler class
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "sighandler.hpp"
#include "machine.hpp"
#if HAVE_SIGNAL_H && (SIG_ARG_TYPE_INT || SIG_ARG_TYPE_DOTS)
#define USE_SIGNAL
extern "C" {
#include <signal.h>
}
#endif
///

/// Protos
static SigHandler *sighdn;
///

/// sigint_handler: The low-level signal handler
// The IRIX compiler requires here the "..." as argument, and not int. 
// Hence, we better check for the return type in configure.in and make use
// of the result here.
#ifdef USE_SIGNAL
#if defined(SIG_ARG_TYPE_INT)
static RETSIGTYPE sigint_handler(int)
#elif defined(SIG_ARG_TYPE_DOTS)
static RETSIGTYPE sigint_handler(...)
#endif
{
#if HAVE_SIGNAL_H
  sighdn->Signal(); // run the real frontend
  signal(SIGINT,sigint_handler); // re-install. Necessitity depends on the system
#endif
}
#endif
///

/// SigHandler::SigHandler
SigHandler::SigHandler(class Machine *mach)
  : machine(mach)
{
#ifdef USE_SIGNAL
  // Now install the ^C handler here
  signal(SIGINT, sigint_handler);
  // We must keep the pointer to this class somewhere, unfortunately. Yuck!
  sighdn = this;
#endif
}
///

/// SigHandler::~SigHandler
// This destructor also removes the signal handler
SigHandler::~SigHandler(void)
{
#ifdef USE_SIGNAL
  signal(SIGINT,SIG_DFL);
#endif
}
///

/// SigHandler::Signal
// This is the signal/interrupt entrance point of the class
void SigHandler::Signal(void)
{
  machine->SigBreak();
}
///

/// SIGSEGV handler
// override the SIGSEGV handler the SDL installs
// because it doesn't allow us to analyze the core
// dump in case of errors.
#ifdef USE_SIGNAL
void SigHandler::RestoreCoreDump(void)
{
  signal(SIGSEGV,SIG_DFL);
}
#else
void SigHandler::RestoreCoreDump(void)
{
}
#endif
///
