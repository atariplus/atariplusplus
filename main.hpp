/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: main.hpp,v 1.3 2002-09-19 20:46:23 thor Exp $
 **
 ** In this module: Main entrance point of the emulator, main loop.
 **********************************************************************************/

#ifndef MAIN_HPP
#define MAIN_HPP

/// Includes
///

/// Protos
int main(int argc,char **argv);

#if DEBUG_LEVEL > 0
void MainBreakPoint(void);
#endif
///

///
#endif

