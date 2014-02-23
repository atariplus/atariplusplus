/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: main.hpp,v 1.4 2013-03-16 15:08:52 thor Exp $
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

