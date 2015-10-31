/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: charmap.hpp,v 1.5 2015/05/21 18:52:37 thor Exp $
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
