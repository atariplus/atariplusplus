/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mathsupport.hpp,v 1.1 2017/04/06 00:47:37 thor Exp $
 **
 ** In this module: Support package for MathPack support
 **********************************************************************************/

#ifndef MATHSUPPORT_HPP
#define MATHSUPPORT_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#if HAVE_MATH_H && HAVE_POW && HAVE_EXP && HAVE_LOG && HAVE_LOG10 && HAVE_STRTOD
#define HAVE_MATH 1
#endif
///

/// Forwards
class CPU;
class AdrSpace;
///

/// Class MathPackSupport
class MathSupport {
  //
#ifdef HAVE_MATH
  // The following table contains positive powers
  // of ten for conversion between BCD and IEEE
  // floating point numbers. The powers are
  // 10^2,10^4,10^8,...,10^128
  static const double PosTenPowers[7];
  // The same negative powers
  static const double NegTenPowers[7];
  //
protected:
  // Initinalizer for the maximal number
  static const double Huge;
  //
  // A BCD number as used by the MathPack
  struct BCD {
    UBYTE SignExponent;   // contains sign and exponent to the base of 100 with bias 64
    UBYTE Mantissa[5];    // five bytes BCD encoded mantissa
    // The implied decimal dot is between Mantissa[0] and Mantissa[1]
  };
  //
  // Helper functions:
  //
  // Convert a BCD number to an IEEE float
  static double BCDToIEEE(const struct BCD &in);
  //
  // Convert an IEEE float to a BCD number
  static void IEEEToBCD(double ieee,struct BCD &out);
  //
  // Extract FR0
  double ReadFR0(class AdrSpace *adr);
  // Extract FR1
  double ReadFR1(class AdrSpace *adr);
  // Set FR0
  void SetFR0(class AdrSpace *adr,double val);
#endif
};
///

///
#endif
