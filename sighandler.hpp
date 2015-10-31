/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: sighandler.hpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** In this module: SigInt signal handler class
 **********************************************************************************/

#ifndef SIGHANDLER_HPP
#define SIGHANDLER_HPP

/// Includes
#include "types.hpp"
///

/// Forwards
class Machine;
///

/// Class SigHandler
// This signal handler class installs a private ^C handler that runs the
// monitor on request.
class SigHandler {
  //
  class Machine *machine;
  //
public:
  // The constructor installs the signal handler as well
  SigHandler(class Machine *mach);
  //
  // The destructor removes the signal handler
  ~SigHandler(void);
  //
  // External call: The signal handler will enter here to catch the signal
  void Signal(void);
  //
  // Restore the original core dump routine.
  static void RestoreCoreDump(void);
};
///

///
#endif
