/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: basicmathpatch.cpp,v 1.2 2017/04/09 09:16:26 thor Exp $
 **
 ** In this module: Replacements for BASIC calls for speedup
 **********************************************************************************/

/// Includes
#include "types.h"
#include "basicmathpatch.hpp"
#include "adrspace.hpp"
#include "string.hpp"
#include "stdlib.hpp"
#include "stdio.hpp"
#include "cpu.hpp"
#if HAVE_MATH
#include <errno.h>
#include <math.h>
///

/// BasicMathPatch::BasicMathPatch
BasicMathPatch::BasicMathPatch(class Machine *mach,class PatchProvider *p,
			       const ADR *entrypts)
  : Patch(mach,p,6), EntryPoints(entrypts)
{
}
///

/// BasicMathPatch::BasicSQRT
// Square root, compute SQRT(fr0)
void BasicMathPatch::BasicSQRT(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  if (fr0 <= 0.0) {
    // An error
    cpu->P() |= CPU::C_Mask;
  } else {
    fr0 = sqrt(fr0);
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

/// BasicMathPatch::BasicPOW
// Power function, compute fr0^fr1
void BasicMathPatch::BasicPOW(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);
  double fr1 = ReadFR1(adr);

  fr0 = pow(fr0,fr1);
  if (fabs(fr0) > Huge || fr0 != fr0) { // No typo, this is a test for NAN
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// BasicMathPatch::BasicINT
// Floor function. Basic calls this INT. Computes Floor(fr0)
void BasicMathPatch::BasicINT(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  fr0 = floor(fr0);
  if (fabs(fr0) > Huge) {
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// BasicMathPatch::BasicCOS
// Cosine function. Computes COS(fr0)
void BasicMathPatch::BasicCOS(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  if (adr->ReadByte(0xfb)) {
    // If the argument is in radiants, convert
    fr0 *= M_PI / 180.0;
  }
  fr0 = cos(fr0);
  if (fr0 != fr0) { // Test for NAN
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// BasicMathPatch::BasicSIN
// Sine function. Computes SIN(fr0)
void BasicMathPatch::BasicSIN(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  if (adr->ReadByte(0xfb)) {
    // If the argument is in radiants, convert
    fr0 *= M_PI / 180.0;
  }
  fr0 = sin(fr0);
  if (fr0 != fr0) { // test for NAN
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// BasicMathPatch::BasicATAN
// Atan function. Computes ATAN(fr0)
void BasicMathPatch::BasicATAN(class AdrSpace *adr,class CPU *cpu)
{
  double fr0 = ReadFR0(adr);

  fr0 = atan(fr0);
  if (adr->ReadByte(0xfb)) {
    // If the result should be in degrees, convert
    fr0 *= 180.0 / M_PI;
  }
  if (fr0 != fr0) { // Test for NaN
    // signal an error
    cpu->P() |= CPU::C_Mask;
  } else {
    SetFR0(adr,fr0);
    cpu->P() &= ~CPU::C_Mask;
  }
}
///

/// BasicMathPatch::InstallPatch
// Install all the patches into the math pack
void BasicMathPatch::InstallPatch(class AdrSpace *adr,UBYTE code)
{
  int i;
  // These offsets come directly from the basic++ assembled version.
  // This requires updating if basic++ is updated.
  //
  for(i = 0;i < 6;i++) {
    InsertESC(adr,EntryPoints[i],code + i);
  }
}
///

/// BasicMathPatch::RunPatch
// Run one of the math pack patches
void BasicMathPatch::RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code)
{
  switch(code) {
  case 0:
    BasicSQRT(adr,cpu);
    break;
  case 1:
    BasicPOW(adr,cpu);
    break;
  case 2:
    BasicINT(adr,cpu);
    break;
  case 3:
    BasicCOS(adr,cpu);
    break;
  case 4:
    BasicSIN(adr,cpu);
    break;
  case 5:
    BasicATAN(adr,cpu);
    break;
  }
}
///

///
#endif
