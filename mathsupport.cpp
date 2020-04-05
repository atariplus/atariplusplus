/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mathsupport.cpp,v 1.1 2017/04/06 00:47:37 thor Exp $
 **
 ** In this module: Support package for MathPack support
 **********************************************************************************/

/// Includes
#include "types.h"
#include "mathsupport.hpp"
#include "adrspace.hpp"
#include "string.hpp"
#include "stdlib.hpp"
#include "stdio.hpp"
#if HAVE_MATH
#include <errno.h>
#include <math.h>
///

/// Statics
const double MathSupport::Huge = 9.99999999e+99;
///

/// Power tables
// The following table contains positive powers
// of ten for conversion between BCD and IEEE
// floating point numbers. The powers are
// 10^2,10^4,10^8,...,10^128
const double MathSupport::PosTenPowers[7] = {
  1E+2,1E+4,1E+8,1E+16,1E+32,1E+64,1E+128
};
// The same negative powers, but including the one
const double MathSupport::NegTenPowers[7] = {
  1E-2,1E-4,1E-8,1E-16,1E-32,1E-64,1E-128
};
///

/// MathSupport::BCDToIEEE
// Convert a BCD number to an IEEE float
double MathSupport::BCDToIEEE(const struct BCD &in)
{
  bool negative;
  double num;
  int exponent,i;
  const double *powertable;
  //
  // First extract settings.
  negative = (in.SignExponent & 0x80)?(true):(false);
  // extract the exponent to the base of 100. Since the implied decimal dot
  // is between Mantissa[0] and Mantissa[1], we need to offset this a bit.
  exponent = ((in.SignExponent & 0x7f) - 64) - 4;
  //
  // Now extract the mantissa
  for(i=0,num=0.0;i<=4;i++) {
    num *= 100.0; // Scale for the next digit
    num += (in.Mantissa[i]   >> 4) * 10.0;
    num += (in.Mantissa[i] & 0x0f);
  }
  //
  // Get the powertable for upscaling/downscaling now
  if (exponent >= 0) {
    powertable = PosTenPowers;
  } else {
    powertable = NegTenPowers;
    exponent   = -exponent;
  }
  while(exponent) {
    if (exponent & 1) {
      num *= *powertable;
    }
    powertable++;
    exponent >>= 1;
  }
  //
  // Now add the sign.
  if (negative) {
    num = -num;
  }
  return num;
}
///

/// MathSupport::IEEEToBCD
// Convert an IEEE float to a BCD number
void MathSupport::IEEEToBCD(double num,struct BCD &out)
{
  bool negative;
  int exponent;
  const double *powertable;
  //
  // Check whether we are negative
  if (num < 0) {
    negative = true;
    num      = -num;
  } else {
    negative = false;
  }
  //
  // Check for the special case 0.0 and handle this part
  // immediately.
  if (num == 0.0) {
    exponent = -64;
    memset(out.Mantissa,0,sizeof(out.Mantissa));
  } else {
    // We must extract the exponent here. For that, check whether we need
    // to use the positive or negative powers.
    exponent   = 0;
    if (num >= 1.0) {
      powertable = PosTenPowers + 6;
      do {
	exponent <<= 1;
	if (num >= *powertable) {
	  exponent |= 1;
	  num      /= *powertable;
	}
	powertable--;
      } while(powertable >= PosTenPowers);
    } else {
      powertable = NegTenPowers + 6;
      do {
	exponent <<= 1;
	if (num < *powertable) {
	  exponent |= 1;
	  num      /= *powertable;
	}
	powertable--;
      } while(powertable >= NegTenPowers);
      // num is now between 1/100 and 1. Perform the last scale manually
      exponent++;
      num     *= 100.0;
      exponent = -exponent;
    }
    //
    // Now check whether we overrun or underrun the
    // range of the BCD floats. Either saturate or
    // denormalize.
    if (exponent >= 50) {
      // Saturate, use an exponent of 49 = ten-exponent of 98 
      // and a mantissa that is set to all nine
      exponent = 49;
      memset(out.Mantissa,0x99,sizeof(out.Mantissa));
    } else {
      int i;
      if (exponent < -64) {
	int delta = -64 - exponent; // should hence be positive.
	// Denormalize the number by making the mantissa smaller
	// at the price of making the exponent larger.
	powertable = NegTenPowers;
	while(delta) {
	  if (delta & 1) {
	    num *= *powertable;
	  }
	  delta >>= 1;
	  powertable++;
	}
      } else {
	// We are normalized and hence have a number between 1 and 10
	// We now add an epsilon value for the conversion to avoid
	// systematic errors by always rounding down.
	num += 5e-9 - 1e-14;
      }
      // Mantissa should now be a number between 1..99 except it is
      // denormalized. Extract this number and insert it into the
      // mantissa.
      for(i=0;i<5;i++) {
	int lo,hi,digit = int(num); // extract integer component. 
	// By clause A.6.3 in K&R, this must cut the decimal fraction.
	// Convert now the digit into BCD
	hi = digit / 10;
	lo = digit % 10;
	out.Mantissa[i] = (hi << 4) | lo;
	num -= double(digit);  // compute the remainder
	num *= 100;            // and scale up for the next digit
      }
    }
  }
  // At this point the mantissa has been computed and the exponent get inserted.
  out.SignExponent = (exponent + 64) | ((negative)?(0x80):(0x00));
}
///

/// MathSupport::ReadFR0
// Extract FR0
double MathSupport::ReadFR0(class AdrSpace *adr)
{
  struct BCD fr0;
  //
  // Read bytes one after another
  fr0.SignExponent = adr->ReadByte(0xd4);
  fr0.Mantissa[0]  = adr->ReadByte(0xd5);
  fr0.Mantissa[1]  = adr->ReadByte(0xd6);
  fr0.Mantissa[2]  = adr->ReadByte(0xd7);
  fr0.Mantissa[3]  = adr->ReadByte(0xd8);
  fr0.Mantissa[4]  = adr->ReadByte(0xd9);
  // And now convert it
  return BCDToIEEE(fr0);
}
///

/// MathSupport::ReadFR1
// Extract FR1
double MathSupport::ReadFR1(class AdrSpace *adr)
{
  struct BCD fr1;
  //
  // Read bytes one after another
  fr1.SignExponent = adr->ReadByte(0xe0);
  fr1.Mantissa[0]  = adr->ReadByte(0xe1);
  fr1.Mantissa[1]  = adr->ReadByte(0xe2);
  fr1.Mantissa[2]  = adr->ReadByte(0xe3);
  fr1.Mantissa[3]  = adr->ReadByte(0xe4);
  fr1.Mantissa[4]  = adr->ReadByte(0xe5);
  // And now convert it
  return BCDToIEEE(fr1);
}
///

/// MathSupport::SetFR0
// Deliver a result in FR0
void MathSupport::SetFR0(class AdrSpace *adr,double val)
{
  struct BCD fr0;
  //
  // Convert to BCD
  IEEEToBCD(val,fr0);
  //
  // And write out to memory
  adr->WriteByte(0xd4,fr0.SignExponent);
  adr->WriteByte(0xd5,fr0.Mantissa[0]);
  adr->WriteByte(0xd6,fr0.Mantissa[1]);
  adr->WriteByte(0xd7,fr0.Mantissa[2]);
  adr->WriteByte(0xd8,fr0.Mantissa[3]);
  adr->WriteByte(0xd9,fr0.Mantissa[4]);
}
///

///
#endif
