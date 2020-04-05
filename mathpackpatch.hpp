/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: mathpackpatch.hpp,v 1.10 2020/03/28 14:05:58 thor Exp $
 **
 ** In this module: Replacements for MathPack calls for speedup
 **********************************************************************************/

#ifndef MATHPACKPATCH_HPP
#define MATHPACKPATCH_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "patch.hpp"
#include "mathsupport.hpp"
///

/// Forwards
class CPU;
class AdrSpace;
class PatchProvider;
///

/// Class MathPackPatch
#ifdef HAVE_MATH
class MathPackPatch : public Patch, private MathSupport {
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
  // Multiply fr0 by ten.
  void FR0TIMESTEN(class AdrSpace *adr,class CPU *cpu);
  //
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
