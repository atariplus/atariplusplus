/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: basicmathpatch.hpp,v 1.1 2017/04/06 00:47:37 thor Exp $
 **
 ** In this module: Replacements for BASIC calls for speedup
 **********************************************************************************/

#ifndef BASICMATHPATCH_HPP
#define BASICMATHPATCH_HPP

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

/// Class BasicMathPatch
#ifdef HAVE_MATH
class BasicMathPatch : public Patch, private MathSupport {
  //
  // Addresses we need to patch for Basic, in the order
  // of the functions below. They come from the BasicROM
  // class.
  const ADR *EntryPoints;
  //
  // Entry points for Basic++:
  //
  // Square root, compute SQRT(fr0)
  void BasicSQRT(class AdrSpace *adr,class CPU *cpu);
  // Power function, compute fr0^fr1
  void BasicPOW(class AdrSpace *adr,class CPU *cpu);
  // Floor function. Basic calls this INT. Computes Floor(fr0)
  void BasicINT(class AdrSpace *adr,class CPU *cpu);
  // Cosine function. Computes COS(fr0)
  void BasicCOS(class AdrSpace *adr,class CPU *cpu);
  // Sine function. Computes SIN(fr0)
  void BasicSIN(class AdrSpace *adr,class CPU *cpu);
  // Atan function. Computes ATAN(fr0)
  void BasicATAN(class AdrSpace *adr,class CPU *cpu);
protected:
  // Implemenation of the patch interface:
  // We can only provide the RunPatch interface. 
  virtual void RunPatch(class AdrSpace *adr,class CPU *cpu,UBYTE code);
  virtual void InstallPatch(class AdrSpace *adr,UBYTE code);
  //
public:  
  // Constructors and destructors
  // The constructor requires an array of entry points where to patch.
  BasicMathPatch(class Machine *mach,class PatchProvider *p,const ADR *entrypts);
  virtual ~BasicMathPatch(void)
  { 
  }
  //
};
#endif
///

///
#endif
