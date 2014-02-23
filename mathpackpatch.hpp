/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mathpackpatch.hpp,v 1.5 2013-03-16 15:08:52 thor Exp $
 **
 ** In this module: Replacements for MathPack calls for speedup
 **********************************************************************************/

#ifndef MATHPACKPATCH_HPP
#define MATHPACKPATCH_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "patch.hpp"
#if HAVE_MATH_H && HAVE_POW && HAVE_EXP && HAVE_LOG && HAVE_LOG10 && HAVE_STRTOD
#define HAVE_MATHPACKPATCH 1
#endif
///

/// Forwards
class CPU;
class AdrSpace;
class PatchProvider;
///

/// Class MathPackPatch
#ifdef HAVE_MATHPACKPATCH
class MathPackPatch : public Patch {
  //
  // The following table contains positive powers
  // of ten for conversion between BCD and IEEE
  // floating point numbers. The powers are
  // 10^2,10^4,10^8,...,10^128
  static const double PosTenPowers[7];
  // The same negative powers
  static const double NegTenPowers[7];
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
  //
  // Convert ASCII to BCD in FR0. Set carry flag on error
  void AFP(class AdrSpace *adr,class CPU *cpu);
  // Convert BCD FR0 to ASCII in LBUFF.
  void FASC(class AdrSpace *adr,class CPU *cpu);
  // Convert a two-byte integer in d4,d5 to BCD in FR0
  void IFP(class AdrSpace *adr,class CPU *cpu);
  // Convert a BCD float in FR0 to integer in d4,d5.
  // Signal range error with C set
  void FPI(class AdrSpace *adr,class CPU *cpu);
  // Clear FR0
  void ZFR0(class AdrSpace *adr,class CPU *cpu);
  // Clear FR1
  void ZFR1(class AdrSpace *adr,class CPU *cpu);
  // Compute FR0-FR1->FR0. Set carry on out of range
  void FSUB(class AdrSpace *adr,class CPU *cpu);
  // Compute FR0+FR1->FR0. Set carry on out of range
  void FADD(class AdrSpace *adr,class CPU *cpu);
  // Compute FR0*FR1->FR0. Set carry on out of range
  void FMUL(class AdrSpace *adr,class CPU *cpu);
  // Compute FR0-FR1->FR0. Set carry on out of range
  void FDIV(class AdrSpace *adr,class CPU *cpu);
  // Compute the polynomial given by X,Y and A
  void PLYEVL(class AdrSpace *adr,class CPU *cpu);
  // Load FR0 by the value pointed to by X,Y
  void FLD0R(class AdrSpace *adr,class CPU *cpu);
  // Load FR0 by the value pointed to by FLPTR
  void FLD0P(class AdrSpace *adr,class CPU *cpu);
  // Load FR1 by the value pointed to by X,Y
  void FLD1R(class AdrSpace *adr,class CPU *cpu);
  // Load FR1 by the value pointed to by FLPTR
  void FLD1P(class AdrSpace *adr,class CPU *cpu);
  // Store FR0 at the address pointed to by X,Y
  void FST0R(class AdrSpace *adr,class CPU *cpu);
  // Store FR0 at the address pointed to by FLPTR
  void FST0P(class AdrSpace *adr,class CPU *cpu);
  // Copy FR0 to FR1
  void FMOVE(class AdrSpace *adr,class CPU *cpu);
  // Compute the exponential of FR0->FR0
  void FEXP(class AdrSpace *adr,class CPU *cpu);
  // Compute the exponential to base 10 of FR0->FR0
  void FEXP10(class AdrSpace *adr,class CPU *cpu);
  // Compute the logarithm of FR0->FR0
  void FLOG(class AdrSpace *adr,class CPU *cpu);
  // Compute the logarithm to base 10 of FR0->FR0
  void FLOG10(class AdrSpace *adr,class CPU *cpu);
  // Compute the fraction (FR0 - (X,Y))/(FR0 + (X,Y))->FR0
  void FFRACT(class AdrSpace *adr,class CPU *cpu);
  // Initialize INBUFF to 0x580.
  void INITINBUF(class AdrSpace *adr,class CPU *cpu);
  // Skip blanks in the buffer pointed to INBUF, CIX, return
  // new CIX.
  void SKIPBLANKS(class AdrSpace *adr,class CPU *cpu);
  // Multiply the temporary 0xf8,0xf7 by two.
  void TIMESTWO(class AdrSpace *adr,class CPU *cpu);
  // Zero the given register(s) from register X Y bytes
  void ZERORGS(class AdrSpace *adr,class CPU *cpu);
  // Normalize the number in FR0, return C=1 on overflow.
  void NORMALIZE(class AdrSpace *adr,class CPU *cpu);
  // Test whether the character at INBUF+CIX is a valid
  // digit, set carry if not.
  void TESTDIGIT(class AdrSpace *adr,class CPU *cpu);
protected:
  // Implemenation of the patch interface:
  // We can only provide the RunPatch interface. 
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
  //
public:  
  // Constructors and destructors
  MathPackPatch(class Machine *mach,class PatchProvider *p);
  virtual ~MathPackPatch(void)
  { 
  }
  //
};
#endif
///

///
#endif
