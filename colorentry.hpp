/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: colorentry.hpp,v 1.2 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: Color Entry color map definition
 **********************************************************************************/

#ifndef COLORENTRY_HPP
#define COLORENTRY_HPP

/// Includes
#include "types.hpp"
#include "display.hpp"
///

/// struct ColorEntry
// The colormap of the GTIA: Since it is created by the GTIA for the
// real hardware, the colormap is also defined here.
struct ColorEntry {
  UBYTE                   alpha,red,green,blue;
  AtariDisplay::PackedRGB packed;
  //
  // Pack the contents of the rgb values into a LONG as X requires it,
  // return it.
  AtariDisplay::PackedRGB XPackColor(void) const 
  {
      return packed;
  }
  //
  // Mix this color with the color passed in and return it as packed
  // array.
  AtariDisplay::PackedRGB XMixColor(const struct ColorEntry &o) const
  {
    // The quick'n dirty method with less precision...
    return ((packed & 0xfefefefe) + (o.packed & 0xfefefefe)) >> 1;
    /*
      return ((((LONG)(red   + o.red  ) & ~1UL)<<15) | 
      (((LONG)(green + o.green) & ~1UL)<<7)  | 
      (((LONG)(blue  + o.blue)        )>>1));
    */
  }    
  // Mix this color with the two other color passed in and return it as packed
  // array. The third color has the double weight (intentionally)
  AtariDisplay::PackedRGB XMixColor(const struct ColorEntry &o1,const struct ColorEntry &o2) const
  {
    // The quick'n dirty method with less precision.
    return (((((packed & 0xfefefefe) + (o1.packed & 0xfefefefe)) >> 1) & 0xfefefefe) + 
	    (o2.packed & 0xfefefefe)) >> 1;
    
    /*
      return ((((LONG)(red   + o1.red   + o2.red   + o2.red)   & ~3UL)<<14) | 
      (((LONG)(green + o1.green + o2.green + o2.green) & ~3UL)<<6)  | 
      (((LONG)(blue  + o1.blue  + o2.blue  + o2.blue)        )>>2));
    */
  }
};
///

///
#endif
