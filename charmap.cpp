/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: charmap.cpp,v 1.2 2004-10-11 20:47:46 thor Exp $
 **
 ** In this module: Atari default character map
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "charmap.hpp"
///

/// ToANTIC
// Convert an ASCII character to its ANTIC screen representation
UBYTE ToANTIC(char c)
{
  UBYTE out = 0;

  // Mask out the inverse map. We handle this separately.
  c &= 0x7f;

  switch(c & 0x60) {
  case 0x00:
    out |= UBYTE(0x40 | (c & 0x1f));
    break;
  case 0x20:
    out |= UBYTE(0x00 | (c & 0x1f));
    break;
  case 0x40:    
    out |= UBYTE(0x20 | (c & 0x1f));
    break;
  case 0x60:
    out |= UBYTE(0x60 | (c & 0x1f));
    break;
  }
  return out;
}
///

/// The Atari default character map.
// The character map is a global object linked to the system.
const UBYTE CharMap[0x0400] = 
  {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x00,
    0x00,0x66,0x66,0x66,0x00,0x00,0x00,0x00,
    0x00,0x66,0xFF,0x66,0x66,0xFF,0x66,0x00,
    0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00,
    0x00,0x66,0x6C,0x18,0x30,0x66,0x46,0x00,
    0x1C,0x36,0x1C,0x38,0x6F,0x66,0x3B,0x00,
    0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00,
    0x00,0x0E,0x1C,0x18,0x18,0x1C,0x0E,0x00,
    0x00,0x70,0x38,0x18,0x18,0x38,0x70,0x00,
    0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,
    0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30,
    0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,
    0x00,0x06,0x0C,0x18,0x30,0x60,0x40,0x00,
    0x00,0x3C,0x66,0x6E,0x76,0x66,0x3C,0x00,
    0x00,0x18,0x38,0x18,0x18,0x18,0x7E,0x00,
    0x00,0x3C,0x66,0x0C,0x18,0x30,0x7E,0x00,
    0x00,0x7E,0x0C,0x18,0x0C,0x66,0x3C,0x00,
    0x00,0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x00,
    0x00,0x7E,0x60,0x7C,0x06,0x66,0x3C,0x00,
    0x00,0x3C,0x60,0x7C,0x66,0x66,0x3C,0x00,
    0x00,0x7E,0x06,0x0C,0x18,0x30,0x30,0x00,
    0x00,0x3C,0x66,0x3C,0x66,0x66,0x3C,0x00,
    0x00,0x3C,0x66,0x3E,0x06,0x0C,0x38,0x00,
    0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x00,
    0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x30,
    0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00,
    0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,
    0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00,
    0x00,0x3C,0x66,0x0C,0x18,0x00,0x18,0x00,
    0x00,0x3C,0x66,0x6E,0x6E,0x60,0x3E,0x00,
    0x00,0x18,0x3C,0x66,0x66,0x7E,0x66,0x00,
    0x00,0x7C,0x66,0x7C,0x66,0x66,0x7C,0x00,
    0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00,
    0x00,0x78,0x6C,0x66,0x66,0x6C,0x78,0x00,
    0x00,0x7E,0x60,0x7C,0x60,0x60,0x7E,0x00,
    0x00,0x7E,0x60,0x7C,0x60,0x60,0x60,0x00,
    0x00,0x3E,0x60,0x60,0x6E,0x66,0x3E,0x00,
    0x00,0x66,0x66,0x7E,0x66,0x66,0x66,0x00,
    0x00,0x7E,0x18,0x18,0x18,0x18,0x7E,0x00,
    0x00,0x06,0x06,0x06,0x06,0x66,0x3C,0x00,
    0x00,0x66,0x6C,0x78,0x78,0x6C,0x66,0x00,
    0x00,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,
    0x00,0x63,0x77,0x7F,0x6B,0x63,0x63,0x00,
    0x00,0x66,0x76,0x7E,0x7E,0x6E,0x66,0x00,
    0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00,
    0x00,0x7C,0x66,0x66,0x7C,0x60,0x60,0x00,
    0x00,0x3C,0x66,0x66,0x66,0x6C,0x36,0x00,
    0x00,0x7C,0x66,0x66,0x7C,0x6C,0x66,0x00,
    0x00,0x3C,0x60,0x3C,0x06,0x06,0x3C,0x00,
    0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x00,
    0x00,0x66,0x66,0x66,0x66,0x66,0x7E,0x00,
    0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,
    0x00,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00,
    0x00,0x66,0x66,0x3C,0x3C,0x66,0x66,0x00,
    0x00,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,
    0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00,
    0x00,0x1E,0x18,0x18,0x18,0x18,0x1E,0x00,
    0x00,0x40,0x60,0x30,0x18,0x0C,0x06,0x00,
    0x00,0x78,0x18,0x18,0x18,0x18,0x78,0x00,
    0x00,0x08,0x1C,0x36,0x63,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,
    0x00,0x36,0x7F,0x7F,0x3E,0x1C,0x08,0x00,
    0x18,0x18,0x18,0x1F,0x1F,0x18,0x18,0x18,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x18,0x18,0x18,0xF8,0xF8,0x00,0x00,0x00,
    0x18,0x18,0x18,0xF8,0xF8,0x18,0x18,0x18,
    0x00,0x00,0x00,0xF8,0xF8,0x18,0x18,0x18,
    0x03,0x07,0x0E,0x1C,0x38,0x70,0xE0,0xC0,
    0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x07,0x03,
    0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF,
    0x00,0x00,0x00,0x00,0x0F,0x0F,0x0F,0x0F,
    0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF,
    0x0F,0x0F,0x0F,0x0F,0x00,0x00,0x00,0x00,
    0xF0,0xF0,0xF0,0xF0,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,
    0x00,0x00,0x00,0x00,0xF0,0xF0,0xF0,0xF0,
    0x00,0x1C,0x1C,0x77,0x77,0x08,0x1C,0x00,
    0x00,0x00,0x00,0x1F,0x1F,0x18,0x18,0x18,
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,
    0x18,0x18,0x18,0xFF,0xFF,0x18,0x18,0x18,
    0x00,0x00,0x3C,0x7E,0x7E,0x7E,0x3C,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
    0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,
    0x00,0x00,0x00,0xFF,0xFF,0x18,0x18,0x18,
    0x18,0x18,0x18,0xFF,0xFF,0x00,0x00,0x00,
    0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
    0x18,0x18,0x18,0x1F,0x1F,0x00,0x00,0x00,
    0x78,0x60,0x78,0x60,0x7E,0x18,0x1E,0x00,
    0x00,0x18,0x3C,0x7E,0x18,0x18,0x18,0x00,
    0x00,0x18,0x18,0x18,0x7E,0x3C,0x18,0x00,
    0x00,0x18,0x30,0x7E,0x30,0x18,0x00,0x00,
    0x00,0x18,0x0C,0x7E,0x0C,0x18,0x00,0x00,
    0x00,0x18,0x3C,0x7E,0x7E,0x3C,0x18,0x00,
    0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00,
    0x00,0x60,0x60,0x7C,0x66,0x66,0x7C,0x00,
    0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00,
    0x00,0x06,0x06,0x3E,0x66,0x66,0x3E,0x00,
    0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00,
    0x00,0x0E,0x18,0x3E,0x18,0x18,0x18,0x00,
    0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C,
    0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x00,
    0x00,0x18,0x00,0x38,0x18,0x18,0x3C,0x00,
    0x00,0x06,0x00,0x06,0x06,0x06,0x06,0x3C,
    0x00,0x60,0x60,0x6C,0x78,0x6C,0x66,0x00,
    0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,
    0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00,
    0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00,
    0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00,
    0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60,
    0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06,
    0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00,
    0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00,
    0x00,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00,
    0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00,
    0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00,
    0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00,
    0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00,
    0x00,0x00,0x66,0x66,0x66,0x3E,0x0C,0x78,
    0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00,
    0x00,0x18,0x3C,0x7E,0x7E,0x18,0x3C,0x00,
    0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
    0x00,0x7E,0x78,0x7C,0x6E,0x66,0x06,0x00,
    0x08,0x18,0x38,0x78,0x38,0x18,0x08,0x00,
    0x10,0x18,0x1C,0x1E,0x1C,0x18,0x10,0x00,
  };
///
