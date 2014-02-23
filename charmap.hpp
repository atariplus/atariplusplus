/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: charmap.hpp,v 1.4 2013-03-16 15:08:51 thor Exp $
 **
 ** In this module: Atari default character map
 **********************************************************************************/

#ifndef CHARMAP_HPP

/// Includes
#include "types.h"
#include "types.hpp"
///

/// ToANTIC
// Convert an ASCII character to its ANTIC screen representation
UBYTE ToANTIC(char c);
///

/// The default character map is in the .hpp file
extern const UBYTE CharMap[0x0400];
///

///
#endif
