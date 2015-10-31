/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: no_frontend.cpp,v 1.2 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: A dummy frontend not displaying anything.
 **********************************************************************************/

/// Includes
#include "no_frontend.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "machine.hpp"
#include "antic.hpp"
///

/// No_FrontEnd::No_FrontEnd
No_FrontEnd::No_FrontEnd(class Machine *mach)
  : AtariDisplay(mach,0), 
    InputBuffer(new UBYTE[Antic::DisplayModulo])
{
}
///

// No_FrontEnd::~No_FrontEnd
No_FrontEnd::~No_FrontEnd(void)
{
  delete[] InputBuffer;
}
///

/// No_FrontEnd::DisplayStatus
// Print the status.
void No_FrontEnd::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("No_FrontEnd Status:\n"
                   "Front end installed and working.\n");
}
///
