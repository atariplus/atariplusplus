/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: main.hpp,v 1.5 2015/05/21 18:52:40 thor Exp $
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

