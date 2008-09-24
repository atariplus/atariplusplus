/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mathpackpatch.cpp,v 1.19 2008/09/24 15:05:23 thor Exp $
 **
 ** In this module: Replacements for MathPack calls for speedup
 **********************************************************************************/

/// Includes
#include "types.h"
#include "mathpackpatch.hpp"
#include "adrspace.hpp"
#include "rompage.hpp"
#include "string.hpp"
#include "stdlib.hpp"
#include "stdio.hpp"
#include "cpu.hpp"
#if HAVE_MATHPACKPATCH
#include <errno.h>
#include <math.h>
///

/// Statics
const double MathPackPatch::Huge = 9.99999999e+99;
///

/// Power tables
// The following table contains positive powers
// of ten for conversion between BCD and IEEE
// floating point numbers. The powers are
// 10^2,10^4,10^8,...,10^128
const double MathPackPatch::PosTenPowers[7] = {
  1E+2,1E+4,1E+8,1E+16,1E+32,1E+64,1E+128
};
// The same negative powers, but including the one
const double MathPackPatch::NegTenPowers[7] = {
  1E-2,1E-4,1E-8,1E-16,1E-32,1E-64,1E-128
};
///

/// MathPackPatch::MathPackPatch
MathPackPatch::MathPackPatch(class Machine *mach,class PatchProvider *p)
  : Patch(mach,p,29) // requires quite a lot of slots
{
}
///

/// MathPackPatch::BCDToIEEE
// Convert a BCD number to an IEEE float
double MathPackPatch::BCDToIEEE(const struct BCD &in)
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

/// MathPackPatch::IEEEToBCD
// Convert an IEEE float to a BCD number
void MathPackPatch::IEEEToBCD(double num,struct BCD &out)
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
	num += 0.4e-9;
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

/// MathPackPatch::ReadFR0
// Extract FR0
double MathPackPatch::ReadFR0(class AdrSpace *adr)
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

/// MathPackPatch::ReadFR1
// Extract FR1
double MathPackPatch::ReadFR1(class AdrSpace *adr)
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

/// MathPackPatch::SetFR0
// Deliver a result in FR0
void MathPackPatch::SetFR0(class AdrSpace *adr,double val)
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

/// MathPackPatch::AFP
// Convert ASCII to BCD in FR0. Set carry flag on error
void MathPackPatch::AFP(class AdrSpace *adr,class CPU *cpu)
{
  char buffer[257],*in;
  int cix = adr->ReadByte(0xf2); // Read the offset into the data
  ADR mem = adr->ReadWord(0xf3); // read the input offset
  ADR rd;
  int cnt = 0;
  double v;
  //
  // AFP converts at most a complete page. We also stop as soon as we
  // reach a character that cannot be part of a number.
  cnt = 0,in = buffer,rd = mem + cix;
  do {
    unsigned char chr = adr->ReadByte(rd);
    // Check whether the character could be possibly valid.
    if (chr == ' ' || chr == '.' || chr == 'E' || chr == 'e' || chr == '+' || chr == '-' || 
	(chr >= '0' && chr <= '9')) {
      *in++ = chr;
      cnt++;
      rd++;
    } else break;
  } while(cnt < 256);
  // Terminate the buffer now
  *in = 0;
  //
  // Now convert the string
  v = strtod(buffer,&in);
  // Check whether we could convert it into something useful.
  if (in > buffer && (fabs(v) < Huge)) {
    // Yup, worked. Now place the result in FR0, adjust CIX and clear the carry.
    SetFR0(adr,v);
    adr->WriteByte(0xf2,UBYTE(in - buffer + cix));
    cpu->P() &= ~CPU::C_Mask;
  } else {
    // Otherwise,set the carry
    cpu->P() |= CPU::C_Mask;
  }
}
///

/// MathPackPatch::FASC
// Convert BCD FR0 to ASCII in LBUFF.
void MathPackPatch::FASC(class AdrSpace *adr,class CPU *cpu)
{
  ADR  mem = 0x580; // the output buffer
  char *out,buffer[32];
  double v = ReadFR0(adr);
  //
  snprintf(buffer,31,"%.10G",v);
  buffer[31] = 0; // zero-terminate
  out = buffer;
  //
  // Now write the result into the output buffer
  do {
    if (out[1] == 0) {
      // If this is the last character, write it with its seventh bit set
      adr->WriteByte(mem,*out | 0x80);
      // Special hack: Write an additonal decimal separator.
      adr->WriteByte(mem+1,'.');
      break;
    }
    adr->WriteByte(mem,*out);
    mem++,out++;
  } while(true);
  // Install the target into 0xf3,0xf4: This
  // must point to the buffer.
  adr->WriteByte(0xf3,0x80);
  adr->WriteByte(0xf4,0x05);
  // Now clear the carry flag
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::IFP
// Convert a two-byte integer in d4,d5 to BCD in FR0
void MathPackPatch::IFP(class AdrSpace *adr,class CPU *cpu)
{
  UWORD data;
  double v;
  //
  // Rather straight.
  data = adr->ReadWord(0xd4);
  v    = double(data);
  SetFR0(adr,v);
  cpu->P() &= ~CPU::C_Mask; 
}
///

/// MathPackPatch::FPI
// Convert a BCD float in FR0 to integer in d4,d5.
// Signal range error with C set
void MathPackPatch::FPI(class AdrSpace *adr,class CPU *cpu)
{
  UWORD data;
  double v;
  //
  // Extract FR0
  v = ReadFR0(adr);
  if (v>=0 && v < 65536) {
    data = UWORD(v); // convert to integer
    adr->WriteByte(0xd4,UBYTE(data & 0xff));
    adr->WriteByte(0xd5,UBYTE(data >> 8));
    cpu->P() &= ~CPU::C_Mask;
  } else {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  }
}
///

/// MathPackPatch::ZFR0
// Clear FR0
void MathPackPatch::ZFR0(class AdrSpace *adr,class CPU *cpu)
{
  adr->WriteByte(0xd4,0);
  adr->WriteByte(0xd5,0);
  adr->WriteByte(0xd6,0);
  adr->WriteByte(0xd7,0);
  adr->WriteByte(0xd8,0);
  adr->WriteByte(0xd9,0);
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::ZFR1
// Clear FR1
void MathPackPatch::ZFR1(class AdrSpace *adr,class CPU *cpu)
{  
  ADR  reg = cpu->X();

  adr->WriteByte(reg + 0,0);
  adr->WriteByte(reg + 1,0);
  adr->WriteByte(reg + 2,0);
  adr->WriteByte(reg + 3,0);
  adr->WriteByte(reg + 4,0);
  adr->WriteByte(reg + 5,0);
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FSUB
// Compute FR0-FR1->FR0. Set carry on out of range
void MathPackPatch::FSUB(class AdrSpace *adr,class CPU *cpu)
{
  volatile double fr0,fr1; // required to ensure that numbers are rounded to double before we continue

  fr0  = ReadFR0(adr);
  fr1  = ReadFR1(adr);
  fr0 -= fr1;
  // Check for overflow
  if (fabs(fr0) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::FADD
// Compute FR0+FR1->FR0. Set carry on out of range
void MathPackPatch::FADD(class AdrSpace *adr,class CPU *cpu)
{
  volatile double fr0,fr1; // required to ensure that numbers are rounded to double before we continue

  fr0  = ReadFR0(adr);
  fr1  = ReadFR1(adr);
  fr0 += fr1;
  // Check for overflow
  if (fabs(fr0) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::FMUL
// Compute FR0*FR1->FR0. Set carry on out of range
void MathPackPatch::FMUL(class AdrSpace *adr,class CPU *cpu)
{
  volatile double fr0,fr1;

  fr0  = ReadFR0(adr);
  fr1  = ReadFR1(adr);
  fr0 *= fr1;
  // Check for overflow
  if (fabs(fr0) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::FDIV
// Compute FR0/FR1->FR0. Set carry on out of range
void MathPackPatch::FDIV(class AdrSpace *adr,class CPU *cpu)
{
  volatile double fr0,fr1;

  fr0  = ReadFR0(adr);
  fr1  = ReadFR1(adr);
  if (fr1 == 0.0) {
    // Error due to zero division
    cpu->P() |= CPU::C_Mask;
  } else {
    fr0 /= fr1;
    // Check for overflow
    if (fabs(fr0) > Huge) {
      // signal an error
      cpu->P() |= CPU::C_Mask;
    } else {
      SetFR0(adr,fr0);
      cpu->P() &= ~CPU::C_Mask;
    }
  }
}
///

/// MathPackPatch::PLYEVL
// Compute the polynomial given by X,Y and A
void MathPackPatch::PLYEVL(class AdrSpace *adr,class CPU *cpu)
{
  int coefficients = cpu->A();
  ADR mem          = (ADR(cpu->Y())<<8) | ADR(cpu->X());
  struct BCD factor;
  double y,x;
  //
  // Compute the polynomial by the Horner scheme.
  x = ReadFR0(adr);
  y = 0;
  do {    
    // Multiply by x and advance; does nothing on the first loop
    y *= x;
    // Extract the next coefficient
    factor.SignExponent = adr->ReadByte(mem++);
    factor.Mantissa[0]  = adr->ReadByte(mem++);
    factor.Mantissa[1]  = adr->ReadByte(mem++);
    factor.Mantissa[2]  = adr->ReadByte(mem++);
    factor.Mantissa[3]  = adr->ReadByte(mem++);
    factor.Mantissa[4]  = adr->ReadByte(mem++);
    // Add to the value
    y += BCDToIEEE(factor);
  } while(--coefficients);
  // Now install the result
  if (fabs(y) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,y);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::FLD0R
// Load FR0 by the value pointed to by X,Y
void MathPackPatch::FLD0R(class AdrSpace *adr,class CPU *cpu)
{
  ADR mem          = (ADR(cpu->Y())<<8) | ADR(cpu->X());
  //
  // Now copy this to FR0
  adr->WriteByte(0xd4,adr->ReadByte(mem++));
  adr->WriteByte(0xd5,adr->ReadByte(mem++));
  adr->WriteByte(0xd6,adr->ReadByte(mem++));
  adr->WriteByte(0xd7,adr->ReadByte(mem++));
  adr->WriteByte(0xd8,adr->ReadByte(mem++));
  adr->WriteByte(0xd9,adr->ReadByte(mem++));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FLD0P
// Load FR0 by the value pointed to by FLPTR
void MathPackPatch::FLD0P(class AdrSpace *adr,class CPU *cpu)
{
  ADR mem          = adr->ReadWord(0xfc);
  //
  // Now copy this to FR0
  adr->WriteByte(0xd4,adr->ReadByte(mem++));
  adr->WriteByte(0xd5,adr->ReadByte(mem++));
  adr->WriteByte(0xd6,adr->ReadByte(mem++));
  adr->WriteByte(0xd7,adr->ReadByte(mem++));
  adr->WriteByte(0xd8,adr->ReadByte(mem++));
  adr->WriteByte(0xd9,adr->ReadByte(mem++));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FLD1R
// Load FR1 by the value pointed to by X,Y
void MathPackPatch::FLD1R(class AdrSpace *adr,class CPU *cpu)
{
  ADR mem          = (ADR(cpu->Y())<<8) | ADR(cpu->X());
  //
  // Now copy this to FR0
  adr->WriteByte(0xe0,adr->ReadByte(mem++));
  adr->WriteByte(0xe1,adr->ReadByte(mem++));
  adr->WriteByte(0xe2,adr->ReadByte(mem++));
  adr->WriteByte(0xe3,adr->ReadByte(mem++));
  adr->WriteByte(0xe4,adr->ReadByte(mem++));
  adr->WriteByte(0xe5,adr->ReadByte(mem++));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FLD1P
// Load FR0 by the value pointed to by FLPTR
void MathPackPatch::FLD1P(class AdrSpace *adr,class CPU *cpu)
{
  ADR mem          = adr->ReadWord(0xfc);
  //
  // Now copy this to FR0
  adr->WriteByte(0xe0,adr->ReadByte(mem++));
  adr->WriteByte(0xe1,adr->ReadByte(mem++));
  adr->WriteByte(0xe2,adr->ReadByte(mem++));
  adr->WriteByte(0xe3,adr->ReadByte(mem++));
  adr->WriteByte(0xe4,adr->ReadByte(mem++));
  adr->WriteByte(0xe5,adr->ReadByte(mem++));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FST0R
// Store FR0 at the address indicated by X,Y
void MathPackPatch::FST0R(class AdrSpace *adr,class CPU *cpu)
{
  ADR mem          = (ADR(cpu->Y())<<8) | ADR(cpu->X());
  
  adr->WriteByte(mem++,adr->ReadByte(0xd4));
  adr->WriteByte(mem++,adr->ReadByte(0xd5));
  adr->WriteByte(mem++,adr->ReadByte(0xd6));
  adr->WriteByte(mem++,adr->ReadByte(0xd7));
  adr->WriteByte(mem++,adr->ReadByte(0xd8));
  adr->WriteByte(mem++,adr->ReadByte(0xd9));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FST0P
// Store FR0 at the address indicated by X,Y
void MathPackPatch::FST0P(class AdrSpace *adr,class CPU *cpu)
{
  ADR mem          = adr->ReadWord(0xfc);
  
  adr->WriteByte(mem++,adr->ReadByte(0xd4));
  adr->WriteByte(mem++,adr->ReadByte(0xd5));
  adr->WriteByte(mem++,adr->ReadByte(0xd6));
  adr->WriteByte(mem++,adr->ReadByte(0xd7));
  adr->WriteByte(mem++,adr->ReadByte(0xd8));
  adr->WriteByte(mem++,adr->ReadByte(0xd9));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FMOVE
// Copy FR0 to FR1
void MathPackPatch::FMOVE(class AdrSpace *adr,class CPU *cpu)
{
  adr->WriteByte(0xe0,adr->ReadByte(0xd4));
  adr->WriteByte(0xe1,adr->ReadByte(0xd5));
  adr->WriteByte(0xe2,adr->ReadByte(0xd6));
  adr->WriteByte(0xe3,adr->ReadByte(0xd7));
  adr->WriteByte(0xe4,adr->ReadByte(0xd8));
  adr->WriteByte(0xe5,adr->ReadByte(0xd9));
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::FEXP
// Compute the exponential of FR0->FR0
void MathPackPatch::FEXP(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  fr0 = exp(fr0); 
  if (fabs(fr0) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::FEXP10
// Compute the exponential to the base ten of FR0->FR0
void MathPackPatch::FEXP10(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  fr0 = pow(10.0,fr0); 
  if (fabs(fr0) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::FLOG
// Compute the logarithm of FR0->FR0
void MathPackPatch::FLOG(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  if (fr0 <= 0.0) {
    // An error
    cpu->P() |= CPU::C_Mask;
  } else {
    fr0 = log(fr0);
    if (fabs(fr0) > Huge) {
      // signal an error
      cpu->P() |= CPU::C_Mask;
    } else {
      SetFR0(adr,fr0);
      cpu->P() &= ~CPU::C_Mask;
    }
  }
}
///

/// MathPackPatch::FLOG10
// Compute the logarithm to base 10 of FR0->FR0
void MathPackPatch::FLOG10(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  if (fr0 <= 0.0) {
    // An error
    cpu->P() |= CPU::C_Mask;
  } else {
    fr0 = log10(fr0);
    if (fabs(fr0) > Huge) {
      // signal an error
      cpu->P() |= CPU::C_Mask;
    } else {
      SetFR0(adr,fr0);
      cpu->P() &= ~CPU::C_Mask;
    }
  }
}
///

/// MathPackPatch::FFRACT
// Compute the fraction (FR0 - (X,Y))/(FR0 + (X,Y))->FR0
void MathPackPatch::FFRACT(class AdrSpace *adr,class CPU *cpu)
{
  UWORD a    = (UWORD(cpu->Y())<<8) | UWORD(cpu->X());
  double fr0 = ReadFR0(adr);
  double b;
  struct BCD bcd;
  //
  // Read bytes one after another
  bcd.SignExponent = adr->ReadByte(a+0);
  bcd.Mantissa[0]  = adr->ReadByte(a+1);
  bcd.Mantissa[1]  = adr->ReadByte(a+2);
  bcd.Mantissa[2]  = adr->ReadByte(a+3);
  bcd.Mantissa[3]  = adr->ReadByte(a+4);
  bcd.Mantissa[4]  = adr->ReadByte(a+5);
  b                = BCDToIEEE(bcd);

  if (fr0 == -b) {
    cpu->P() |= CPU::C_Mask;
  } else {
    fr0 = (fr0 - b)/(fr0 + b);
    if (fabs(fr0) > Huge) {
      // signal an error
      cpu->P() |= CPU::C_Mask;
    } else {
      SetFR0(adr,fr0);
      cpu->P() &= ~CPU::C_Mask;
    }
  }
}
///

/// MathPackPatch::INITINBUF
// Initialize INBUFF to 0x580.
void MathPackPatch::INITINBUF(class AdrSpace *adr,class CPU *)
{
  // Initialize InBuf to 0x580, the math pack output buffer.
  // This is an undocumented call the Mac/65 requires. Urgl.
  adr->WriteByte(0xf3,0x80);
  adr->WriteByte(0xf4,0x05);
}
///

/// MathPackPatch::SkipBlanks
// Skip blanks in the buffer pointed to INBUF, CIX, return
// new CIX.
void MathPackPatch::SKIPBLANKS(class AdrSpace *adr,class CPU *cpu)
{
  int cix = adr->ReadByte(0xf2); // Read the offset into the data
  ADR mem = adr->ReadWord(0xf3); // read the input offset
  ADR rd  = mem + cix;
  // Another undocumented call required by the Mac/65 package.
  // This call skips blanks in the INBUF buffer.
  do {
    unsigned char chr = adr->ReadByte(rd);
    // Check whether this is a blank space. If not, abort and re-set CIX.
    // otherwise, continue reading.
    if (chr != ' ')
      break;
    cix++;
    rd++;
  } while(cix < 256);
  //
  // Write new CIX back.
  adr->WriteByte(0xf2,cix);
  // Forward this to the CPU. Mac/65 requires this.
  cpu->Y() = cix;
}
///

/// MathPackPatch::TIMESTWO
// Another undocumented entry requires by Mac/65:
// multiply the temporary 0xf8,0xf7 by two.
void MathPackPatch::TIMESTWO(class AdrSpace *adr,class CPU *cpu)
{
  UWORD tmp;
  // This is interestingly a big-endian word!
  tmp =  adr->ReadByte(0xf8) << 1;
  adr->WriteByte(0xf8,UBYTE(tmp));
  tmp = (adr->ReadByte(0xf7) << 1) | (tmp >> 8);
  adr->WriteByte(0xf7,UBYTE(tmp));
  if (tmp & 0x100) {
    cpu->P() |= CPU::C_Mask;
  } else {
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// MathPackPatch::ZERORGS
// Zero the given register(s) from register X Y bytes
// This is an undocumented call-in used by the BASIC ROM
void MathPackPatch::ZERORGS(class AdrSpace *adr,class CPU *cpu)
{
  ADR  reg = cpu->X();
  UBYTE ln = cpu->Y();
  //
  do {
    adr->WriteByte(reg,0);
    reg++;
  } while(--ln);
  // Clear the Accu as well.
  cpu->A()  = 0;
  cpu->P() |= CPU::Z_Mask;
}
///

/// MathPackPatch::NORMALIZE
// Normalize the number in FR0, return C=1 on overflow.
void MathPackPatch::NORMALIZE(class AdrSpace *adr,class CPU *cpu)
{
  UBYTE exponent;
  UBYTE mantissa[5];
  int i;
  // First extract the floating point number from the FR0 register
  exponent = adr->ReadByte(0xd4);
  // If the exponent is zero, then nothing to do.
  if ((exponent & 0x7f) == 0) {
    // All fine, just return.
    cpu->P() &= ~CPU::C_Mask;
    return;
  }
  //
  // Otherwise, check closer and get the mantissa.
  for(i=0;i<5;i++) {
    mantissa[i] = adr->ReadByte(0xd5+i);
  }
  // Multiply by 100 as LONG as we have leading zeros and
  // can shrink the exponent. We leave denormalized numbers
  // in the register, unlike the original math pack.
  while(mantissa[0] == 0 && (exponent & 0x7f)) {
    // Move to front, multiply by 100
    memmove(mantissa+0,mantissa+1,4);
    // and adapt the exponent.
    exponent--;
  }
  // If we are here, then either:
  // a) we have still leading zeros and the exponent is
  // zero. If so, this is a denormalized number and we exit.
  // Otherwise, check whether the exponent runs over. If
  // so, deliver an error.
  if ((exponent & 0x7f) >= 0x71) {
    cpu->P() |= CPU::C_Mask;
    return;
  }
  // Write the mantissa and the exponent back.
  adr->WriteByte(0xd4,exponent);
  for(i=0;i<5;i++) {
    adr->WriteByte(0xd5+i,mantissa[i]);
  }  
  // All fine, return.
  cpu->P() &= ~CPU::C_Mask;
}
///

/// MathPackPatch::TESTDIGIT
// Test whether the character at INBUF+CIX is a valid
// digit, set carry if not.
void MathPackPatch::TESTDIGIT(class AdrSpace *adr,class CPU *cpu)
{
  int cix = adr->ReadByte(0xf2); // Read the offset into the data
  ADR mem = adr->ReadWord(0xf3); // read the input offset
  ADR rd  = mem + cix;
  char c;
  //
  c = adr->ReadByte(rd);
  if (c >= '0' && c <= '9') {
    cpu->A()  = c - '0';
    cpu->P() &= ~CPU::C_Mask;
  } else {
    cpu->P() |= CPU::C_Mask;
  }
}
///

/// MathPackPatch::InstallPatch
// Install all the patches into the math pack
void MathPackPatch::InstallPatch(class AdrSpace *adr,UBYTE code)
{
  size_t i;
  const UBYTE basconst[] = {	// constants for basic Atan
     0x3e,0x10,0x82,0x07,0x69,0x40,
     0xbe,0x71,0x67,0x58,0x38,0x21,
     0x3f,0x02,0x22,0x40,0x71,0x99,
     0xbf,0x04,0x43,0x66,0x78,0x16,
     0x3f,0x06,0x72,0x11,0x48,0x46,
     0xbf,0x08,0x80,0x35,0x18,0x38,
     0x3f,0x11,0x05,0x67,0x08,0x42,
     0xbf,0x14,0x27,0x97,0x12,0x93,
     0x3f,0x19,0x99,0x96,0x75,0x33,
     0xbf,0x33,0x33,0x33,0x27,0x67,
     0x3f,0x99,0x99,0x99,0x99,0x99,
     0x3f,0x78,0x53,0x98,0x16,0x34
  };
  //
  // Insert all the patches now
  InsertESC(adr,0xd800,code +  0); // AFP
  InsertESC(adr,0xd8e6,code +  1); // FASC
  InsertESC(adr,0xd9aa,code +  2); // IFP
  InsertESC(adr,0xd9d2,code +  3); // FPI  
  InsertESC(adr,0xda44,code +  4); // ZFR0
  InsertESC(adr,0xda46,code +  5); // ZFR1
  InsertESC(adr,0xda60,code +  6); // FSUB
  InsertESC(adr,0xda66,code +  7); // FADD
  InsertESC(adr,0xdadb,code +  8); // FMUL
  InsertESC(adr,0xdb28,code +  9); // FDIV
  InsertESC(adr,0xdd89,code + 10); // FLD0R
  InsertESC(adr,0xdd8d,code + 11); // FLD0P
  InsertESC(adr,0xdd98,code + 12); // FLD1R
  InsertESC(adr,0xdd9c,code + 13); // FLD1P
  InsertESC(adr,0xdda7,code + 14); // FSTOR
  InsertESC(adr,0xddab,code + 15); // FSTOP
  InsertESC(adr,0xddb6,code + 16); // FMOVE
  InsertESC(adr,0xdd40,code + 17); // PLYEVL
  InsertESC(adr,0xddc0,code + 18); // EXP
  InsertESC(adr,0xddcc,code + 19); // EXP10
  InsertESC(adr,0xdecd,code + 20); // LOG
  InsertESC(adr,0xded1,code + 21); // LOG10
  // Undocumented call-ins follow. These are used
  // by BASIC and Mac/65
  InsertESC(adr,0xde95,code + 22); // FFRACT
  InsertESC(adr,0xda51,code + 23); // INITINBUF
  InsertESC(adr,0xdba1,code + 24); // SKIPBLANKS
  InsertESC(adr,0xda5a,code + 25); // TIMESTWO  
  InsertESC(adr,0xda48,code + 26); // ZERORGS
  InsertESC(adr,0xdc00,code + 27); // NORMALIZE
  InsertESC(adr,0xdbaf,code + 28); // TESTDIGIT
  //
  // Undocumented constants, BASIC requires them
  adr->PatchByte(0xdf6c  ,0x3f);
  adr->PatchByte(0xdf6c+1,0x50);
  adr->PatchByte(0xdf6c+2,0x00);
  adr->PatchByte(0xdf6c+3,0x00);
  adr->PatchByte(0xdf6c+4,0x00);
  adr->PatchByte(0xdf6c+5,0x00);  // 0.5 as BCD constant
  //
  for(i = 0;i < sizeof(basconst);i++) {
    adr->PatchByte(ADR(0xdfae + i),basconst[i]);
  }
}
///

/// MathPackPatch::RunPatch
// Run one of the math pack patches
void MathPackPatch::RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code)
{
  switch(code) {
  case 0:
    AFP(adr,cpu);
    break;
  case 1:
    FASC(adr,cpu);
    break;
  case 2:
    IFP(adr,cpu);
    break;
  case 3:
    FPI(adr,cpu);
    break;
  case 4:
    ZFR0(adr,cpu);
    break;
  case 5:
    ZFR1(adr,cpu);
    break;
  case 6:
    FSUB(adr,cpu);
    break;
  case 7:
    FADD(adr,cpu);
    break;
  case 8:
    FMUL(adr,cpu);
    break;
  case 9:
    FDIV(adr,cpu);
    break;
  case 10:
    FLD0R(adr,cpu);
    break;
  case 11:
    FLD0P(adr,cpu);
    break;
  case 12:
    FLD1R(adr,cpu);
    break;
  case 13:
    FLD1P(adr,cpu);
    break;
  case 14:
    FST0R(adr,cpu);
    break;
  case 15:
    FST0P(adr,cpu);
    break;
  case 16:
    FMOVE(adr,cpu);
    break;
  case 17:
    PLYEVL(adr,cpu);
    break;
  case 18:
    FEXP(adr,cpu);
    break;
  case 19:
    FEXP10(adr,cpu);
    break;
  case 20:
    FLOG(adr,cpu);
    break;
  case 21:
    FLOG10(adr,cpu);
    break;
  case 22:
    FFRACT(adr,cpu);
    break;
  case 23:
    INITINBUF(adr,cpu);
    break;
  case 24:
    SKIPBLANKS(adr,cpu);
    break;
  case 25:
    TIMESTWO(adr,cpu);
    break;
  case 26:
    ZERORGS(adr,cpu);
    break;
  case 27:
    NORMALIZE(adr,cpu);
    break;
  case 28:
    TESTDIGIT(adr,cpu);
    break;
  }
}
///

///
#endif
