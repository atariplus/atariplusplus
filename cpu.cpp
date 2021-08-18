/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cpu.cpp,v 1.134 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: CPU 6502 emulator
 **********************************************************************************/

/// Includes
#include "adrspace.hpp"
#include "debugadrspace.hpp"
#include "mmu.hpp"
#include "argparser.hpp"
#include "cpu.hpp"
#include "antic.hpp"
#include "monitor.hpp"
#include "snapshot.hpp"
#include "string.hpp"
#include "new.hpp"
#if DEBUG_LEVEL > 0
#include "main.hpp"
#endif
///

/// Some defines
#if HAS_TEMPLATE_CASTS
#define NOT(a) UBYTE(~a)
#else
#define NOT(a) ~a
#endif
// Update the Z and N flags quickly.
#define UpdateNZ(operand) (p) = ((p) & (~(Z_Mask | N_Mask))) | (CPU::FlagUpdate[operand & 0xff])
///

/// 65C02 instructions
/*
  0x04:  tsb zpage
  0x0c:  tsb absolute
  0x12:  ORA (zpage)
  0x14:  trb zpage
  0x1a:  ina
  0x1c:  trb absolute
  0x32:  AND (zpage)
  0x34:  bit zpage,x
  0x3a:  dea
  0x3c:  bit absolute,x
  0x52:  EOR (zpage)
  0x5a:  phy
  0x64:  stz zpage
  0x72:  ADC (zpage)  
  0x74:  stz zpage,x 
  0x7a:  ply
  0x7c:  jmp (absolute,x) 
  0x80:  bra displacement
  0x92:  STA (zpage) 
  0x9c:  stz absolute 
  0x9e:  stz absolute,x  
  0xb2:  LDA (zpage) 
  0xd2:  CMP (zpage) 
  0xda:  phx 
  0xf2:  SBC (zpage)  
  0xfa:  plx
*/
///

/// CPU::CPU
CPU::CPU(class Machine *mach)
  : Chip(mach,"CPU"), Saveable(mach,"CPU"), HBIAction(mach),
    ProfilingCounters(NULL), CumulativeCounters(NULL), Instructions(NULL)
{
  int i;
  //
  EnableBreak            = false;
  EnableTracing          = false;
  EnableStacking         = false;
  EnableUntil            = false;
  EnableWatch            = false;
  TraceOnReset           = false;
  Emulate65C02           = false;
  TraceInterrupts        = true;
  GlobalPC               = 0x0000;
  GlobalA                = 0;
  GlobalX                = 0;
  GlobalY                = 0;
  GlobalP                = 0;
  GlobalS                = 0xff;
  PreviousPC             = 0x0000;
  TracePC                = 0x0000;
  TraceS                 = 0xff;
  InterruptS             = 0x00;
  IRQMask                = 0;
  CycleCounter           = 0;
  ProfileCounter         = 0;
  NMI                    = false;
  HaltStart              = ClocksPerLine;
  IRQPending             = false;
  WSyncPosition          = 105; // must be at least 103 for encounter
  AtomicExecutionOperand = 0;
  EffectiveAddress       = 0;
  ISync                  = false;
  HitWatchPoint          = -1;
  //
  // Clear all break points
  for(i=0;i<NumBreakPoints;i++) {
    BreakPoints[i].Enabled = false;
    BreakPoints[i].Free    = true;
    BreakPoints[i].BreakPC = 0x000;
  }
  //
  // Allocate all cycles. This simplifies overflow handling
  // in the GTIA scanline creator; namely, we won't need any.
  memset(StolenCycles,1,sizeof(StolenCycles));
  LastCycle = StolenCycles + ClocksPerLine; // size of a scanline in cycles.
  HBI();
}
///

/// CPU::FlagUpdate
// The following table contains (for quick-access) pre-computed
// flag-updates depending on the operand. It provides Z and N
// flags quickly without a branch.
const UBYTE CPU::FlagUpdate[256] = { Z_Mask,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     0     ,0     ,0     ,0     ,0     ,0     ,0     ,0     ,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,
				     N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask,N_Mask};
///

/// CPU::~CPU destructor
CPU::~CPU(void)
{
  delete[] ProfilingCounters;
  delete[] CumulativeCounters;

  ClearInstructions();
}
///

/// CPU::AtomicExecutionUnit::AtomicExecutionUnit
// A single cycle as taken by the CPU. This is a base class several other
// execution units derive from. This is the constructor CPU wise.
template<class Adr>
CPU::AtomicExecutionUnit<Adr>::AtomicExecutionUnit(class CPU *cpu)
  : Ram(((sizeof(Adr) == sizeof(AdrSpace))?
	 ((Adr *)(cpu->Ram)):
	 ((Adr *)(cpu->DebugRam)))),
	ZPage(cpu->ZPage), Stack(cpu->Stack), Cpu(cpu)
{
}
///

/// CPU::MicroCode::Insert
// Insert another slot intermediately into the execution order.
void CPU::MicroCode::Insert(class CPU *cpu)
{
  cpu->NextStep = this;
  cpu->ExecutionSteps--;
}
///

/// CPU::ExecutionSequence::ExecutionSequence
// An execution sequence: A sequence = an array of steps. As total, an
// instruction with its instruction slots.
CPU::ExecutionSequence::ExecutionSequence(void)
{
  // Clear all pointers in here to ensure that we do not
  // dispose anything we don't have.
  memset(Sequence,0,sizeof(Sequence));
}
///

/// CPU::ExecutionSequence::~ExecutionSequence
// Dispose the execution sequence and all steps that are part of it.
CPU::ExecutionSequence::~ExecutionSequence(void)
{
  int i;

  for(i=0;i<8;i++) {
    delete Sequence[i];
  }
}
///

/// CPU::ExecutionSequence::AddStep
// For construction of an execution sequence: Add an atomic
// execution sequence here.
void CPU::ExecutionSequence::AddStep(class CPU::MicroCode *step)
{
  class MicroCode **aeu;
  // Find the last free slot and insert the step there.
  aeu = Sequence;
  while(*aeu)
    aeu++;
#if CHECK_LEVEL > 0
  if (aeu - Sequence >= 8) {
    Throw(OutOfRange,"CPU::ExecutionSequence::AddStep","execution sequence overfull");
  }
#endif
  *aeu = step;
}
///

/// CPU::LDAUnit::Execute
// The load accumulator unit. This takes the operand and stores it in the
// accumulator. This is the main operation sequence for an LDA instruction.
// It also sets the condition codes.
UWORD CPU::LDAUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  Cpu->GlobalA = UBYTE(operand);
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::LDXUnit::Execute
// The load X register unit. This takes the operand and stores it in the
// X register. This is the main operation sequence for an LDX instruction
// It also sets the condition codes.
UWORD CPU::LDXUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  Cpu->GlobalX = UBYTE(operand);

  UpdateNZ(operand);

  Cpu->GlobalP = p;

  return operand;
}
///

/// CPU::LDYUnit::Execute
// The load Y register unit. This takes the operand and stores it in the
// Y register. This is the main operation sequence for an LDY instruction
// It also sets the condition codes.
UWORD CPU::LDYUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  Cpu->GlobalY = UBYTE(operand);

  UpdateNZ(operand);

  Cpu->GlobalP = p;

  return operand;
}
///

/// CPU::LoadVectorUnit::Execute
// The LoadVector unit: This loads the contents of a fixed address (lo-byte) into the
// operand. This is one of the processing steps of a BRK intruction or of an interrupt
// processing step. It also or's the P register with a fixed value.
template<UWORD vector,UBYTE mask>
UWORD CPU::LoadVectorUnit<vector,mask>::Execute(UWORD operand)
{
  Cpu->GlobalP |= mask; // Set I or B bit, dependending on what is required.
  return UWORD((operand & 0xff00) | Ram->ReadByte(vector));
}
///
/// CPU::LoadVectorUnitQuirk::Execute
// The LoadVector unit: This loads the contents of a fixed address (lo-byte) into the
// operand. This is one of the processing steps of a BRK intruction or of an interrupt
// processing step. It also or's the P register with a fixed value.
// This is the quirky version which loads an alternative vector if the NMI is active.
// Bummer!
template<UWORD vector,UWORD alternative,UBYTE mask>
UWORD CPU::LoadVectorUnitQuirk<vector,alternative,mask>::Execute(UWORD operand)
{
  Cpu->GlobalP |= mask; // Set I or B bit, dependending on what is required.
  if (Cpu->NMI) {
    return UWORD((Ram->ReadByte(alternative+1) << 8) | Ram->ReadByte(alternative));
  } else {
    return UWORD((operand & 0xff00) | Ram->ReadByte(vector));
  }
}
///
/// CPU::LoadVectorUnitExtend::Execute
// This is the high-byte extender of the LoadVector unit loading the high-byte of
// the vector into the operand and then into the PC. This is the last step for interrupt
// processing
template<UWORD vector>
UWORD CPU::LoadVectorUnitExtend<vector>::Execute(UWORD operand)
{
  operand       = UWORD((operand & 0x00ff) | (UWORD(Ram->ReadByte(vector + 1)) << 8));
  return operand;
}
///

/// CPU::BITUnit::Execute
// Implements the BIT instruction test: And the accumulator with
// the operand, set Z bit. Set N,V from the bits 7,6 of the tested
// operand.
UWORD CPU::BITUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  
  if ((operand & Cpu->GlobalA & 0xff) == 0) {
    p |= Z_Mask;
  } else {
    p &= ~Z_Mask;
  }
  
  // Luckely, the V_Mask is bit 6 and the N_Mask is bit 7,
  // making all of the following just masking.
  //
  p = (p & (~(V_Mask | N_Mask))) | (operand & 0xc0);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::BITWierdUnit::Execute
// Implements the a weird extra-instruction BIT unit and
// implements bit-fiddling of V,C depending on the operand.
UWORD CPU::BITWierdUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  
  //
  // C is a copy from bit 6.
  if (operand & 0x40) {
    p |= C_Mask;
  } else {
    p &= ~C_Mask;
  }
  //
  // V is the exor of bit 5 and 6
  if ((operand ^ (operand << 1)) & 0x40) {
    p |= V_Mask;
  } else {
    p &= ~V_Mask;
  }

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::TRBUnit::Execute
// Implements the TRB instruction: operand = operand & (NOT A)
// Set bits N,V from bits 7,6 from the operand before operating
// on it, Z bit on the result.
UWORD CPU::TRBUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  // Luckely, the V_Mask is bit 6 and the N_Mask is bit 7,
  // making all of the following just masking.
  p = (p & (~(V_Mask | N_Mask))) | (operand & 0xc0);
  //
  // Masking with the Accumulator.
  if ((operand &= ~Cpu->GlobalA) == 0) {
    p |= Z_Mask;
  } else {
    p &= ~Z_Mask;
  }
  
  Cpu->GlobalP = p;

  return operand;
}
///

/// CPU::TSBUnit::Execute
// Implements the TSB instruction: operand = operand | A
// Set bits N,V from bits 7,6 from the operand before operating
// on it, Z bit on the result.
UWORD CPU::TSBUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  // Luckely, the V_Mask is bit 6 and the N_Mask is bit 7,
  // making all of the following just masking.
  p = (p & (~(V_Mask | N_Mask))) | (operand & 0xc0);
  //
  // Masking with the Accumulator.
  if ((operand |= Cpu->GlobalA) == 0) {
    p |= Z_Mask;
  } else {
    p &= ~Z_Mask;
  }
  
  Cpu->GlobalP = p;

  return operand;
}
///

/// CPU::ORAUnit::Execute
// The ORA unit. This takes the operand, or's it into the accumulator
// and stores the result in the accumulator. It also sets the condition
// codes.
UWORD CPU::ORAUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  operand = Cpu->GlobalA |= UBYTE(operand);
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::ANDUnit::Execute
// The AND unit. This takes the operand, or's it into the accumulator
// and stores the result in the accumulator. It also sets the condition
// codes.
UWORD CPU::ANDUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  operand = Cpu->GlobalA &= UBYTE(operand);
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::EORUnit::Execute
// The EOR unit. This takes the operand, or's it into the accumulator
// and stores the result in the accumulator. It also sets the condition
// codes.
UWORD CPU::EORUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  operand = Cpu->GlobalA ^= UBYTE(operand);
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::LSRUnit::Execute
// The LSR unit. This takes the operand, shifts it right and stores
// the result back into the accumulator. It also sets the condition
// codes.
UWORD CPU::LSRUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  //
  // The C_Mask is just bit 0, making this simple.
  p = (p & 0xfe) | (operand & 0x01);

  operand = UWORD((operand & 0xff) >> 1);
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::ASLUnit::Execute
// The ASL unit. This takes the operand, shifts it left
// and stores the result back into the operand. It also sets the condition
// codes.
UWORD CPU::ASLUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  //
  // Moves bit 7 into bit 0 and thus into the carry.
  p = (p & 0xfe) | ((operand >> 7) & 0x01);

  operand = UWORD((operand & 0xff) << 1);
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::RORUnit::Execute
// The ROR unit. It takes the operand, rotates it right and sets the condition
// codes. It does not touch the accumulator.
UWORD CPU::RORUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  operand &= 0xff;
  operand |= (p & C_Mask) << 8; // Move the C bit into bit 8
  //
  // The C_Mask is just bit 0, making this simple.
  p = (p & 0xfe) | (operand & 0x01);
  
  operand >>= 1;
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::ROLUnit::Execute
// The ROL unit. It takes the operand, rotates it left and sets the condition
// codes. It does not touch the accumulator.
UWORD CPU::ROLUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;

  operand <<= 1;

  operand  |= (p & C_Mask); // Is in bit #0
  //
  // Move bit #8 into the carry.
  p         = (p & ~C_Mask) | ((operand & 0x100) >> 8);
  
  operand  &= 0xff;
  
  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::ADCUnitFixed::Execute
// The ADC unit. It takes the operand, and adds it with carry to the
// accumulator. It also sets the condition codes, and keeps care of the
// decimal flag in the P register. This is the version that is used
// for the western design center 65c02 which fixes some bugs of the
// 6502 version.
UWORD CPU::ADCUnitFixed::Execute(UWORD operand)
{
  UBYTE p     = Cpu->GlobalP;
  UWORD adata = UWORD(operand & 0xff);
  UWORD tmp   = UWORD(Cpu->GlobalA + adata + (p & C_Mask)); // CMask is 1!
  //
  p &= ~(N_Mask | V_Mask | Z_Mask | C_Mask);
  if (((~(Cpu->GlobalA ^ adata)) & (Cpu->GlobalA ^ tmp)) & 0x80)
    p |= V_Mask;
  //
  // Check for decimal mode: If decimal mode is set, run the special
  // decimal adjustion
  if (p & D_Mask) {
    UWORD A = Cpu->GlobalA;
    UWORD al,ah;

    p   &= ~V_Mask;
    al   = UWORD((A & 0x0f) + (adata & 0x0f) + (Cpu->GlobalP & C_Mask));
    ah   = UWORD((A & 0xf0) + (adata & 0xf0));
    if (al > 9) {
      al += 6;
      ah += 0x10;
    }
    if (ah > 0x90) {
      ah += 0x60;
      if (ah >= 0x100)
	p |= V_Mask;
    }
    
    tmp  = UWORD(ah | (al & 0x0f));

    Wait.Insert(Cpu);
  }
  // 
  if ((tmp & 0xff) == 0)
    p |= Z_Mask;
  if (tmp & 0x80)
    p |= N_Mask;  
  if (tmp >= 0x100)
    p |= C_Mask;
  //
  // Install the results.
  Cpu->GlobalA = UBYTE(tmp);
  Cpu->GlobalP = p;
  
  return tmp;
}
///

/// CPU::ADCUnit::Execute
// The ADC unit. It takes the operand, and adds it with carry to the
// accumulator. It also sets the condition codes, and keeps care of the
// decimal flag in the P register.
UWORD CPU::ADCUnit::Execute(UWORD operand)
{
  UBYTE p     = Cpu->GlobalP;
  UWORD adata = UWORD(operand & 0xff);
  UWORD tmp   = UWORD(Cpu->GlobalA + adata + (p & C_Mask)); // CMask is 1!
  UWORD ah    = tmp;
  //
  p &= ~(N_Mask | V_Mask | Z_Mask | C_Mask);
  //
  // The Z flag is computed on the binary result (yuck!)
  if ((tmp & 0xff) == 0)
    p |= Z_Mask;
  //
  // Check for decimal mode: If decimal mode is set, run the special
  // decimal adjustion
  if (p & D_Mask) { 
    UWORD A = Cpu->GlobalA;
    UWORD al;

    al   = UWORD((A & 0x0f) + (adata & 0x0f) + (Cpu->GlobalP & C_Mask));
    ah   = UWORD((A & 0xf0) + (adata & 0xf0));
    if (al > 9) {
      al += 6;
      ah += 0x10;
    } 
    if (ah > 0x90) {
      ah += 0x60;
    }
    
    tmp  = UWORD(ah | (al & 0x0f));
  } 
  // overflow is set if result & A signs differ and
  // the signs of the arguments agree.
  if (((~(Cpu->GlobalA ^ adata)) & (Cpu->GlobalA ^ ah)) & 0x80)
    p |= V_Mask;
  if (ah & 0x80)
    p |= N_Mask;
  if (ah >= 0x100)
    p |= C_Mask;
  //
  // Install the results.
  Cpu->GlobalA = UBYTE(tmp);
  Cpu->GlobalP = p;
  
  return tmp;
}
///

/// CPU::SBCUnitFixed::Execute
// The ADC unit. It takes the operand, and subtracts it with carry from the
// accumulator. It also sets the condition codes, and keeps care of the
// decimal flag in the P register. This is the version that is used
// for the western design center 65c02 which fixes some bugs of the
// 6502 version.
UWORD CPU::SBCUnitFixed::Execute(UWORD operand)
{
  UBYTE p     = Cpu->GlobalP;
  UWORD adata = UWORD(operand & 0xff);
  UWORD tmp   = UWORD(Cpu->GlobalA - adata - 1 + (p & C_Mask)); // C_Mask is 1!
  //
  p &= ~(N_Mask | V_Mask | Z_Mask | C_Mask);

  if (((~(Cpu->GlobalA ^ adata)) & (Cpu->GlobalA ^ tmp)) & 0x80)
    p |= V_Mask;
  
  if (p & D_Mask) {
    UWORD A = Cpu->GlobalA;
    UWORD al,ah;
    
    p &= ~V_Mask;
  
    al = UWORD((A & 0x0f) - (adata & 0x0f) - 1 + (Cpu->GlobalP & C_Mask));
    ah = UWORD((A & 0xf0) - (adata & 0xf0));
    if (al & 0x10) {
      al -= 6;
      ah -= 0x10;
    }
    if (ah & 0x100) {
      ah -= 0x60;
      p  |= V_Mask;
    }

    tmp = UWORD(ah | (al & 0x0f)); 

    Wait.Insert(Cpu);
  }  
  //
  if ((tmp & 0xff) == 0)
    p |= Z_Mask;
  if (tmp & 0x80)
    p |= N_Mask;  
  if (tmp < 0x100)
    p |= C_Mask;
  //
  // Install the results.
  Cpu->GlobalA = UBYTE(tmp);
  Cpu->GlobalP = p;

  return tmp;
}
///

/// CPU::SBCUnit::Execute
// The ADC unit. It takes the operand, and subtracts it with carry from the
// accumulator. It also sets the condition codes, and keeps care of the
// decimal flag in the P register.
UWORD CPU::SBCUnit::Execute(UWORD operand)
{
  UBYTE p     = Cpu->GlobalP;
  UWORD adata = UWORD(operand & 0xff);
  UWORD tmp   = UWORD(Cpu->GlobalA - adata - 1 + (p & C_Mask)); // C_Mask is 1!
  //
  // Overflow is set if result & A signs differ and
  // (even though) the signs of the arguments differ
  p &= ~(N_Mask | V_Mask | Z_Mask | C_Mask);
  //
  // Here all flags are set according to the binary result.
  if ((tmp & 0xff) == 0)
    p |= Z_Mask;
  if (((Cpu->GlobalA ^ adata) & (Cpu->GlobalA ^ tmp)) & 0x80)
    p |= V_Mask;
  if (tmp & 0x80)
    p |= N_Mask;
  if (tmp < 0x100)
    p |= C_Mask;
  
  if (p & D_Mask) {
    UWORD A = Cpu->GlobalA;
    UWORD al,ah;
    
    al = UWORD((A & 0x0f) - (adata & 0x0f) - 1 + (Cpu->GlobalP & C_Mask));
    ah = UWORD((A & 0xf0) - (adata & 0xf0));
    if (al & 0x10) {
      al -= 6;
      ah -= 0x10;
    }
    if (ah & 0x100) {
      ah -= 0x60;
    }

    tmp = UWORD(ah | (al & 0x0f)); 
  }  
  //
  // Install the results.
  Cpu->GlobalA = UBYTE(tmp);
  Cpu->GlobalP = p;

  return tmp;
}
///

/// CPU::INCUnit::Execute
// The INC unit. It takes the argument and adds one to it. It does not
// set the accumulator. It sets the Z and N condition codes, but does
// not touch the C condition code.
UWORD CPU::INCUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  
  operand = UWORD((operand + 1) & 0xff);  

  UpdateNZ(operand);

  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::DECUnit::Execute
// The DEC unit. It takes the argument and subtracts one from it. It does not
// set the accumulator. It sets the Z and N condition codes, but does
// not touch the C condition code.
UWORD CPU::DECUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  
  operand = UWORD((operand - 1) & 0xff);

  UpdateNZ(operand);
  
  Cpu->GlobalP = p;
  
  return operand;
}
///

/// CPU::CMPUnit::Execute
// The CMP unit. It compares the A register with the operand and sets the
// condition codes
UWORD CPU::CMPUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  UBYTE t = UBYTE(Cpu->GlobalA - UBYTE(operand));

  UpdateNZ(t);
  
  if (Cpu->GlobalA >= UBYTE(operand)) {
    p |= C_Mask;
  } else {
    p &= ~C_Mask;
  }
  
  Cpu->GlobalP = p;
  
  //
  // Returns the difference for some extra instructions.
  return t;
}
///

/// CPU::CPXUnit::Execute
// The CPX unit. It compares the A register with the operand and sets the
// condition codes
UWORD CPU::CPXUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  UBYTE t = UBYTE(Cpu->GlobalX - UBYTE(operand));

  UpdateNZ(t);

  if (Cpu->GlobalX >= UBYTE(operand)) {
    p |= C_Mask;
  } else {
    p &= ~C_Mask;
  }
  
  Cpu->GlobalP = p;

  return t;
}
///

/// CPU::CPYUnit::Execute
// The CPY unit. It compares the A register with the operand and sets the
// condition codes
UWORD CPU::CPYUnit::Execute(UWORD operand)
{
  UBYTE p = Cpu->GlobalP;
  UBYTE t = UBYTE(Cpu->GlobalY - UBYTE(operand));

  UpdateNZ(t);

  if (Cpu->GlobalY >= UBYTE(operand)) {
    p |= C_Mask;
  } else {
    p &= ~C_Mask;
  }
  
  Cpu->GlobalP = p;
  
  return t;
}
///

/// CPU::HaltUnit::Execute
// The Halt unit: This cycles the CPU until an interrupt (NMI or IRQ) is
// detected.
UWORD CPU::HaltUnit::Execute(UWORD operand)
{ 
  // do not loop forever if the emulator is waiting for an instruction boundary sync.
  // Note that the I-flag is ignored here!
  if (!Cpu->NMI && !Cpu->IRQMask && !Cpu->ISync) {
    Cpu->ExecutionSteps--; // repeat this step forever.
    Cpu->NextStep = Cpu->ExecutionSteps[-1];
  }

  return operand;
}
///

/// CPU::BranchUnit::Execute
// The branch unit. This unit reads an displacement off the PC and jumps to a relative
// location. It may add a wait state if a page boundary is crossed. This step should never
// be inserted directly, but it should be part of the BranchDetectUnit step.
UWORD CPU::BranchUnit::Execute(UWORD operand)
{
  UWORD newpc = UWORD(Cpu->GlobalPC + BYTE(Ram->ReadByte(Cpu->GlobalPC)) + 1);
  // Check whether we cross a page boundary. If so, insert a wait state.
  if ((newpc ^ Cpu->GlobalPC) & 0xff00) {
    Wait.Insert(Cpu);
  }
  // Install the new PC now.
  Cpu->GlobalPC = newpc;
  //
  return operand;
}
///

/// CPU::BranchDetectUnit::Execute
// The branch detect unit. It reads a displacement from the current PC and may or may
// not branch. This is the first step of a two-step branch execution unit which
// may or may not insert additional steps.
template<UBYTE mask,UBYTE value>
UWORD CPU::BranchDetectUnit<mask,value>::Execute(UWORD)
{
  // Check whether the condition encoded in this unit is true or not.
  if ((Cpu->GlobalP & mask) == value) {
    // It is met. Install the branch unit as next instruction step.
    Branch.Insert(Cpu);
    return 1;
  } else {
    // Skip the additional displacement value.
    Cpu->GlobalPC++;
    return 0;
  }
}
///

/// CPU::BranchBitTestUnit::Execute
// The BranchBitTestUnit, used for the Rockwell BBR/BBS instructions.
template<UBYTE bitmask,UBYTE bitvalue>
UWORD CPU::BranchBitTestUnit<bitmask,bitvalue>::Execute(UWORD operand)
{
  // This has to test against the operand.
  if ((operand & bitmask) == bitvalue) {
    // Is met. Install the branch unit as next instruction.
    Branch.Insert(Cpu);
    return 1;
  } else {
    // Skip the displacement value.
    Wait.Insert(Cpu); // always takes the same number of cycles.
    Cpu->GlobalPC++;
    return 0;
  }
}
///

/// CPU::RMBUnit::Execute
// The Rockwell Reset Memory Bit instruction
template<UBYTE bitmask>
UWORD CPU::RMBUnit<bitmask>::Execute(UWORD operand)
{
  operand &= ~bitmask;
  
  return operand;
}
///

/// CPU::SMBUnit::Execute
// The Rockwell Reset Memory Bit instruction
template<UBYTE bitmask>
UWORD CPU::SMBUnit<bitmask>::Execute(UWORD operand)
{
  operand |= bitmask;
  
  return operand;
}
///

/// CPU::AndHiPlusOneYAddXUnit::Execute
// Another wierd unit for extra instructions:
// AND the Y unit with the high-byte of the operand+1,
// add the X register to the operand, store as effective address,
// then return the result of the AND.
UWORD CPU::AndHiPlusOneYAddXUnit::Execute(UWORD operand)
{

  // Carry over triggers Z-page writing.
  if ((((operand + Cpu->GlobalX) ^ operand) & 0xff00) == 0) {
    Cpu->EffectiveAddress = operand + Cpu->GlobalX;
  } else {
    Cpu->EffectiveAddress = (operand + Cpu->GlobalX) & 0xff;
  }
  
  return ((operand >> 8) + 1) & Cpu->GlobalY;
}
///

/// CPU::AndHiPlusOneXAddYUnit::Execute
// Another wierd unit for extra instructions:
// AND the X unit with the high-byte of the operand+1,
// add the Y register to the operand, store as effective address,
// then return the result of the AND.
UWORD CPU::AndHiPlusOneXAddYUnit::Execute(UWORD operand)
{
  
  // Carry over triggers Z-page writing.
  if ((((operand + Cpu->GlobalY) ^ operand) & 0xff00) == 0) {
    Cpu->EffectiveAddress = operand + Cpu->GlobalY;
  } else {
    Cpu->EffectiveAddress = (operand + Cpu->GlobalY) & 0xff;
  }

  return ((operand >> 8) + 1) & Cpu->GlobalX;
}
///

/// CPU::JAMUnit::Execute
// Halt the CPU, enter the JAM state and therefore the monitor
template<UBYTE instruction>
UWORD CPU::JAMUnit<instruction>::Execute(UWORD)
{
  Cpu->GlobalPC--;
  Cpu->Machine()->Jam(instruction);

  return 0;
}
///

/// CPU::UnstableUnit::Execute
// Halt the CPU, enter the unstable state and therefore the monitor
template<UBYTE instruction>
UWORD CPU::UnstableUnit<instruction>::Execute(UWORD)
{  
  Cpu->GlobalPC--;
  Cpu->Machine()->Crash(instruction);

  return 0;
}
///

/// CPU::ESCUnit::Execute
// The ESCUnit: This implements an ESC code mechanism that will run emulator
// specific actions.
UWORD CPU::ESCUnit::Execute(UWORD operand)
{
  // The operand is the escape code.
  Cpu->Machine()->Escape(UBYTE(operand));
  // pass it back
  return operand;
}
///

/// CPU::DecodeUnit::Execute
// The decode unit decodes a single instruction, loads the
// instruction into the step pipeline and tests for NMIs and
// IRQs.
UWORD CPU::DecodeUnit::Execute(UWORD)
{
  //
  // Forward this to the CPU directly. The reason is
  // that the monitor might replace the atomic instruction
  // unit "under our feet" and hence crash us.
  return Cpu->DecodeInstruction();
}
///

/// CPU::DecodeInstruction
// Run the microcode decoder, fetch the next instruction.
// This is here and not in an atomic step because the monitor
// could replace the latter on the fly when inserting
// watch-points.
UWORD CPU::DecodeInstruction(void)
{
  class MicroCode **aeu;
  int opcode;
  //
  // If we require synchronization, then bail out here.
  if (ISync) {
    NextStep       = ExecutionSteps[-2];
    ExecutionSteps--;
    ISync          = false;
    return 0;
  }
  //
  // Check for pending breaks.
  if (EnableBreak) {
    int i;	
    // Re-insert the instruction decoder into the pipeline here unless something
    // else happens. This ensures that the monitor can at least
    // launch off the CPU again once we left here due to a breakpoint
    NextStep       = ExecutionSteps[-2];
    ExecutionSteps--;
    //
#ifdef HAS_MEMBER_INIT
    for(i=0;i<NumBreakPoints;i++) {
      if (BreakPoints[i].Enabled && BreakPoints[i].BreakPC == GlobalPC) {  
	InterruptS = 0x00; // Keep tracing
	monitor->CapturedBreakPoint(i,GlobalPC);
      }
    }
#else
    for(i=0;i<NumBreakPoints;i++) {
      if (BreakPoints[i].Enabled && BreakPoints[i].BreakPC == GlobalPC) {
	InterruptS = 0x00; // Keep tracing
	monitor->CapturedBreakPoint(i,GlobalPC);
      }
    }
#endif
    // Check for watchpoints. The instruction is completed before the watchpoint is
    // entered.
    if (EnableWatch) {
      int watch = HitWatchPoint;
      if (watch >= 0) {
	HitWatchPoint = -1;
	monitor->CapturedWatchPoint(watch,PreviousPC);
      }
      PreviousPC = GlobalPC;
    }
    //
    // Check for Tracing and stacking.
    if (EnableStacking) {
      if (GlobalS >= TraceS) {
	InterruptS = 0x00; // Keep tracing
	monitor->CapturedTrace(GlobalPC);
      }
    } else if (EnableUntil) {
      if (GlobalS > TraceS || (GlobalPC > TracePC && GlobalS == TraceS)) {
	TracePC = 0x0000;
	monitor->CapturedTrace(GlobalPC);
      }
    } else if (EnableTracing) {
      if (TraceInterrupts || GlobalS >= InterruptS) {
	InterruptS = 0x00; // Keep tracing
	monitor->CapturedTrace(GlobalPC);
      }
    }  
  }
  // Interrupt processing follows.
  // Check for pending NMIs. If so, service them by providing the pipeline of the NMI
  // processor. 
  if (NMI) {
    aeu            = Instructions[0x101]->Sequence;
    NextStep       = aeu[1];
    ExecutionSteps = aeu + 2;
    InterruptS     = GlobalS;
    return (*aeu)->Execute(0);          // start the NMI processing here.
  }
  // Check for pending interrupts. If so, fetch it now.
 
  // Check whether we need to serve an IRQ next - only checked on instruction boundaries.
  if (IRQPending) {
    IRQPending     = false;
    aeu            = Instructions[0x102]->Sequence;
    NextStep       = aeu[1];
    ExecutionSteps = aeu + 2;
    InterruptS     = GlobalS;
    return (*aeu)->Execute(0);          // start the IRQ processing here.
  }
  if (IRQMask && ((GlobalP & I_Mask) == 0)) {
    // Delay for the next instruction - wierdo.
    IRQPending     = true;
  }
  //
  // Update the profiling counters, i.e. run the profiler.
  if (ProfilingCounters) {
    if (ProfilingCounters[GlobalPC] < ULONG(~0UL))
      ProfilingCounters[GlobalPC]++;
    if (CumulativeCounters) {
      ADR addr = GlobalS + 0x101;
      if (CumulativeCounters[0xffff] < ULONG(~0UL) - ProfileCounter)
	CumulativeCounters[0xffff] += ProfileCounter;
      while(addr <= 0x1fe) {
	// Check whether the address is a RAM or ROM address.
	ADR back = Ram->ReadWord(addr) - 2;
	if (back >= 0x0000 && back < 0xfffa &&
	    Ram->isIOSpace(back) == false && Ram->isIOSpace(back + 1) == false) {
	  if (Ram->ReadByte(back) == 0x20) { // Is a JSR
	    ADR target = Ram->ReadWord(back + 1);
	    if (CumulativeCounters[target] < ULONG(~0UL) - ProfileCounter)
	      CumulativeCounters[target] += ProfileCounter;
	    addr += 2;
	  } else {
	    addr++;
	  }
	} else {
	  addr++;
	}
      }
    }
  }
  ProfileCounter = 0;
  //
  // Fetch the next Opcode: This counts as one execution step.
  opcode     = Ram->ReadByte(GlobalPC);
#if CHECK_LEVEL > 0
  // Keep the last opcode for debugging purposes
  LastIR     = opcode;
#endif
  GlobalPC++;
  //
  // Fetch the execution sequence for this opcode
  // and install it.
  aeu            = Instructions[opcode]->Sequence;
  NextStep       = aeu[0];
  ExecutionSteps = aeu + 1;
  return 0;
}
///

/// CPU::ClearInstructions
// Remove all instructions for the CPU, for reset and
// coldstart.
void CPU::ClearInstructions(void)
{ 
  int i;
  struct ExecutionSequence **es;
  // dispose all instruction sequences, and its sub-steps.
  // This cleans up all of the state machine.
  es = Instructions;
  if (es) {
    for(i=0;i<256+3;i++) {
      if (*es) {
	delete *es;
	*es = NULL;
      }
      es++;
    }
    delete[] Instructions;Instructions = NULL;
  }
}
///

/// CPU::BuildInstructions
// Build up all the instruction sequences for the CPU, namely the full
// state machine of all instructions we have.
void CPU::BuildInstructions(bool addwatches)
{
  int i;
  //
  ClearInstructions();
  Instructions = new struct ExecutionSequence *[256+3];
  // Clear all instruction pointers now.
  memset(Instructions,0,(3+256) * sizeof(struct ExecutionSequence *));
  for(i=0;i<256+3;i++) {
    Instructions[i] = new struct ExecutionSequence;
  }
  //
  // After that preparation, build up the instructions one by one.
  // We build it in several groups to avoid LONG branches that limit
  // portability to systems with successfully broken compilers...
  //
  if (addwatches) {    
    BuildInstructions00<class DebugAdrSpace>();
    BuildInstructions10<class DebugAdrSpace>();
    BuildInstructions20<class DebugAdrSpace>();
    BuildInstructions30<class DebugAdrSpace>();
    BuildInstructions40<class DebugAdrSpace>();
    BuildInstructions50<class DebugAdrSpace>();
    BuildInstructions60<class DebugAdrSpace>();
    BuildInstructions70<class DebugAdrSpace>();
    BuildInstructions80<class DebugAdrSpace>();
    BuildInstructions90<class DebugAdrSpace>();
    BuildInstructionsA0<class DebugAdrSpace>();
    BuildInstructionsB0<class DebugAdrSpace>();
    BuildInstructionsC0<class DebugAdrSpace>();
    BuildInstructionsD0<class DebugAdrSpace>();
    BuildInstructionsE0<class DebugAdrSpace>();
    BuildInstructionsF0<class DebugAdrSpace>();
    BuildInstructionsExtra<class DebugAdrSpace>();
  } else {
    BuildInstructions00<class AdrSpace>();
    BuildInstructions10<class AdrSpace>();
    BuildInstructions20<class AdrSpace>();
    BuildInstructions30<class AdrSpace>();
    BuildInstructions40<class AdrSpace>();
    BuildInstructions50<class AdrSpace>();
    BuildInstructions60<class AdrSpace>();
    BuildInstructions70<class AdrSpace>();
    BuildInstructions80<class AdrSpace>();
    BuildInstructions90<class AdrSpace>();
    BuildInstructionsA0<class AdrSpace>();
    BuildInstructionsB0<class AdrSpace>();
    BuildInstructionsC0<class AdrSpace>();
    BuildInstructionsD0<class AdrSpace>();
    BuildInstructionsE0<class AdrSpace>();
    BuildInstructionsF0<class AdrSpace>();
    BuildInstructionsExtra<class AdrSpace>();
  }
}
///

/// CPU::BuildInstructions00
// Build instruction opcodes 0x00 to 0x0f
template<class Adr>
void CPU::BuildInstructions00(void)
{
  //
  // 0x00: BRK: 7 cycles
  Disassembled[0x00] =Instruction("BRK",Instruction::NoArgs,7);
  Instructions[0x00]->AddStep(new Cat2<LoadPCUnit<1>,PushUnitExtend<Adr> >(this,Emulate65C02));    // PC+2->op,op.hi->Stack
  Instructions[0x00]->AddStep(new Cat1<PushUnit<Adr> >(this,Emulate65C02));                        // op.lo->Stack
  Instructions[0x00]->AddStep(new Cat2<OrToStatusUnit<B_Mask>,PushUnit<Adr> >(this,Emulate65C02)); // P|Break->op, op->Stack
  if (Emulate65C02) {
    Instructions[0x00]->AddStep(new Cat2<AndToStatusUnit<NOT(D_Mask)>,LoadVectorUnitExtend<0xfffe> >(this));         // IRQ_Vector.hi->op
    // The 65c02 clears the D bit
    Instructions[0x00]->AddStep(new Cat1<LoadVectorUnit<0xfffe,I_Mask> >(this));
    Instructions[0x00]->AddStep(new Cat1<JMPUnit<0> >(this));                                 // op->PC,P|=I_Mask;
  } else {
    Instructions[0x00]->AddStep(new Cat1<LoadVectorUnitExtend<0xfffe> >(this));         // IRQ_Vector.hi->op
    Instructions[0x00]->AddStep(new Cat1<LoadVectorUnitQuirk<0xfffe,0xfffa,I_Mask> >(this));  // IRQ_Vector.lo->op
    Instructions[0x00]->AddStep(new Cat2<NMIResetUnit, JMPUnit<0> >(this));                   // op->PC,P|=I_Mask;
  }
  Instructions[0x00]->AddStep(new Cat1<DecodeUnit>(this));                    // decode next instruction
  //
  // 0x01: ORA (addr,X): 6 cycles
  Disassembled[0x01] =Instruction("ORA",Instruction::Indirect_X,6);
  Instructions[0x01]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x01]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x01]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
  Instructions[0x01]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
  Instructions[0x01]->AddStep(new Cat1<ORAUnit>(this));                         // op|A->A
  Instructions[0x01]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) { 
    // 0x02: NOP2
    Disassembled[0x02] =Instruction("NOPE",Instruction::Immediate,2);
    Instructions[0x02]->AddStep(new Cat1<ImmediateUnit>(this));                                      
    Instructions[0x02]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x02: JAM
    Disassembled[0x02] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x02]->AddStep(new Cat1<JAMUnit<0x02> >(this));                                      // just crash
    Instructions[0x02]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x03] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x03]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x03: SLO (addr,X): 8 cycles
    Disassembled[0x03] =Instruction("SLOR",Instruction::Indirect_X,8);
    Instructions[0x03]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x03]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0x03]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0x03]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0x03]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));// op<<1->op
    Instructions[0x03]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));        // op->ea
    Instructions[0x03]->AddStep(new Cat1<ORAUnit>(this));                         // op|A->A
    Instructions[0x03]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0x04: NOP zpage, 3 cycles
  if (Emulate65C02) {
    // TSB zpage, 5 cycles
    Disassembled[0x04] =Instruction("TSB",Instruction::ZPage,5);
    Instructions[0x04]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x04]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // (op)->op
    Instructions[0x04]->AddStep(new Cat1<TSBUnit>(this));                         // A|op->flags,op
    Instructions[0x04]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // write result back out
    Instructions[0x04]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  } else {
    Disassembled[0x04] =Instruction("NOPE",Instruction::Immediate,3);
    Instructions[0x04]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x04]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // (op)->op
    Instructions[0x04]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0x05: ORA zpage, 3 cycles
  Disassembled[0x05] =Instruction("ORA",Instruction::ZPage,3);
  Instructions[0x05]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x05]->AddStep(new Cat2<ZPageIndirectionUnit<Adr> ,ORAUnit>(this)); // (op)->op,op|A->A
  Instructions[0x05]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x06: ASL zpage, 5 cycles
  Disassembled[0x06] =Instruction("ASL",Instruction::ZPage,5);
  Instructions[0x06]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x06]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0x06]->AddStep(new Cat1<ASLUnit>(this));                           // op<<1->op
  Instructions[0x06]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->ea
  Instructions[0x06]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  if (Emulate65C02) {
    // 0x07: RMB0 zpage, 5 cycles
    Disassembled[0x07] =Instruction("RMB0",Instruction::ZPage,5);
    Instructions[0x07]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x07]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x07]->AddStep(new Cat1<RMBUnit<0x01> >(this));                  // reset bit #0
    Instructions[0x07]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0x07]->AddStep(new Cat1<DecodeUnit>(this)); 
  } else {
    // 0x07: SLO zpage, 5 cycles
    Disassembled[0x07] =Instruction("SLOR",Instruction::ZPage,5);
    Instructions[0x07]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x07]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x07]->AddStep(new Cat1<ASLUnit>(this));                         // op<<1->op
    Instructions[0x07]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr> ,ORAUnit>(this,Emulate65C02)); // op->ea,op|A->A
    Instructions[0x07]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0x08: PHP, 3 cycles
  Disassembled[0x08] =Instruction("PHP",Instruction::NoArgs,3);
  Instructions[0x08]->AddStep(new Cat1<OrToStatusUnit<B_Mask> >(this));            // P->op (with B set)
  Instructions[0x08]->AddStep(new Cat1<PushUnit<Adr> >(this,Emulate65C02));                     // op->stack
  Instructions[0x08]->AddStep(new Cat1<DecodeUnit>(this));                         // decode next instruction
  //
  // 0x09: ORA immediate, 2 cycles
  Disassembled[0x09] =Instruction("ORA",Instruction::Immediate,2);
  Instructions[0x09]->AddStep(new Cat2<ImmediateUnit,ORAUnit>(this));              // #->op,op|A->A
  Instructions[0x09]->AddStep(new Cat1<DecodeUnit>(this));                         // decode next instruction
  //
  // 0x0a: ASL A, 2 cycles
  Disassembled[0x0a] =Instruction("ASL",Instruction::Accu,2);
  Instructions[0x0a]->AddStep(new Cat3<AccuUnit,ASLUnit,LDAUnit>(this));
  Instructions[0x0a]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x0b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x0b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x0b: ANC, 3 cycles FIXME: Should this be two? I won't believe it.
    // This is instable on my machine.
    Disassembled[0x0b] =Instruction("ANDC",Instruction::Immediate,3);
    Instructions[0x0b]->AddStep(new Cat2<ImmediateUnit,ANDUnit>(this));           // #->op,op&A->A
    // Actually, part of ROL but the remaining result is thrown away.
    Instructions[0x0b]->AddStep(new Cat1<CopyNToCUnit>(this));                    
    Instructions[0x0b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // TSB absolute, 6 cycles
    Disassembled[0x0c] =Instruction("TSB",Instruction::Absolute,6);
    Instructions[0x0c]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0x0c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
    Instructions[0x0c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
    Instructions[0x0c]->AddStep(new Cat1<TSBUnit>(this));                         // op|A->A
    Instructions[0x0c]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this));
    Instructions[0x0c]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x0c: NOP abs, 4 cycles
    Disassembled[0x0c] =Instruction("NOPE",Instruction::Absolute,4);
    Instructions[0x0c]->AddStep(new Cat1<ImmediateUnit>(this));                                      // lo->op
    Instructions[0x0c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));                         // hi->op
    Instructions[0x0c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                                    // (op)->op
    Instructions[0x0c]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x0d: ORA abs, 4 cycles
  Disassembled[0x0d] =Instruction("ORA",Instruction::Absolute,4);
  Instructions[0x0d]->AddStep(new Cat1<ImmediateUnit>(this));                                      // lo->op
  Instructions[0x0d]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));                         // hi->op
  Instructions[0x0d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ORAUnit>(this));         // (op)->op,op|A->A
  Instructions[0x0d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x0e: ASL abs, 6 cycles
  Disassembled[0x0e] =Instruction("ASL",Instruction::Absolute,6);
  Instructions[0x0e]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0x0e]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0x0e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // op->ea,(op)->op
  if (Emulate65C02) {
    Instructions[0x0e]->AddStep(new Cat1<ASLUnit>(this));                         // op<<1->op
  } else {
    Instructions[0x0e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));// op<<1->op
  }
  Instructions[0x0e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));          // op->(ea)
  Instructions[0x0e]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
   // 0x0f: BBR0 zpage,disp, 5 cycles
    Disassembled[0x0f] =Instruction("BBR0",Instruction::ZPage_Disp,5);
    Instructions[0x0f]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x0f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x0f]->AddStep(new Cat1<BranchBitTestUnit<0x01,0x00> >(this));   // branch if bit #0 is 0 
    Instructions[0x0f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x0f: SLO abs, 6 cycles
    Disassembled[0x0f] =Instruction("SLOR",Instruction::Absolute,6);
    Instructions[0x0f]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0x0f]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
    Instructions[0x0f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0x0f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));// op<<1->op
    Instructions[0x0f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ORAUnit>(this,Emulate65C02));// op->(ea)
    Instructions[0x0f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions10
// Build instruction opcodes 0x10 to 0x1f
template<class Adr>
void CPU::BuildInstructions10(void)
{
  //
  // 0x10: BPL abs, 2 cycles
  Disassembled[0x10] =Instruction("BPL",Instruction::Disp,2);
  Instructions[0x10]->AddStep(new Cat1<BranchDetectUnit<N_Mask,0> >(this));     // branch if N flag cleared
  Instructions[0x10]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x11: ORA (indirect),Y 5* cycles
  Disassembled[0x11] =Instruction("ORA",Instruction::Indirect_Y,5);
  Instructions[0x11]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x11]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnit>(this));  // (op)->op,op+Y->op
  Instructions[0x11]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // (op)->op
  Instructions[0x11]->AddStep(new Cat1<ORAUnit>(this));                         // op|A->A
  Instructions[0x11]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // ORA (zpage)
    Disassembled[0x12] =Instruction("ORA",Instruction::Indirect_Z,5);
    Instructions[0x12]->AddStep(new Cat1<ImmediateUnit>(this));                  // ZPage address->op
    Instructions[0x12]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this)); // (op)->op,op+Y->op
    Instructions[0x12]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                // (op)->op
    Instructions[0x12]->AddStep(new Cat1<ORAUnit>(this));                        // op|A->A
    Instructions[0x12]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x12: JAM
    Disassembled[0x12] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x12]->AddStep(new Cat1<JAMUnit<0x12> >(this));
    Instructions[0x12]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x13] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x13]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x13: SLO (indirect),Y 8 cycles
    Disassembled[0x13] =Instruction("SLOR",Instruction::Indirect_Y,8);
    Instructions[0x13]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
    Instructions[0x13]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this));  // (op)->op,op+Y->op
    Instructions[0x13]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
    Instructions[0x13]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));        // op<<1->op
    Instructions[0x13]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ORAUnit>(this,Emulate65C02));        // op->(ea)
    Instructions[0x13]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // TRB zpage, 5 cycles
    Disassembled[0x14] =Instruction("TRB",Instruction::ZPage,5);
    Instructions[0x14]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x14]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // (op)->op
    Instructions[0x14]->AddStep(new Cat1<TRBUnit>(this));                         // ~A&op->flags,op
    Instructions[0x14]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this));        // write result back out
    Instructions[0x14]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  } else {
    // 0x14: NOP zpage,X 4 cycles 
    Disassembled[0x14] =Instruction("NOPE",Instruction::ZPage_X,4);
    Instructions[0x14]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x14]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0x14]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // (op)->op
    Instructions[0x14]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x15: ORA zpage,X 4 cycles
  Disassembled[0x15] =Instruction("ORA",Instruction::ZPage_X,4);
  Instructions[0x15]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x15]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0x15]->AddStep(new Cat2<ZPageIndirectionUnit<Adr> ,ORAUnit>(this));// (op)->op,op|A->A
  Instructions[0x15]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x16: ASL zpage,X 6 cycles
  Disassembled[0x16] =Instruction("ASL",Instruction::ZPage_X,6);
  Instructions[0x16]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x16]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0x16]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0x16]->AddStep(new Cat1<ASLUnit>(this));                           // op<<1->op
  Instructions[0x16]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->(ea)
  Instructions[0x16]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0x017: RMB1 zpage, 5 cycles
    Disassembled[0x17] =Instruction("RMB1",Instruction::ZPage,5);
    Instructions[0x17]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x17]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x17]->AddStep(new Cat1<RMBUnit<0x02> >(this));                  // reset bit #1
    Instructions[0x17]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0x17]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x17: SLO zpage,X 6 cycles
    Disassembled[0x17] =Instruction("SLOR",Instruction::ZPage_X,6);
    Instructions[0x17]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x17]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0x17]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x17]->AddStep(new Cat1<ASLUnit>(this));                         // op<<1->op
    Instructions[0x17]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr> ,ORAUnit>(this,Emulate65C02)); // op->(ea),op|A->A
    Instructions[0x17]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x18: CLC, 2 cycles
  Disassembled[0x18] =Instruction("CLC",Instruction::NoArgs,2);
  Instructions[0x18]->AddStep(new Cat1<AndToStatusUnit<NOT(C_Mask)> >(this));     // P & (~C) -> P
  Instructions[0x18]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x19: ORA absolute,Y 4* cycles
  Disassembled[0x19] =Instruction("ORA",Instruction::Absolute_Y,4);
  Instructions[0x19]->AddStep(new Cat1<ImmediateUnit>(this));                       // lo->op
  Instructions[0x19]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op, op+Y->op
  Instructions[0x19]->AddStep(new Cat2<IndirectionUnit<Adr> ,ORAUnit>(this));       // (op)->op,op|A->A
  Instructions[0x19]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x1a: INA, 2 cycles
    Disassembled[0x1a] =Instruction("INA",Instruction::NoArgs,2);
    Instructions[0x1a]->AddStep(new Cat3<AccuUnit,INCUnit,LDAUnit>(this));
    Instructions[0x1a]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  } else {
    // 0x1a: NOP, 2 cycles
    Disassembled[0x1a] =Instruction("NOPE",Instruction::NoArgs,2);
    Instructions[0x1a]->AddStep(new Cat1<WaitUnit>(this));                          // delay a cycle
    Instructions[0x1a]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x1b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x1b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x1b: SLO absolute,Y 7 cycles
    Disassembled[0x1b] =Instruction("SLOR",Instruction::Absolute_Y,7);
    Instructions[0x1b]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
    Instructions[0x1b]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this)); // hi->op, op+Y->op
    Instructions[0x1b]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
    Instructions[0x1b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));        // op<<1->op
    Instructions[0x1b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ORAUnit>(this,Emulate65C02));        // op->(ea),op|A->A
    Instructions[0x1b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // TRB absolute, 6 cycles
    Disassembled[0x1c] =Instruction("TRB",Instruction::Absolute,6);
    Instructions[0x1c]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
    Instructions[0x1c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
    Instructions[0x1c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // (op)->op
    Instructions[0x1c]->AddStep(new Cat1<TRBUnit>(this));                           // op&~A->A,flags
    Instructions[0x1c]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this));
    Instructions[0x1c]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x1c: NOP absolute,X 4* cycles
    Disassembled[0x1c] =Instruction("NOPE",Instruction::Absolute_X,4);
    Instructions[0x1c]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
    Instructions[0x1c]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
    Instructions[0x1c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
    Instructions[0x1c]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x1d: ORA absolute,X 4* cycles
  Disassembled[0x1d] =Instruction("ORA",Instruction::Absolute_X,4);
  Instructions[0x1d]->AddStep(new Cat1<ImmediateUnit>(this));                       // lo->op
  Instructions[0x1d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
  Instructions[0x1d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ORAUnit>(this));             // op->ea,(op)->op
  Instructions[0x1d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x1e: ASL absolute,X 7 cycles (6* für 65C02)
  Disassembled[0x1e] =Instruction("ASL",Instruction::Absolute_X,7);
  Instructions[0x1e]->AddStep(new Cat1<ImmediateUnit>(this));                           // lo->op
  if (Emulate65C02)
    Instructions[0x1e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));   // hi->op, op+X->op
  else
    Instructions[0x1e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this));// hi->op, op+X->op
  Instructions[0x1e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x1e]->AddStep(new Cat1<ASLUnit>(this));                               // op<<1->op
  else
    Instructions[0x1e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));      // op<<1->op
  Instructions[0x1e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0x1e]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x1f: BBR1 zpage,disp, 5 cycles
    Disassembled[0x1f] =Instruction("BBR1",Instruction::ZPage_Disp,5);
    Instructions[0x1f]->AddStep(new Cat1<ImmediateUnit>(this));                         // ZPage address->op
    Instructions[0x1f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));            // op->ea,(op)->op
    Instructions[0x1f]->AddStep(new Cat1<BranchBitTestUnit<0x02,0x00> >(this));         // branch if bit #1 is 0
    Instructions[0x1f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x1f: SLO absolute,X 7 cycles
    Disassembled[0x1f] =Instruction("SLOR",Instruction::Absolute_X,7);
    Instructions[0x1f]->AddStep(new Cat1<ImmediateUnit>(this));                         // lo->op
    Instructions[0x1f]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
    Instructions[0x1f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0x1f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ASLUnit>(this,Emulate65C02));      // op<<1->op
    Instructions[0x1f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ORAUnit>(this,Emulate65C02));      // op->(ea),op|A->A
    Instructions[0x1f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions20
// Build instruction opcodes 0x20 to 0x2f
template<class Adr>
void CPU::BuildInstructions20(void)
{
  //
  // 0x20: JSR absolute unit
  Disassembled[0x20] =Instruction("JSR",Instruction::Absolute,6);
  Instructions[0x20]->AddStep(new Cat2<LoadPCUnit<1>,PushUnitExtend<Adr> >(this,Emulate65C02)); // pc+1->op,hi->stack
  Instructions[0x20]->AddStep(new Cat1<PushUnit<Adr> >(this,Emulate65C02));                  // lo->stack
  Instructions[0x20]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x20]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0x20]->AddStep(new Cat1<JMPUnit<0> >(this));                     // op->PC
  Instructions[0x20]->AddStep(new Cat1<DecodeUnit>(this));  
  //
  // 0x21: AND (addr,X): 6 cycles
  Disassembled[0x21] =Instruction("AND",Instruction::Indirect_X,6);
  Instructions[0x21]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x21]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x21]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
  Instructions[0x21]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
  Instructions[0x21]->AddStep(new Cat1<ANDUnit>(this));                         // op&A->A
  Instructions[0x21]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x22: First escape code: Pulls the immediate operand from the code,
  // runs the emulation cycle and executes an RTS
  // This would be a NOP on the CMOS version, a HALT on the NMOS.
  Disassembled[0x22] =Instruction("ESCA",Instruction::Immediate,6);
  Instructions[0x22]->AddStep(new Cat1<ImmediateUnit>(this));                   // fetch operand
  Instructions[0x22]->AddStep(new Cat1<ESCUnit>(this));                         // run the machine emulator
  Instructions[0x22]->AddStep(new Cat1<PullUnit<Adr> >(this));                  // stack->lo
  Instructions[0x22]->AddStep(new Cat1<PullUnitExtend<Adr> >(this));            // stack->hi
  Instructions[0x22]->AddStep(new Cat1<JMPUnit<1> >(this));                     // op+1->PC
  Instructions[0x22]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x23] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x23]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x23: RLA (addr,X): 8 cycles
    Disassembled[0x23] =Instruction("RLAN",Instruction::NoArgs,8);
    Instructions[0x23]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x23]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0x23]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0x23]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0x23]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));// op<<1->op
    Instructions[0x23]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));        // op->ea
    Instructions[0x23]->AddStep(new Cat1<ANDUnit>(this));                         // op&A->A
    Instructions[0x23]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0x24: BIT zpage, 3 cycles
  Disassembled[0x24] =Instruction("BIT",Instruction::ZPage,3);
  Instructions[0x24]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x24]->AddStep(new Cat2<ZPageIndirectionUnit<Adr> ,BITUnit>(this)); // (op)->op,A&op->flags
  Instructions[0x24]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  //
  // 0x25: AND zpage, 3 cycles
  Disassembled[0x25] =Instruction("AND",Instruction::ZPage,3);
  Instructions[0x25]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x25]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,ANDUnit>(this));// (op)->op,op&A->A
  Instructions[0x25]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x26: ROL zpage, 5 cycles
  Disassembled[0x26] =Instruction("ROL",Instruction::ZPage,5);
  Instructions[0x26]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x26]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
  Instructions[0x26]->AddStep(new Cat1<ROLUnit>(this));                         // op<<1->op
  Instructions[0x26]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->ea
  Instructions[0x26]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  //
  if (Emulate65C02) {
    // 0x27: RMB2 zpage, 5 cycles
    Disassembled[0x27] =Instruction("RMB2",Instruction::ZPage,5);
    Instructions[0x27]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x27]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));     // op->ea,(op)->op
    Instructions[0x27]->AddStep(new Cat1<RMBUnit<0x04> >(this));                // reset bit #2
    Instructions[0x27]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->ea
    Instructions[0x27]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x27: RLA zpage, 5 cycles
    Disassembled[0x27] =Instruction("RLAN",Instruction::ZPage,5);
    Instructions[0x27]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x27]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x27]->AddStep(new Cat1<ROLUnit>(this));                       // op<<1->op
    Instructions[0x27]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr> ,ANDUnit>(this,Emulate65C02));// op->ea,op&A->A
    Instructions[0x27]->AddStep(new Cat1<DecodeUnit>(this));                    // decode next instruction
  }
  //
  // 0x28: PLP, 4 cycles
  Disassembled[0x28] =Instruction("PLP",Instruction::NoArgs,4);
  Instructions[0x28]->AddStep(new Cat1<PullUnit<Adr> >(this));                  // stack->op
  Instructions[0x28]->AddStep(new Cat1<SetStatusUnit>(this));                   // op->P
  Instructions[0x28]->AddStep(new Cat1<WaitUnit>(this));                        // does nothing here.
  Instructions[0x28]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  //
  // 0x29: AND immediate, 2 cycles
  Disassembled[0x29] =Instruction("AND",Instruction::Immediate,2);
  Instructions[0x29]->AddStep(new Cat2<ImmediateUnit,ANDUnit>(this));           // #->op,op|A->A
  Instructions[0x29]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  //
  // 0x2a: ROL A, 2 cycles
  Disassembled[0x2a] =Instruction("ROL",Instruction::Accu,2);
  Instructions[0x2a]->AddStep(new Cat3<AccuUnit,ROLUnit,LDAUnit>(this));
  Instructions[0x2a]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x2b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x2b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0x2b] =Instruction("UNST",Instruction::NoArgs,0);
    // 0x2b: ANC, 3 cycles FIXME: Should this be two? I won't believe it.
    // Actually, the result of this instruction cannot be predicted exactly,
    // it seems to differ depending on conditions I cannot control. The same
    // P registers generate differning results.
    Instructions[0x2b]->AddStep(new Cat1<UnstableUnit<0x2b> >(this));           // just crash
    Instructions[0x2b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x2c: BIT abs, 4 cycles
  Disassembled[0x2c] =Instruction("BIT",Instruction::Absolute,4);
  Instructions[0x2c]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x2c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0x2c]->AddStep(new Cat2<IndirectionUnit<Adr> ,BITUnit>(this));         // (op)->op, op&A->Flags
  Instructions[0x2c]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x2d: AND abs, 4 cycles
  Disassembled[0x2d] =Instruction("AND",Instruction::Absolute,4);
  Instructions[0x2d]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x2d]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0x2d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ANDUnit>(this));         // (op)->op,op&A->A
  Instructions[0x2d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x2e: ROL abs, 6 cycles
  Disassembled[0x2e] =Instruction("ROL",Instruction::Absolute,6);
  Instructions[0x2e]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x2e]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0x2e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x2e]->AddStep(new Cat1<ROLUnit>(this));                       // op<<1->op
  else
    Instructions[0x2e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));    // op<<1->op
  Instructions[0x2e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));              // op->(ea)
  Instructions[0x2e]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x2f: BBR2 zpage,disp, 5 cycles
    Disassembled[0x2f] =Instruction("BBR2",Instruction::ZPage_Disp,5);
    Instructions[0x2f]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x2f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x2f]->AddStep(new Cat1<BranchBitTestUnit<0x04,0x00> >(this)); // branch if bit #2 is 0  
    Instructions[0x2f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x2f: RLA abs, 6 cycles
    Disassembled[0x2f] =Instruction("RLAN",Instruction::Absolute,6);
    Instructions[0x2f]->AddStep(new Cat1<ImmediateUnit>(this));                 // lo->op
    Instructions[0x2f]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));    // hi->op
    Instructions[0x2f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x2f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));    // op<<1->op
    Instructions[0x2f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ANDUnit>(this,Emulate65C02));    // op->(ea)
    Instructions[0x2f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions30
// Build instruction opcodes 0x30 to 0x3f
template<class Adr>
void CPU::BuildInstructions30(void)
{
  //
  // 0x30: BMI abs, 2 cycles
  Disassembled[0x30] =Instruction("BMI",Instruction::Disp,2);
  Instructions[0x30]->AddStep(new Cat1<BranchDetectUnit<N_Mask,N_Mask> >(this));// branch if N flag set
  Instructions[0x30]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x31: AND (indirect),Y 5* cycles
  Disassembled[0x31] =Instruction("AND",Instruction::Indirect_Y,5);
  Instructions[0x31]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x31]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnit>(this));   // (op)->op,op+Y->op
  Instructions[0x31]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
  Instructions[0x31]->AddStep(new Cat1<ANDUnit>(this));                         // op&A->A
  Instructions[0x31]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // AND (zpage)
    Disassembled[0x32] =Instruction("AND",Instruction::Indirect_Z,5);
    Instructions[0x32]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x32]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));// (op)->op,op+Y->op
    Instructions[0x32]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // (op)->op
    Instructions[0x32]->AddStep(new Cat1<ANDUnit>(this));                       // op&A->A
    Instructions[0x32]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x32: JAM
    Disassembled[0x32] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x32]->AddStep(new Cat1<JAMUnit<0x32> >(this));
    Instructions[0x32]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x33] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x33]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x33: RLA (indirect),Y 8 cycles
    Disassembled[0x33] =Instruction("RLAN",Instruction::Indirect_Y,8);
    Instructions[0x33]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x33]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this));   // (op)->op,op+Y->op
    Instructions[0x33]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x33]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));    // op<<1->op
    Instructions[0x33]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ANDUnit>(this,Emulate65C02));    // op->(ea)
    Instructions[0x33]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // BIT zpage,x 4 cycles
    Disassembled[0x34] =Instruction("BIT",Instruction::ZPage_X,3);
    Instructions[0x34]->AddStep(new Cat2<ImmediateUnit,AddXUnitZero>(this));    // ZPage address->op
    Instructions[0x34]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // (op)->op,A&op->flags
    Instructions[0x34]->AddStep(new Cat1<BITUnit>(this));                       // (op)->op,A&op->flags
    Instructions[0x34]->AddStep(new Cat1<DecodeUnit>(this));                                       
  } else {
    // 0x34: NOP zpage,X 4 cycles 
    Disassembled[0x34] =Instruction("NOPE",Instruction::ZPage_X,4);
    Instructions[0x34]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x34]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0x34]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // (op)->op
    Instructions[0x34]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x35: AND zpage,X 4 cycles
  Disassembled[0x35] =Instruction("AND",Instruction::ZPage_X,4);
  Instructions[0x35]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x35]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x35]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,ANDUnit>(this));// (op)->op,op&A->A
  Instructions[0x35]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x36: ROL zpage,X 6 cycles
  Disassembled[0x36] =Instruction("ROL",Instruction::ZPage_X,6);
  Instructions[0x36]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x36]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x36]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
  Instructions[0x36]->AddStep(new Cat1<ROLUnit>(this));                         // op<<1->op
  Instructions[0x36]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->(ea)
  Instructions[0x36]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0x37: RMB3 zpage, 5 cycles
    Disassembled[0x37] =Instruction("RMB3",Instruction::ZPage,5);
    Instructions[0x37]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x37]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x37]->AddStep(new Cat1<RMBUnit<0x08> >(this));                // reset bit #3
    Instructions[0x37]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->ea
    Instructions[0x37]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x37: RLA zpage,X 6 cycles
    Disassembled[0x37] =Instruction("RLAN",Instruction::ZPage_X,6);
    Instructions[0x37]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x37]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0x37]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x37]->AddStep(new Cat1<ROLUnit>(this));                       // op<<1->op
    Instructions[0x37]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,ANDUnit>(this,Emulate65C02)); // op->(ea),op&A->A
    Instructions[0x37]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x38: SEC, 2 cycles
  Disassembled[0x38] =Instruction("SEC",Instruction::NoArgs,2);
  Instructions[0x38]->AddStep(new Cat1<OrToStatusUnit<C_Mask> >(this));         // P | C -> P
  Instructions[0x38]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x39: AND absolute,Y 4* cycles
  Disassembled[0x39] =Instruction("AND",Instruction::Absolute_Y,4);
  Instructions[0x39]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x39]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op, op+Y->op
  Instructions[0x39]->AddStep(new Cat2<IndirectionUnit<Adr> ,ANDUnit>(this));         // (op)->op,op&A->A
  Instructions[0x39]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // DEA, 2 cycles
    Disassembled[0x3a] =Instruction("DEA",Instruction::NoArgs,2);
    Instructions[0x3a]->AddStep(new Cat3<AccuUnit,DECUnit,LDAUnit>(this));
    Instructions[0x3a]->AddStep(new Cat1<DecodeUnit>(this));                     // decode next instruction
  } else {
    // 0x3a: NOP, 2 cycles
    Disassembled[0x3a] =Instruction("NOPE",Instruction::NoArgs,2);
    Instructions[0x3a]->AddStep(new Cat1<WaitUnit>(this));                       // delay a cycle
    Instructions[0x3a]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x3b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x3b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x3b: RLA absolute,Y 7 cycles
    Disassembled[0x3b] =Instruction("RLAN",Instruction::Absolute_Y,7);
    Instructions[0x3b]->AddStep(new Cat1<ImmediateUnit>(this));                  // lo->op
    Instructions[0x3b]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this)); // hi->op, op+Y->op
    Instructions[0x3b]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                // op->ea,(op)->op
    Instructions[0x3b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));     // op<<1->op
    Instructions[0x3b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ANDUnit>(this,Emulate65C02));     // op->(ea),op&A->A
    Instructions[0x3b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) { 
    // BIT absolute,X
    Disassembled[0x3c] =Instruction("BIT",Instruction::Absolute_X,4);
    Instructions[0x3c]->AddStep(new Cat1<ImmediateUnit>(this));                        // lo->op
    Instructions[0x3c]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));  // hi->op
    Instructions[0x3c]->AddStep(new Cat2<IndirectionUnit<Adr> ,BITUnit>(this));        // (op)->op, op&A->Flags
    Instructions[0x3c]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x3c: NOP absolute,X 4* cycles
    Disassembled[0x3c] =Instruction("NOPE",Instruction::Absolute_X,4);
    Instructions[0x3c]->AddStep(new Cat1<ImmediateUnit>(this));                        // lo->op
    Instructions[0x3c]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));  // hi->op, op+X->op
    Instructions[0x3c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                      // op->ea,(op)->op
    Instructions[0x3c]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x3d: AND absolute,X 4* cycles
  Disassembled[0x3d] =Instruction("AND",Instruction::Absolute_X,4);
  Instructions[0x3d]->AddStep(new Cat1<ImmediateUnit>(this));                          // lo->op
  Instructions[0x3d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));    // hi->op, op+X->op
  Instructions[0x3d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ANDUnit>(this));                // op->ea,(op)->op
  Instructions[0x3d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x3e: ROL absolute,X 7 cycles
  Disassembled[0x3e] =Instruction("ROL",Instruction::Absolute_X,7);
  Instructions[0x3e]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  if (Emulate65C02)
    Instructions[0x3e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));  // hi->op, op+X->op
  else
    Instructions[0x3e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
  Instructions[0x3e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x3e]->AddStep(new Cat1<ROLUnit>(this));                         // op<<1->op
  else
    Instructions[0x3e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));      // op<<1->op
  Instructions[0x3e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0x3e]->AddStep(new Cat1<DecodeUnit>(this));
  //  
  if (Emulate65C02) {
    // 0x3f: BBR3 zpage,disp, 5 cycles
    Disassembled[0x3f] =Instruction("BBR3",Instruction::ZPage_Disp,5);
    Instructions[0x3f]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x3f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x3f]->AddStep(new Cat1<BranchBitTestUnit<0x08,0x00> >(this));   // branch if bit #3 is 0  
    Instructions[0x3f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x3f: RLA absolute,X 7 cycles
    Disassembled[0x3f] =Instruction("RLAN",Instruction::Absolute_X,7);
    Instructions[0x3f]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0x3f]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
    Instructions[0x3f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0x3f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ROLUnit>(this,Emulate65C02));      // op<<1->op
    Instructions[0x3f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ANDUnit>(this,Emulate65C02));      // op->(ea),op&A->A
    Instructions[0x3f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions40
// Build instruction opcodes 0x40 to 0x4f
template<class Adr>
void CPU::BuildInstructions40(void)
{
  //
  // 0x40: RTI  6 cycles
  Disassembled[0x40] =Instruction("RTI",Instruction::NoArgs,6);
  Instructions[0x40]->AddStep(new Cat1<PullUnit<Adr> >(this));                    // stack->op
  Instructions[0x40]->AddStep(new Cat1<SetStatusUnit>(this));                     // op->P
  Instructions[0x40]->AddStep(new Cat1<PullUnit<Adr> >(this));                    // stack->lo
  Instructions[0x40]->AddStep(new Cat1<PullUnitExtend<Adr> >(this));              // stack->hi
  Instructions[0x40]->AddStep(new Cat1<JMPUnit<0> >(this));                       // op->PC
  Instructions[0x40]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x41: EOR (addr,X): 6 cycles
  Disassembled[0x41] =Instruction("EOR",Instruction::Indirect_X,6);
  Instructions[0x41]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x41]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0x41]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));    // (op)->op
  Instructions[0x41]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // (op)->op
  Instructions[0x41]->AddStep(new Cat1<EORUnit>(this));                           // op^A->A
  Instructions[0x41]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0x42: NOP
    Disassembled[0x42] =Instruction("NOPE",Instruction::Immediate,2);
    Instructions[0x42]->AddStep(new Cat1<ImmediateUnit>(this));                                      
    Instructions[0x42]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x42: JAM
    Disassembled[0x42] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x42]->AddStep(new Cat1<JAMUnit<0x42> >(this));                  // just crash
    Instructions[0x42]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x43] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x43]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x43: LSE (addr,X): 8 cycles
    Disassembled[0x43] =Instruction("LSEO",Instruction::Indirect_X,8);
    Instructions[0x43]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x43]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0x43]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0x43]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0x43]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));// op>>1->op
    Instructions[0x43]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));        // op->ea
    Instructions[0x43]->AddStep(new Cat1<EORUnit>(this));                         // op^A->A
    Instructions[0x43]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0x44: NOP zpage, 3 cycles
  Disassembled[0x44] =Instruction("NOPE",Instruction::ZPage,3);
  Instructions[0x44]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x44]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // (op)->op
  Instructions[0x44]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0x45: EOR zpage, 3 cycles
  Disassembled[0x45] =Instruction("EOR",Instruction::ZPage,3);
  Instructions[0x45]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x45]->AddStep(new Cat2<ZPageIndirectionUnit<Adr> ,EORUnit>(this));// (op)->op,op^A->A
  Instructions[0x45]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x46: LSR zpage, 5 cycles
  Disassembled[0x46] =Instruction("LSR",Instruction::ZPage,5);
  Instructions[0x46]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x46]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0x46]->AddStep(new Cat1<LSRUnit>(this));                           // op>>1->op
  Instructions[0x46]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->ea
  Instructions[0x46]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  // 
  if (Emulate65C02) {
    // 0x047: RMB4 zpage, 5 cycles
    Disassembled[0x47] =Instruction("RMB4",Instruction::ZPage,5);
    Instructions[0x47]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x47]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x47]->AddStep(new Cat1<RMBUnit<0x10> >(this));                  // reset bit #3
    Instructions[0x47]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0x47]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x47: LSE zpage, 5 cycles
    Disassembled[0x47] =Instruction("LSEO",Instruction::ZPage,5);
    Instructions[0x47]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x47]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x47]->AddStep(new Cat1<LSRUnit>(this));                         // op>>1->op
    Instructions[0x47]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,EORUnit>(this,Emulate65C02)); // op->ea,op^A->A
    Instructions[0x47]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0x48: PHA, 3 cycles
  Disassembled[0x48] =Instruction("PHA",Instruction::NoArgs,3);
  Instructions[0x48]->AddStep(new Cat1<AccuUnit>(this));                          // A->op
  Instructions[0x48]->AddStep(new Cat1<PushUnit<Adr> >(this,Emulate65C02));                    // op->stack
  Instructions[0x48]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0x49: EOR immediate, 2 cycles
  Disassembled[0x49] =Instruction("EOR",Instruction::Immediate,2);
  Instructions[0x49]->AddStep(new Cat2<ImmediateUnit,EORUnit>(this));             // #->op,op^A->A
  Instructions[0x49]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0x4a: LSR A, 2 cycles
  Disassembled[0x4a] =Instruction("LSR",Instruction::Accu,2);
  Instructions[0x4a]->AddStep(new Cat3<AccuUnit,LSRUnit,LDAUnit>(this));
  Instructions[0x4a]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x4b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x4b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x4b: ALR, 3 cycles FIXME: Should this be two? I won't believe it.
    // This is instable, the final LDA is not always executed.
    Disassembled[0x4b] =Instruction("ANLR",Instruction::Immediate,3);
    Instructions[0x4b]->AddStep(new Cat2<ImmediateUnit,ANDUnit>(this));           // #->op,op&A->A
    Instructions[0x4b]->AddStep(new Cat2<LSRUnit,LDAUnit>(this));                 // op>>1->op
    Instructions[0x4b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x4c: JMP abs, 3 cycles
  Disassembled[0x4c] =Instruction("JMP",Instruction::Absolute,3);
  Instructions[0x4c]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0x4c]->AddStep(new Cat2<ImmediateWordExtensionUnit,JMPUnit<0> >(this)); // hi->op, op->PC
  Instructions[0x4c]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x4d: EOR abs, 4 cycles
  Disassembled[0x4d] =Instruction("EOR",Instruction::Absolute,4);
  Instructions[0x4d]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0x4d]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0x4d]->AddStep(new Cat2<IndirectionUnit<Adr> ,EORUnit>(this));           // (op)->op,op^A->A
  Instructions[0x4d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x4e: LSR abs, 6 cycles
  Disassembled[0x4e] =Instruction("LSR",Instruction::Absolute,6);
  Instructions[0x4e]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0x4e]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0x4e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x4e]->AddStep(new Cat1<LSRUnit>(this));                         // op>>1->op
  else
    Instructions[0x4e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));      // op>>1->op
  Instructions[0x4e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0x4e]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x4f: BBR4 zpage,disp, 5 cycles
    Disassembled[0x4f] =Instruction("BBR4",Instruction::ZPage_Disp,5);
    Instructions[0x4f]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0x4f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0x4f]->AddStep(new Cat1<BranchBitTestUnit<0x10,0x00> >(this));   // branch if bit #4 is 0  
    Instructions[0x4f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x4f: LSE abs, 6 cycles
    Disassembled[0x4f] =Instruction("LSEO",Instruction::Absolute,6);
    Instructions[0x4f]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0x4f]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
    Instructions[0x4f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0x4f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));      // op>>1->op
    Instructions[0x4f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,EORUnit>(this,Emulate65C02));      // op->(ea)
    Instructions[0x4f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions50
// Build instruction opcodes 0x50 to 0x5f
template<class Adr>
void CPU::BuildInstructions50(void)
{
  //
  // 0x50: BVC abs, 2 cycles
  Disassembled[0x50] =Instruction("BVC",Instruction::Disp,2);
  Instructions[0x50]->AddStep(new Cat1<BranchDetectUnit<V_Mask,0> >(this));       // branch if V flag clear
  Instructions[0x50]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x51: EOR (indirect),Y 5* cycles
  Disassembled[0x51] =Instruction("EOR",Instruction::Indirect_Y,5);
  Instructions[0x51]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0x51]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnit>(this));   // (op)->op,op+Y->op
  Instructions[0x51]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                    // (op)->op
  Instructions[0x51]->AddStep(new Cat1<EORUnit>(this));                           // op^A->A
  Instructions[0x51]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x52: JAM
  if (Emulate65C02) {
    // EOR (zpage)
    Disassembled[0x52] =Instruction("EOR",Instruction::Indirect_Z,5);
    Instructions[0x52]->AddStep(new Cat1<ImmediateUnit>(this));                    // ZPage address->op
    Instructions[0x52]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));   // (op)->op,op+Y->op
    Instructions[0x52]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));            // (op)->op
    Instructions[0x52]->AddStep(new Cat1<EORUnit>(this));                          // op|A->A
    Instructions[0x52]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0x52] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x52]->AddStep(new Cat1<JAMUnit<0x52> >(this));
    Instructions[0x52]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //  
  if (Emulate65C02) {
    // NOP
    Disassembled[0x53] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x53]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x53: LSE (indirect),Y 8 cycles
    Disassembled[0x53] =Instruction("LSEO",Instruction::Indirect_Y,8);
    Instructions[0x53]->AddStep(new Cat1<ImmediateUnit>(this));                    // ZPage address->op
    Instructions[0x53]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this));// (op)->op,op+Y->op
    Instructions[0x53]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                  // op->ea,(op)->op
    Instructions[0x53]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));       // op>>1->op
    Instructions[0x53]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,EORUnit>(this,Emulate65C02));       // op->(ea)
    Instructions[0x53]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x54: NOP zpage,X 4 cycles
  Disassembled[0x54] =Instruction("NOPE",Instruction::ZPage_X,4);
  Instructions[0x54]->AddStep(new Cat1<ImmediateUnit>(this));                      // ZPage address->op
  Instructions[0x54]->AddStep(new Cat1<AddXUnitZero>(this));                       // op+X->op
  Instructions[0x54]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));         // (op)->op
  Instructions[0x54]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x55: EOR zpage,X 4 cycles
  Disassembled[0x55] =Instruction("EOR",Instruction::ZPage_X,4);
  Instructions[0x55]->AddStep(new Cat1<ImmediateUnit>(this));                      // ZPage address->op
  Instructions[0x55]->AddStep(new Cat1<AddXUnitZero>(this));                       // op+X->op
  Instructions[0x55]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,EORUnit>(this));  // (op)->op,op^A->A
  Instructions[0x55]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x56: LSR zpage,X 6 cycles
  Disassembled[0x56] =Instruction("LSR",Instruction::ZPage_X,6);
  Instructions[0x56]->AddStep(new Cat1<ImmediateUnit>(this));                      // ZPage address->op
  Instructions[0x56]->AddStep(new Cat1<AddXUnitZero>(this));                       // op+X->op
  Instructions[0x56]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));         // op->ea,(op)->op
  Instructions[0x56]->AddStep(new Cat1<LSRUnit>(this));                            // op>>1->op
  Instructions[0x56]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));      // op->(ea)
  Instructions[0x56]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x57: RMB5 zpage, 5 cycles
    Disassembled[0x57] =Instruction("RMB5",Instruction::ZPage,5);
    Instructions[0x57]->AddStep(new Cat1<ImmediateUnit>(this));                    // ZPage address->op
    Instructions[0x57]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));       // op->ea,(op)->op
    Instructions[0x57]->AddStep(new Cat1<RMBUnit<0x20> >(this));                   // reset bit #5
    Instructions[0x57]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));    // op->ea
    Instructions[0x57]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x57: LSE zpage,X 6 cycles 
    Disassembled[0x57] =Instruction("LSEO",Instruction::ZPage_X,6);
    Instructions[0x57]->AddStep(new Cat1<ImmediateUnit>(this));                    // ZPage address->op
    Instructions[0x57]->AddStep(new Cat1<AddXUnitZero>(this));                     // op+X->op
    Instructions[0x57]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));       // op->ea,(op)->op
    Instructions[0x57]->AddStep(new Cat1<LSRUnit>(this));                          // op>>1->op
    Instructions[0x57]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,EORUnit>(this,Emulate65C02));  // op->(ea),op^A->A
    Instructions[0x57]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x58: CLI, 2 cycles
  Disassembled[0x58] =Instruction("CLI",Instruction::NoArgs,2);
  Instructions[0x58]->AddStep(new Cat1<AndToStatusUnit<NOT(I_Mask)> >(this));      // P & ~I -> P
  Instructions[0x58]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x59: EOR absolute,Y 4* cycles
  Disassembled[0x59] =Instruction("EOR",Instruction::Absolute_Y,4);
  Instructions[0x59]->AddStep(new Cat1<ImmediateUnit>(this));                      // lo->op
  Instructions[0x59]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this));// hi->op, op+Y->op
  Instructions[0x59]->AddStep(new Cat2<IndirectionUnit<Adr> ,EORUnit>(this));            // (op)->op,op^A->A
  Instructions[0x59]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // PHY, 3 cycles
    Disassembled[0x5a] =Instruction("PHY",Instruction::NoArgs,3);
    Instructions[0x5a]->AddStep(new Cat1<YUnit>(this));                            // Y->op
    Instructions[0x5a]->AddStep(new Cat1<PushUnit<Adr> >(this));                   // op->stack
    Instructions[0x5a]->AddStep(new Cat1<DecodeUnit>(this));     
  } else {
    // 0x5a: NOP, 2 cycles
    Disassembled[0x5a] =Instruction("NOPE",Instruction::NoArgs,2);
    Instructions[0x5a]->AddStep(new Cat1<WaitUnit>(this));                         // delay a cycle
    Instructions[0x5a]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x5b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x5b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x5b: LSE absolute,Y 7 cycles
    Disassembled[0x5b] =Instruction("LSEO",Instruction::Absolute,7);
    Instructions[0x5b]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
    Instructions[0x5b]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this));            // hi->op, op+Y->op
    Instructions[0x5b]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                  // op->ea,(op)->op
    Instructions[0x5b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));       // op>>1->op
    Instructions[0x5b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,EORUnit>(this,Emulate65C02));       // op->(ea),op^A->A
    Instructions[0x5b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x5c: NOP absolute,X 8 cycles
  Disassembled[0x5c] =Instruction("NOPE",Instruction::Absolute_X,8);
  Instructions[0x5c]->AddStep(new Cat1<ImmediateUnit>(this));                      // lo->op
  Instructions[0x5c]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this));// hi->op, op+X->op
  Instructions[0x5c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                    // op->ea,(op)->op
  Instructions[0x5c]->AddStep(new Cat1<WaitUnit>(this));
  Instructions[0x5c]->AddStep(new Cat1<WaitUnit>(this));
  Instructions[0x5c]->AddStep(new Cat1<WaitUnit>(this));
  Instructions[0x5c]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x5d: EOR absolute,X 4* cycles
  Disassembled[0x5d] =Instruction("EOR",Instruction::Absolute_X,4);
  Instructions[0x5d]->AddStep(new Cat1<ImmediateUnit>(this));                      // lo->op
  Instructions[0x5d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));// hi->op, op+X->op
  Instructions[0x5d]->AddStep(new Cat2<IndirectionUnit<Adr> ,EORUnit>(this));            // op->ea,(op)->op
  Instructions[0x5d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x5e: LSR absolute,X 7 cycles
  Disassembled[0x5e] =Instruction("LSR",Instruction::Absolute_X,7);
  Instructions[0x5e]->AddStep(new Cat1<ImmediateUnit>(this));                             // lo->op
  if (Emulate65C02) {
    Instructions[0x5e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));     // hi->op, op+X->op
  } else {
    Instructions[0x5e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
  }
  Instructions[0x5e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                    // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x5e]->AddStep(new Cat1<LSRUnit>(this));                          // op>>1->op
  else
    Instructions[0x5e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));       // op>>1->op
  Instructions[0x5e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                 // op->(ea)
  Instructions[0x5e]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x5f: BBR5 zpage,disp, 5 cycles
    Disassembled[0x5f] =Instruction("BBR5",Instruction::ZPage_Disp,5);
    Instructions[0x5f]->AddStep(new Cat1<ImmediateUnit>(this));                    // ZPage address->op
    Instructions[0x5f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));       // op->ea,(op)->op
    Instructions[0x5f]->AddStep(new Cat1<BranchBitTestUnit<0x20,0x00> >(this));    // branch if bit #5 is 0  
    Instructions[0x5f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x5f: LSE absolute,X 7 cycles
    Disassembled[0x5f] =Instruction("LSEO",Instruction::Absolute_X,7);
    Instructions[0x5f]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
    Instructions[0x5f]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this));// hi->op, op+X->op
    Instructions[0x5f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                  // op->ea,(op)->op
    Instructions[0x5f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,LSRUnit>(this,Emulate65C02));       // op>>1->op
    Instructions[0x5f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,EORUnit>(this,Emulate65C02));       // op->(ea),op^A->A
    Instructions[0x5f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions60
// Build instruction opcodes 0x60 to 0x6f
template<class Adr>
void CPU::BuildInstructions60(void)
{
  //  
  // 0x60: RTS  6 cycles
  Disassembled[0x60] =Instruction("RTS",Instruction::NoArgs,6);
  Instructions[0x60]->AddStep(new Cat1<WaitUnit>(this));
  Instructions[0x60]->AddStep(new Cat1<PullUnit<Adr> >(this));                     // stack->lo
  Instructions[0x60]->AddStep(new Cat1<PullUnitExtend<Adr> >(this));               // stack->hi
  Instructions[0x60]->AddStep(new Cat1<WaitUnit>(this));
  Instructions[0x60]->AddStep(new Cat1<JMPUnit<1> >(this));                        // op+1->PC
  Instructions[0x60]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x61: ADC (addr,X): 6 cycles
  Disassembled[0x61] =Instruction("ADC",Instruction::Indirect_X,6);
  Instructions[0x61]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x61]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x61]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
  Instructions[0x61]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
  if (Emulate65C02) {
    Instructions[0x61]->AddStep(new Cat1<ADCUnitFixed>(this));                  // op+A->A
  } else {
    Instructions[0x61]->AddStep(new Cat1<ADCUnit>(this));                       // op+A->A
  }
  Instructions[0x61]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x62: NOP
    Disassembled[0x62] =Instruction("NOPE",Instruction::Immediate,2);
    Instructions[0x62]->AddStep(new Cat1<ImmediateUnit> (this));                                  
    Instructions[0x62]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x62: JAM
    Disassembled[0x62] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x62]->AddStep(new Cat1<JAMUnit<0x62> >(this));                // just crash
    Instructions[0x62]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x63] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x63]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x63: RRA (addr,X): 8 cycles
    Disassembled[0x63] =Instruction("RRAD",Instruction::Indirect_X,8);
    Instructions[0x63]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x63]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0x63]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));// (op)->op
    Instructions[0x63]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));         // op->ea,(op)->op
    Instructions[0x63]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
    Instructions[0x63]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));      // op->ea
    Instructions[0x63]->AddStep(new Cat1<ADCUnit>(this));                       // op+A->A
    Instructions[0x63]->AddStep(new Cat1<DecodeUnit>(this));                    // decode next instruction
  }
  //
  if (Emulate65C02) {
    // STZ ZPage
    Disassembled[0x64] =Instruction("STZ",Instruction::ZPage,3);
    Instructions[0x64]->AddStep(new Cat2<ImmediateUnit,ZeroUnit>(this));        // address->op,op->ea,A->op
    Instructions[0x64]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->(ea)
    Instructions[0x64]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x64: NOP zpage, 3 cycles
    Disassembled[0x64] =Instruction("NOPE",Instruction::ZPage,3);
    Instructions[0x64]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x64]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // (op)->op
    Instructions[0x64]->AddStep(new Cat1<DecodeUnit>(this));  
  }
  //
  // 0x65: ADC zpage, 3 cycles
  Disassembled[0x65] =Instruction("ADC",Instruction::ZPage,3);
  Instructions[0x65]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  if (Emulate65C02) {
    Instructions[0x65]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,ADCUnitFixed>(this));// op+A->A
  } else {
    Instructions[0x65]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,ADCUnit>(this));     // op+A->A
  }
  Instructions[0x65]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x66: ROR zpage, 5 cycles
  Disassembled[0x66] =Instruction("ROR",Instruction::ZPage,5);
  Instructions[0x66]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x66]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
  Instructions[0x66]->AddStep(new Cat1<RORUnit>(this));                         // op>>1->op
  Instructions[0x66]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->ea
  Instructions[0x66]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  // 
  if (Emulate65C02) {
    // 0x67: RMB6 zpage, 5 cycles
    Disassembled[0x67] =Instruction("RMB6",Instruction::ZPage,5);
    Instructions[0x67]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x67]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x67]->AddStep(new Cat1<RMBUnit<0x40> >(this));                // reset bit #6
    Instructions[0x67]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->ea
    Instructions[0x67]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x67: RRA zpage, 5 cycles
    Disassembled[0x67] =Instruction("RRAD",Instruction::ZPage,5);
    Instructions[0x67]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x67]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x67]->AddStep(new Cat1<RORUnit>(this));                       // op>>1->op
    Instructions[0x67]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,ADCUnit>(this,Emulate65C02)); // op->ea,op+A->A
    Instructions[0x67]->AddStep(new Cat1<DecodeUnit>(this));                    // decode next instruction
  }
  //
  // 0x68: PLA, 4 cycles
  Disassembled[0x68] =Instruction("PLA",Instruction::NoArgs,4);
  Instructions[0x68]->AddStep(new Cat1<PullUnit<Adr> >(this));                  // stack->op
  Instructions[0x68]->AddStep(new Cat1<LDAUnit>(this));                         // A->op
  Instructions[0x68]->AddStep(new Cat1<WaitUnit>(this));                        // used for whatever
  Instructions[0x68]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  //
  // 0x69: ADC immediate, 2 cycles
  Disassembled[0x69] =Instruction("ADC",Instruction::Immediate,2);
  if (Emulate65C02) {
    Instructions[0x69]->AddStep(new Cat2<ImmediateUnit,ADCUnitFixed>(this));    // op+A->A
  } else {
    Instructions[0x69]->AddStep(new Cat2<ImmediateUnit,ADCUnit>(this));         // op+A->A
  }
  Instructions[0x69]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  //
  // 0x6a: ROR A, 2 cycles
  Disassembled[0x6a] =Instruction("ROR",Instruction::Accu,2);
  Instructions[0x6a]->AddStep(new Cat3<AccuUnit,RORUnit,LDAUnit>(this));
  Instructions[0x6a]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x6b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x6b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x6b: ARR, 3 cycles FIXME: Should this be two? I won't believe it. 
    Disassembled[0x6b] =Instruction("ANDR",Instruction::Immediate,3);
    Instructions[0x6b]->AddStep(new Cat2<ImmediateUnit,ANDUnit>(this));         // op&A->A
    Instructions[0x6b]->AddStep(new Cat3<RORUnit,LDAUnit,BITWierdUnit>(this));               // op>>1->op
    Instructions[0x6b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x6c: JMP (ind), 5 cycles
  Disassembled[0x6c] =Instruction("JMP",Instruction::Indirect,5);
  Instructions[0x6c]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x6c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0x6c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->lo
  if (Emulate65C02) {
    // The 65C02 fixes this hardware bug.
    Instructions[0x6c]->AddStep(new Cat2<IndirectionUnitExtendFixed,JMPUnit<0> >(this)); // (ea+1)->hi
  } else {
    Instructions[0x6c]->AddStep(new Cat2<IndirectionUnitExtend,JMPUnit<0> >(this)); // (ea+1)->hi
  }
  Instructions[0x6c]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x6d: ADC abs, 4 cycles
  Disassembled[0x6d] =Instruction("ADC",Instruction::Absolute,4);
  Instructions[0x6d]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x6d]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  if (Emulate65C02) {
    Instructions[0x6d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ADCUnitFixed>(this));  // op+A->A
  } else {
    Instructions[0x6d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ADCUnit>(this));       // op+A->A
  }
  Instructions[0x6d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x6e: ROR abs, 6 cycles
  Disassembled[0x6e] =Instruction("ROR",Instruction::Absolute,6);
  Instructions[0x6e]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x6e]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0x6e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x6e]->AddStep(new Cat1<RORUnit>(this));                       // op>>1->op
  else
    Instructions[0x6e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
  Instructions[0x6e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));              // op->(ea)
  Instructions[0x6e]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0x6f: BBR6 zpage,disp, 5 cycles
    Disassembled[0x6f] =Instruction("BBR6",Instruction::ZPage_Disp,5);
    Instructions[0x6f]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x6f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x6f]->AddStep(new Cat1<BranchBitTestUnit<0x40,0x00> >(this)); // branch if bit #6 is 0  
    Instructions[0x6f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x6f: RRA abs, 6 cycles
    Disassembled[0x6f] =Instruction("RRAD",Instruction::Absolute,6);
    Instructions[0x6f]->AddStep(new Cat1<ImmediateUnit>(this));                 // lo->op
    Instructions[0x6f]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));    // hi->op
    Instructions[0x6f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x6f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
    Instructions[0x6f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ADCUnit>(this,Emulate65C02));    // op->(ea)
    Instructions[0x6f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions70
// Build instruction opcodes 0x70 to 0x7f
template<class Adr>
void CPU::BuildInstructions70(void)
{
  //
  // 0x70: BVS abs, 2 cycles
  Disassembled[0x70] =Instruction("BVS",Instruction::Disp,2);
  Instructions[0x70]->AddStep(new Cat1<BranchDetectUnit<V_Mask,V_Mask> >(this));// branch if V flag set
  Instructions[0x70]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x71: ADC (indirect),Y 5* cycles
  Disassembled[0x71] =Instruction("ADC",Instruction::Indirect_Y,5);
  Instructions[0x71]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x71]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnit>(this));   // (op)->op,op+Y->op
  Instructions[0x71]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
  if (Emulate65C02) {
    Instructions[0x71]->AddStep(new Cat1<ADCUnitFixed>(this));                  // op+A->A
  } else {
    Instructions[0x71]->AddStep(new Cat1<ADCUnit>(this));                       // op+A->A
  }
  Instructions[0x71]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // ADC (zpage)
    Disassembled[0x72] =Instruction("ADC",Instruction::Indirect_Z,5);
    Instructions[0x72]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x72]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));// (op)->op,op+Y->op
    Instructions[0x72]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));         // (op)->op
    Instructions[0x72]->AddStep(new Cat1<ADCUnitFixed>(this));                  // op+A->A
    Instructions[0x72]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x72: JAM
    Disassembled[0x72] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x72]->AddStep(new Cat1<JAMUnit<0x72> >(this));
    Instructions[0x72]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x73] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x73]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x73: RRA (indirect),Y 8 cycles
    Disassembled[0x73] =Instruction("RRAD",Instruction::Indirect_Y,8);
    Instructions[0x73]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x73]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this));// (op)->op,op+Y->op
    Instructions[0x73]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x73]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
    Instructions[0x73]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ADCUnit>(this,Emulate65C02));    // op+A->A
    Instructions[0x73]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x74: NOP zpage,X 4 cycles
  if (Emulate65C02) {
    // STZ zpage,X
    Disassembled[0x74] =Instruction("STZ",Instruction::ZPage_X,3);
    Instructions[0x74]->AddStep(new Cat2<ImmediateUnit,AddXUnitZero>(this));        // address->op,op+X->op
    Instructions[0x74]->AddStep(new Cat2<ZeroUnit,ZPageIndirectWriterUnit<Adr> >(this));// op->ea,A->(ea)
    Instructions[0x74]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0x74] =Instruction("NOPE",Instruction::ZPage_X,4);
    Instructions[0x74]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x74]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0x74]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // (op)->op
    Instructions[0x74]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x75: ADC zpage,X 4 cycles
  Disassembled[0x75] =Instruction("ADC",Instruction::ZPage_X,4);
  Instructions[0x75]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x75]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  if (Emulate65C02) {
    Instructions[0x75]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,ADCUnitFixed>(this));// (op)->op,op+A->A
  } else {
    Instructions[0x75]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,ADCUnit>(this));  // (op)->op,op+A->A
  }
  Instructions[0x75]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x76: ROR zpage,X 6 cycles
  Disassembled[0x76] =Instruction("ROR",Instruction::ZPage_X,6);
  Instructions[0x76]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x76]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x76]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
  Instructions[0x76]->AddStep(new Cat1<RORUnit>(this));                         // op>>1->op
  Instructions[0x76]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->(ea)
  Instructions[0x76]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
   // 0x77: RMB7 zpage, 5 cycles
    Disassembled[0x77] =Instruction("RMB7",Instruction::ZPage,5);
    Instructions[0x77]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x77]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x77]->AddStep(new Cat1<RMBUnit<0x80> >(this));                // reset bit #6
    Instructions[0x77]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->ea
    Instructions[0x77]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x77: RRA zpage,X 6 cycles
    Disassembled[0x77] =Instruction("RRAD",Instruction::ZPage_X,6);
    Instructions[0x77]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x77]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0x77]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x77]->AddStep(new Cat1<RORUnit>(this));                       // op>>1->op
    Instructions[0x77]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,ADCUnit>(this,Emulate65C02));// op->(ea),op+A->A
    Instructions[0x77]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x78: SEI, 2 cycles
  Disassembled[0x78] =Instruction("SEI",Instruction::NoArgs,2);
  Instructions[0x78]->AddStep(new Cat1<OrToStatusUnit<I_Mask> >(this));         // P & I -> P
  Instructions[0x78]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x79: ADC absolute,Y 4* cycles
  Disassembled[0x79] =Instruction("ADC",Instruction::Absolute_Y,4);
  Instructions[0x79]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x79]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op, op+Y->op
  if (Emulate65C02) {
    Instructions[0x79]->AddStep(new Cat2<IndirectionUnit<Adr> ,ADCUnitFixed>(this));  // op+A->A
  } else {
    Instructions[0x79]->AddStep(new Cat2<IndirectionUnit<Adr> ,ADCUnit>(this));       // op+A->A
  }
  Instructions[0x79]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // PLY, 4 cycles
    Disassembled[0x7a] =Instruction("PLY",Instruction::NoArgs,4);
    Instructions[0x7a]->AddStep(new Cat1<PullUnit<Adr> >(this));                // stack->op
    Instructions[0x7a]->AddStep(new Cat1<LDYUnit>(this));                       // Y->op
    Instructions[0x7a]->AddStep(new Cat1<WaitUnit>(this));                      // used for whatever
    Instructions[0x7a]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x7a: NOP, 2 cycles
    Disassembled[0x7a] =Instruction("NOPE",Instruction::NoArgs,2);
    Instructions[0x7a]->AddStep(new Cat1<WaitUnit>(this));                      // delay a cycle
    Instructions[0x7a]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x7b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x7b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x7b: RRA absolute,Y 7 cycles
    Disassembled[0x7b] =Instruction("RRAD",Instruction::NoArgs,7);
    Instructions[0x7b]->AddStep(new Cat1<ImmediateUnit>(this));                 // lo->op
    Instructions[0x7b]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this)); // hi->op, op+Y->op
    Instructions[0x7b]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x7b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
    Instructions[0x7b]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ADCUnit>(this,Emulate65C02));    // op+A->A
    Instructions[0x7b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // JMP (indirect,X) 6 cycles
    Disassembled[0x7c] =Instruction("JMP",Instruction::AbsIndirect_X,6);
    Instructions[0x7c]->AddStep(new Cat1<ImmediateUnit>(this));                 // lo->op
    Instructions[0x7c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));    // hi->op
    Instructions[0x7c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));         // op->ea,(op)->lo
    Instructions[0x7c]->AddStep(new Cat3<IndirectionUnitExtendFixed,AddXUnitWait,JMPUnit<0> >(this)); // (ea+1)->hi
    Instructions[0x7c]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x7c: NOP absolute,X 4* cycles 
    Disassembled[0x7c] =Instruction("NOPE",Instruction::Absolute_X,4);
    Instructions[0x7c]->AddStep(new Cat1<ImmediateUnit>(this));                 // lo->op
    Instructions[0x7c]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));        // hi->op, op+X->op
    Instructions[0x7c]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x7c]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x7d: ADC absolute,X 4* cycles
  Disassembled[0x7d] =Instruction("ADC",Instruction::Absolute_X,4);
  Instructions[0x7d]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0x7d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
  if (Emulate65C02) {
    Instructions[0x7d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ADCUnitFixed>(this));  // op+A->A
  } else {
    Instructions[0x7d]->AddStep(new Cat2<IndirectionUnit<Adr> ,ADCUnit>(this));       // op+A->A
  }
  Instructions[0x7d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x7e: ROR absolute,X 7 cycles
  Disassembled[0x7e] =Instruction("ROR",Instruction::Absolute_X,7);
  Instructions[0x7e]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  if (Emulate65C02)
    Instructions[0x7e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));     // hi->op, op+X->op
  else
    Instructions[0x7e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
  Instructions[0x7e]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0x7e]->AddStep(new Cat1<RORUnit>(this));                       // op>>1->op
  else
    Instructions[0x7e]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
  Instructions[0x7e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));              // op->(ea)
  Instructions[0x7e]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
     // 0x7f: BBR7 zpage,disp, 5 cycles
    Disassembled[0x7f] =Instruction("BBR7",Instruction::ZPage_Disp,5);
    Instructions[0x7f]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x7f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x7f]->AddStep(new Cat1<BranchBitTestUnit<0x80,0x00> >(this)); // branch if bit #7 is 0  
    Instructions[0x7f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x7f: RRA absolute,X 7 cycles
    Disassembled[0x7f] =Instruction("RRAD",Instruction::Absolute_X,7);
    Instructions[0x7f]->AddStep(new Cat1<ImmediateUnit>(this));                 // lo->op
    Instructions[0x7f]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
    Instructions[0x7f]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));               // op->ea,(op)->op
    Instructions[0x7f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,RORUnit>(this,Emulate65C02));    // op>>1->op
    Instructions[0x7f]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,ADCUnit>(this,Emulate65C02));    // op->(ea),op+A->A
    Instructions[0x7f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions80
// Build instruction opcodes 0x80 to 0x8f
template<class Adr>
void CPU::BuildInstructions80(void)
{
  //  
  // 0x80: NOP immed, 2 cycles 
  if (Emulate65C02) {
    // BRA displacement
    Disassembled[0x80] =Instruction("BRA",Instruction::Disp,2);
    Instructions[0x80]->AddStep(new Cat1<BranchDetectUnit<0,0> >(this));        // branch always
    Instructions[0x80]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0x80] =Instruction("NOPE",Instruction::Immediate,2);
    Instructions[0x80]->AddStep(new Cat1<ImmediateUnit>(this));                 // fetch operand
    Instructions[0x80]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x81: STA (zpage,X) 6 cycles
  Disassembled[0x81] =Instruction("STA",Instruction::Indirect_X,6);
  Instructions[0x81]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0x81]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0x81]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
  Instructions[0x81]->AddStep(new Cat1<AccuUnit>(this));                        // op->ea,A->op
  Instructions[0x81]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));        // op->(ea)
  Instructions[0x81]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x82 NOP immed, 2 cycles
  Disassembled[0x82] =Instruction("NOPE",Instruction::Immediate,2);
  Instructions[0x82]->AddStep(new Cat1<ImmediateUnit>(this));                   // fetch operand
  Instructions[0x82]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x83] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x83]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x83: SAX (zpage,X) 6 cycles
    Disassembled[0x83] =Instruction("ANSX",Instruction::Indirect_X,6);
    Instructions[0x83]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x83]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0x83]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));// (op)->op
    Instructions[0x83]->AddStep(new Cat1<ANXUnit>(this));                       // op->ea,A&X->op
    Instructions[0x83]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));      // op->(ea)
    Instructions[0x83]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x84: STY zpage 3 cycles
  Disassembled[0x84] =Instruction("STY",Instruction::ZPage,3);
  Instructions[0x84]->AddStep(new Cat2<ImmediateUnit,YUnit>(this));             // address->op,op->ea,Y->op
  Instructions[0x84]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->(ea)
  Instructions[0x84]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x85: STA zpage 3 cycles
  Disassembled[0x85] =Instruction("STA",Instruction::ZPage,3);
  Instructions[0x85]->AddStep(new Cat2<ImmediateUnit,AccuUnit>(this));          // address->op,op->ea,A->op
  Instructions[0x85]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->(ea)
  Instructions[0x85]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x86: STX zpage 3 cycles
  Disassembled[0x86] =Instruction("STX",Instruction::ZPage,3);
  Instructions[0x86]->AddStep(new Cat2<ImmediateUnit,XUnit>(this));             // address->op,op->ea,X->op
  Instructions[0x86]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));   // op->(ea)
  Instructions[0x86]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0x87: SMB0 zpage, 5 cycles
    Disassembled[0x87] =Instruction("SMB0",Instruction::ZPage,5);
    Instructions[0x87]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0x87]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0x87]->AddStep(new Cat1<SMBUnit<0x01> >(this));                // set bit #0
    Instructions[0x87]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->ea
    Instructions[0x87]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x87: SAX zpage 3 cycles
    Disassembled[0x87] =Instruction("ANSX",Instruction::ZPage,3);
    Instructions[0x87]->AddStep(new Cat2<ImmediateUnit,ANXUnit>(this));         // address->op,op->ea,A&X->op
    Instructions[0x87]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02)); // op->(ea)
    Instructions[0x87]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x88: DEY 2 cycles
  Disassembled[0x88] =Instruction("DEY",Instruction::NoArgs,2);
  Instructions[0x88]->AddStep(new Cat3<YUnit,DECUnit,LDYUnit>(this));           // Y->op,op--,op->Y
  Instructions[0x88]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x89: NOP immed 2 cycles
  Disassembled[0x89] =Instruction("NOPE",Instruction::Immediate,2);
  Instructions[0x89]->AddStep(new Cat1<ImmediateUnit>(this));                   // fetch operand
  Instructions[0x89]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x8a: TXA 2 cycles
  Disassembled[0x8a] =Instruction("TXA",Instruction::NoArgs,2);
  Instructions[0x8a]->AddStep(new Cat2<XUnit,LDAUnit>(this));                   // X->op,op->A
  Instructions[0x8a]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x8b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x8b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x8b: this is an unstable opcode
    Disassembled[0x8b] =Instruction("UNST",Instruction::NoArgs,0);
    Instructions[0x8b]->AddStep(new Cat1<UnstableUnit<0x8b> >(this));           // just crash
    Instructions[0x8b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x8c: STY absolute, 4 cycles
  Disassembled[0x8c] =Instruction("STY",Instruction::Absolute,4);
  Instructions[0x8c]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
  Instructions[0x8c]->AddStep(new Cat2<ImmediateWordExtensionUnit,YUnit>(this)); // hi->op,op->ea,Y->op
  Instructions[0x8c]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));               // op->(ea)
  Instructions[0x8c]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x8d: STA absolute, 4 cycles
  Disassembled[0x8d] =Instruction("STA",Instruction::Absolute,4);
  Instructions[0x8d]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
  Instructions[0x8d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AccuUnit>(this)); // hi->op,op->ea,A->op
  Instructions[0x8d]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));               // op->(ea)
  Instructions[0x8d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x8e: STX absolute, 4 cycles
  Disassembled[0x8e] =Instruction("STX",Instruction::Absolute,4);
  Instructions[0x8e]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
  Instructions[0x8e]->AddStep(new Cat2<ImmediateWordExtensionUnit,XUnit>(this)); // hi->op,op->ea,X->op
  Instructions[0x8e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));               // op->(ea)
  Instructions[0x8e]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
     // 0x8f: BBS0 zpage,disp, 5 cycles
    Disassembled[0x8f] =Instruction("BBS0",Instruction::ZPage_Disp,5);
    Instructions[0x8f]->AddStep(new Cat1<ImmediateUnit>(this));                  // ZPage address->op
    Instructions[0x8f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));     // op->ea,(op)->op
    Instructions[0x8f]->AddStep(new Cat1<BranchBitTestUnit<0x01,0x01> >(this));  // branch if bit #0 is 1
    Instructions[0x8f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x8f: SAX absolute, 4 cycles
    Disassembled[0x8f] =Instruction("ANSX",Instruction::Absolute,4);
    Instructions[0x8f]->AddStep(new Cat1<ImmediateUnit>(this));                  // lo->op
    Instructions[0x8f]->AddStep(new Cat2<ImmediateWordExtensionUnit,ANXUnit>(this)); // hi->op,op->ea,A&X->op
    Instructions[0x8f]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));             // op->(ea)
    Instructions[0x8f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructions90
// Build instruction opcodes 0x90 to 0x9f
template<class Adr>
void CPU::BuildInstructions90(void)
{
  //
  // 0x90: BCC, 2 cycles
  Disassembled[0x90] =Instruction("BCC",Instruction::Disp,2);
  Instructions[0x90]->AddStep(new Cat1<BranchDetectUnit<C_Mask,0> >(this));      // branch if C flag clear
  Instructions[0x90]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x91: STA (indirect),Y 6 cycles
  Disassembled[0x91] =Instruction("STA",Instruction::Indirect_Y,5);
  Instructions[0x91]->AddStep(new Cat1<ImmediateUnit>(this));                    // ZPage address->op
  Instructions[0x91]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this)); // (op)->op,op+Y->op
  Instructions[0x91]->AddStep(new Cat1<AccuUnit>(this));                         // op->ea,A->op
  Instructions[0x91]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));         // op->(ea)
  Instructions[0x91]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x92: JAM
  if (Emulate65C02) {
    // STA (zpage)
    Disassembled[0x92] =Instruction("STA",Instruction::Indirect_Z,5);
    Instructions[0x92]->AddStep(new Cat1<ImmediateUnit>(this));                  // ZPage address->op
    Instructions[0x92]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this)); // (op)->op
    Instructions[0x92]->AddStep(new Cat1<AccuUnit>(this));                       // op->ea,A->op
    Instructions[0x92]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this));       // op->(ea)
    Instructions[0x92]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0x92] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0x92]->AddStep(new Cat1<JAMUnit<0x92> >(this));                 // just crash
    Instructions[0x92]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0x93] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x93]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x93: does not work reliable. Maybe later
    Disassembled[0x93] =Instruction("UNST",Instruction::NoArgs,0);
    Instructions[0x93]->AddStep(new Cat1<UnstableUnit<0x93> >(this));            // just crash
    Instructions[0x93]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x94: STY zpage,X 4 cycles
  Disassembled[0x94] =Instruction("STY",Instruction::ZPage_X,4);
  Instructions[0x94]->AddStep(new Cat1<ImmediateUnit>(this));                    // address->op
  Instructions[0x94]->AddStep(new Cat1<AddXUnitZero>(this));                     // op+X->op
  Instructions[0x94]->AddStep(new Cat2<YUnit,ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02)); // op->ea,Y->(ea)
  Instructions[0x94]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x95: STA zpage,X 4 cycles
  Disassembled[0x95] =Instruction("STA",Instruction::ZPage_X,4);
  Instructions[0x95]->AddStep(new Cat1<ImmediateUnit>(this));                    // address->op
  Instructions[0x95]->AddStep(new Cat1<AddXUnitZero>(this));                     // op+X->op
  Instructions[0x95]->AddStep(new Cat2<AccuUnit,ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02)); // op->ea,A->(ea)
  Instructions[0x95]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x96: STX zpage,Y 4 cycles
  Disassembled[0x96] =Instruction("STX",Instruction::ZPage_Y,4);
  Instructions[0x96]->AddStep(new Cat1<ImmediateUnit>(this));                    // address->op
  Instructions[0x96]->AddStep(new Cat1<AddYUnitZero>(this));                     // op+Y->op
  Instructions[0x96]->AddStep(new Cat2<XUnit,ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));    // op->ea,X->(ea)
  Instructions[0x96]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0x97: SMB1 zpage, 5 cycles
    Disassembled[0x97] =Instruction("SMB1",Instruction::ZPage,5);
    Instructions[0x97]->AddStep(new Cat1<ImmediateUnit>(this));                  // ZPage address->op
    Instructions[0x97]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));     // op->ea,(op)->op
    Instructions[0x97]->AddStep(new Cat1<SMBUnit<0x02> >(this));                 // set bit #1
    Instructions[0x97]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));  // op->ea
    Instructions[0x97]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x97: SAX zpage,Y
    Disassembled[0x97] =Instruction("ANSX",Instruction::ZPage_Y,3);
    Instructions[0x97]->AddStep(new Cat1<ImmediateUnit>(this));                  // address->op
    Instructions[0x97]->AddStep(new Cat1<AddYUnitZero>(this));                   // op+Y->op
    Instructions[0x97]->AddStep(new Cat2<ANXUnit,ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));// op->ea,A&X->(ea)
    Instructions[0x97]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x98: TYA 2 cycles
  Disassembled[0x98] =Instruction("TYA",Instruction::NoArgs,2);
  Instructions[0x98]->AddStep(new Cat2<YUnit,LDAUnit>(this));                    // Y->op,op->A
  Instructions[0x98]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x99: STA absolute,Y 5 cycles
  Disassembled[0x99] =Instruction("STA",Instruction::Absolute_Y,5);
  Instructions[0x99]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
  Instructions[0x99]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this)); // op->hi,op+Y->op
  Instructions[0x99]->AddStep(new Cat2<AccuUnit,IndirectWriterUnit<Adr> >(this,Emulate65C02));      // op->ea,A->op,op->(ea)
  Instructions[0x99]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x9a: TXS, 2 cycles
  Disassembled[0x9a] =Instruction("TXS",Instruction::NoArgs,2);
  Instructions[0x9a]->AddStep(new Cat2<XUnit,SetStackUnit>(this));               // X->op,op->S
  Instructions[0x9a]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0x9b] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0x9b]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x9b: Unreliable (TAS)
    Disassembled[0x9b] =Instruction("UNST",Instruction::NoArgs,0);
    Instructions[0x9b]->AddStep(new Cat1<UnstableUnit<0x9b> >(this));            // Just crash
    Instructions[0x9b]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x9c: Unreliable (SHY)
  if (Emulate65C02) {
    // STZ absolute
    Disassembled[0x9c] =Instruction("STZ",Instruction::Absolute,4);
    Instructions[0x9c]->AddStep(new Cat1<ImmediateUnit>(this));                  // lo->op
    Instructions[0x9c]->AddStep(new Cat2<ImmediateWordExtensionUnit,ZeroUnit>(this)); // hi->op,op->ea,A->op
    Instructions[0x9c]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this));             // op->(ea)
    Instructions[0x9c]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x9c: SAY,SHY, STADY: 5 Cycles, absolute,X
    Disassembled[0x9c] =Instruction("STADY",Instruction::Absolute_X,5);
    Instructions[0x9c]->AddStep(new Cat1<ImmediateUnit>(this));                // lo->op
    Instructions[0x9c]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));   // op->hi,op+X->op
    Instructions[0x9c]->AddStep(new Cat1<AndHiPlusOneYAddXUnit>(this));
    Instructions[0x9c]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));
    Instructions[0x9c]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0x9d: STA absolute,X 5 cycles
  Disassembled[0x9d] =Instruction("STA",Instruction::Absolute_X,5);
  Instructions[0x9d]->AddStep(new Cat1<ImmediateUnit>(this));                    // lo->op
  if (Emulate65C02) {
    Instructions[0x9d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this));     // op->hi,op+X->op
  } else {
    Instructions[0x9d]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // op->hi,op+X->op
  }
  Instructions[0x9d]->AddStep(new Cat2<AccuUnit,IndirectWriterUnit<Adr> >(this,Emulate65C02));      // op->ea,A->op,op->(ea)
  Instructions[0x9d]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0x9e: Unreliable (SHX)
  if (Emulate65C02) {
    // STZ absolute,X
    Disassembled[0x9e] =Instruction("STZ",Instruction::Absolute_X,5);
    Instructions[0x9e]->AddStep(new Cat1<ImmediateUnit>(this));                  // lo->op
    Instructions[0x9e]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // op->hi,op+X->op
    Instructions[0x9e]->AddStep(new Cat2<ZeroUnit,IndirectWriterUnit<Adr> >(this));    // op->ea,Z->op,op->(ea)
    Instructions[0x9e]->AddStep(new Cat1<DecodeUnit>(this));
  } else { 
    // 0x9e: XAS,SHX, STADX: 5 Cycles, absolute,Y
    Disassembled[0x9e] =Instruction("STADX",Instruction::Absolute_Y,5);
    Instructions[0x9e]->AddStep(new Cat1<ImmediateUnit>(this));                // lo->op
    Instructions[0x9e]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));   // op->hi,op+X->op
    Instructions[0x9e]->AddStep(new Cat1<AndHiPlusOneXAddYUnit>(this));
    Instructions[0x9e]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));
    Instructions[0x9e]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // 0x9f: BBS1 zpage,disp, 5 cycles
    Disassembled[0x9f] =Instruction("BBS1",Instruction::ZPage_Disp,5);
    Instructions[0x9f]->AddStep(new Cat1<ImmediateUnit>(this));                  // ZPage address->op
    Instructions[0x9f]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));     // op->ea,(op)->op
    Instructions[0x9f]->AddStep(new Cat1<BranchBitTestUnit<0x02,0x02> >(this));  // branch if bit #1 is 1
    Instructions[0x9f]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0x9f: Unreliable (AHX)
    Disassembled[0x9f] =Instruction("UNST",Instruction::NoArgs,0);
    Instructions[0x9f]->AddStep(new Cat1<UnstableUnit<0x9f> >(this));
    Instructions[0x9f]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsA0
// Build instruction opcodes 0xA0 to 0xAf
template<class Adr>
void CPU::BuildInstructionsA0(void)
{  
  //
  // 0xa0: LDY immed, 2 cycles
  Disassembled[0xa0] =Instruction("LDY",Instruction::Immediate,2);
  Instructions[0xa0]->AddStep(new Cat2<ImmediateUnit,LDYUnit>(this));           // operand->op,op->Y
  Instructions[0xa0]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xa1: LDA (indirect,X) 6 cycles
  Disassembled[0xa1] =Instruction("LDA",Instruction::Indirect_X,6);
  Instructions[0xa1]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
  Instructions[0xa1]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
  Instructions[0xa1]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
  Instructions[0xa1]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
  Instructions[0xa1]->AddStep(new Cat1<LDAUnit>(this));                         // op  ->A
  Instructions[0xa1]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xa2: LDX immed, 2 cycles
  Disassembled[0xa2] =Instruction("LDX",Instruction::Immediate,2);
  Instructions[0xa2]->AddStep(new Cat2<ImmediateUnit,LDXUnit>(this));           // operand->op,op->X
  Instructions[0xa2]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0xa3] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xa3]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xa3: LAX (indirect,X) 6 cycles
    Disassembled[0xa3] =Instruction("LDAX",Instruction::Indirect_X,6);
    Instructions[0xa3]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0xa3]->AddStep(new Cat1<AddXUnitZero>(this));                  // op+X->op
    Instructions[0xa3]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));// (op)->op
    Instructions[0xa3]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));         // (op)->op
    Instructions[0xa3]->AddStep(new Cat2<LDAUnit,LDXUnit>(this));               // op  ->A,op->X
    Instructions[0xa3]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xa4: LDY zpage, 3 cycles
  Disassembled[0xa4] =Instruction("LDY",Instruction::ZPage,3);
  Instructions[0xa4]->AddStep(new Cat1<ImmediateUnit>(this));                   // address->op
  Instructions[0xa4]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,LDYUnit>(this));// (op)->op,op->Y
  Instructions[0xa4]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xa5: LDA zpage, 3 cycles
  Disassembled[0xa5] =Instruction("LDA",Instruction::ZPage,3);
  Instructions[0xa5]->AddStep(new Cat1<ImmediateUnit>(this));                   // address->op
  Instructions[0xa5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,LDAUnit>(this));// (op)->op,op->A
  Instructions[0xa5]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xa6: LDX zpage, 3 cycles
  Disassembled[0xa6] =Instruction("LDX",Instruction::ZPage,3);
  Instructions[0xa6]->AddStep(new Cat1<ImmediateUnit>(this));                   // address->op
  Instructions[0xa6]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,LDXUnit>(this));// (op)->op,op->X
  Instructions[0xa6]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xa7: SMB2 zpage, 5 cycles
    Disassembled[0xa7] =Instruction("SMB2",Instruction::ZPage,5);
    Instructions[0xa7]->AddStep(new Cat1<ImmediateUnit>(this));                 // ZPage address->op
    Instructions[0xa7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));    // op->ea,(op)->op
    Instructions[0xa7]->AddStep(new Cat1<SMBUnit<0x04> >(this));                // set bit #2
    Instructions[0xa7]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this)); // op->ea
    Instructions[0xa7]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xa7: LAX zpage, 3 cycles
    Disassembled[0xa7] =Instruction("LDAX",Instruction::ZPage,3);
    Instructions[0xa7]->AddStep(new Cat1<ImmediateUnit>(this));                 // address->op
    Instructions[0xa7]->AddStep(new Cat3<ZPageIndirectionUnit<Adr>,LDAUnit,LDXUnit>(this)); // (op)->op,op->A,op->X
    Instructions[0xa7]->AddStep(new Cat1<DecodeUnit>(this));
    Instructions[0xa7]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xa8: TAY, 2 cycles
  Disassembled[0xa8] =Instruction("TAY",Instruction::NoArgs,2);
  Instructions[0xa8]->AddStep(new Cat2<AccuUnit,LDYUnit>(this));                // A->op,op->Y
  Instructions[0xa8]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xa9: LDA #immed, 2 cycles
  Disassembled[0xa9] =Instruction("LDA",Instruction::Immediate,2);
  Instructions[0xa9]->AddStep(new Cat2<ImmediateUnit,LDAUnit>(this));           // immed->op,op->A
  Instructions[0xa9]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xaa: TAX, 2 cycles
  Disassembled[0xaa] =Instruction("TAX",Instruction::NoArgs,2);
  Instructions[0xaa]->AddStep(new Cat2<AccuUnit,LDXUnit>(this));                // A->op,op->X
  Instructions[0xaa]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0xab] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xab]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xab: LAX #immed, 2 cycles
    Disassembled[0xab] =Instruction("LDAX",Instruction::Immediate,2);
    Instructions[0xab]->AddStep(new Cat3<ImmediateUnit,LDAUnit,LDXUnit>(this)); // immed->op,op->A
    Instructions[0xab]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xac: LDY absolute 4 cycles
  Disassembled[0xac] =Instruction("LDY",Instruction::Absolute,4);
  Instructions[0xac]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0xac]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0xac]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDYUnit>(this));         // (op)->op,op->Y
  Instructions[0xac]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xad: LDA absolute 4 cycles
  Disassembled[0xad] =Instruction("LDA",Instruction::Absolute,4);
  Instructions[0xad]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0xad]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0xad]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDAUnit>(this));         // (op)->op,op->A
  Instructions[0xad]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xae: LDX absolute 4 cycles
  Disassembled[0xae] =Instruction("LDX",Instruction::Absolute,4);
  Instructions[0xae]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
  Instructions[0xae]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
  Instructions[0xae]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDXUnit>(this));         // (op)->op,op->X
  Instructions[0xae]->AddStep(new Cat1<DecodeUnit>(this));
  //  
  if (Emulate65C02) {
    // 0xaf: BBS2 zpage,disp, 5 cycles
    Disassembled[0xaf] =Instruction("BBS2",Instruction::ZPage_Disp,5);
    Instructions[0xaf]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xaf]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xaf]->AddStep(new Cat1<BranchBitTestUnit<0x04,0x04> >(this));   // branch if bit #2 is 1
    Instructions[0xaf]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xaf: LAX absolute 4 cycles
    Disassembled[0xaf] =Instruction("LDAX",Instruction::Absolute,4);
    Instructions[0xaf]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xaf]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
    Instructions[0xaf]->AddStep(new Cat3<IndirectionUnit<Adr> ,LDAUnit,LDXUnit>(this)); // (op)->op,op->X
    Instructions[0xaf]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsB0
// Build instruction opcodes 0xB0 to 0xBf
template<class Adr>
void CPU::BuildInstructionsB0(void)
{
  //
  // 0xb0: BCS, 2 cycles
  Disassembled[0xb0] =Instruction("BCS",Instruction::Disp,2);
  Instructions[0xb0]->AddStep(new Cat1<BranchDetectUnit<C_Mask,C_Mask> >(this));  // branch if C flag clear
  Instructions[0xb0]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xb1: LDA (indirect),Y 5* cycles
  Disassembled[0xb1] =Instruction("LDA",Instruction::Indirect_Y,5);
  Instructions[0xb1]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xb1]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));    // (op)->op
  Instructions[0xb1]->AddStep(new Cat2<AddYUnit,IndirectionUnit<Adr> >(this));    // op+Y->op,(op)->op
  Instructions[0xb1]->AddStep(new Cat1<LDAUnit>(this));                           // op->ea,op->A
  Instructions[0xb1]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xb2: JAM
  if (Emulate65C02) { 
    // LDA (zpage)
    Disassembled[0xb2] =Instruction("LDA",Instruction::Indirect_Z,5);
    Instructions[0xb2]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xb2]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0xb2]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // (op)->op
    Instructions[0xb2]->AddStep(new Cat1<LDAUnit>(this));                         // op->ea,op->A
    Instructions[0xb2]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0xb2] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0xb2]->AddStep(new Cat1<JAMUnit<0xb2> >(this));                  // just crash
    Instructions[0xb2]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0xb3] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xb3]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xb3: LAX (indirect),Y 5* cycles 
    Disassembled[0xb3] =Instruction("LDAX",Instruction::Indirect_Y,5);
    Instructions[0xb3]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xb3]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0xb3]->AddStep(new Cat2<AddYUnit,IndirectionUnit<Adr> >(this));  // op+Y->op,(op)->op
    Instructions[0xb3]->AddStep(new Cat2<LDAUnit,LDXUnit>(this));                 // op->ea,op->A,op->X
    Instructions[0xb3]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xb4: LDY zpage,X 4 cycles
  Disassembled[0xb4] =Instruction("LDY",Instruction::ZPage_X,4);
  Instructions[0xb4]->AddStep(new Cat1<ImmediateUnit>(this));                     // address->op
  Instructions[0xb4]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xb4]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,LDYUnit>(this)); // (op)->op,op->Y
  Instructions[0xb4]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xb5: LDA zpage,X 4 cycles
  Disassembled[0xb5] =Instruction("LDA",Instruction::ZPage_X,4);
  Instructions[0xb5]->AddStep(new Cat1<ImmediateUnit>(this));                     // address->op
  Instructions[0xb5]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xb5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,LDAUnit>(this)); // (op)->op,op->A
  Instructions[0xb5]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xb6: LDX zpage,Y 4 cycles
  Disassembled[0xb6] =Instruction("LDX",Instruction::ZPage_Y,4);
  Instructions[0xb6]->AddStep(new Cat1<ImmediateUnit>(this));                     // address->op
  Instructions[0xb6]->AddStep(new Cat1<AddYUnitZero>(this));                      // op+Y->op
  Instructions[0xb6]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,LDXUnit>(this)); // (op)->op,op->X
  Instructions[0xb6]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xb7: SMB3 zpage, 5 cycles
    Disassembled[0xb7] =Instruction("SMB3",Instruction::ZPage,5);
    Instructions[0xb7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xb7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xb7]->AddStep(new Cat1<SMBUnit<0x08> >(this));                  // set bit #3
    Instructions[0xb7]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0xb7]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xb7: LAX zpage,Y 4 cycles
    Disassembled[0xb7] =Instruction("LDAX",Instruction::ZPage_Y,4);
    Instructions[0xb7]->AddStep(new Cat1<ImmediateUnit>(this));                           // address->op
    Instructions[0xb7]->AddStep(new Cat1<AddYUnitZero>(this));                            // op+Y->op
    Instructions[0xb7]->AddStep(new Cat3<ZPageIndirectionUnit<Adr>,LDAUnit,LDXUnit>(this));// (op)->op,op->A,op->X
    Instructions[0xb7]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xb8: CLV 2 cycles
  Disassembled[0xb8] =Instruction("CLV",Instruction::NoArgs,2);
  Instructions[0xb8]->AddStep(new Cat1<AndToStatusUnit<NOT(V_Mask)> >(this));     // P & ~V_Mask->P
  Instructions[0xb8]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xb9: LDA abs,Y 4* cycles
  Disassembled[0xb9] =Instruction("LDA",Instruction::Absolute_Y,4);
  Instructions[0xb9]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xb9]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op,op+Y->op
  Instructions[0xb9]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDAUnit>(this));           // (op)->op,op->A
  Instructions[0xb9]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xba: TSX 2 cycles
  Disassembled[0xba] =Instruction("TSX",Instruction::NoArgs,2);
  Instructions[0xba]->AddStep(new Cat2<GetStackUnit,LDXUnit>(this));              // X->op,op->S
  Instructions[0xba]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0xbb] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xbb]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xbb: Unreliable (LAS)
    Disassembled[0xbb] =Instruction("UNST",Instruction::NoArgs,0);
    Instructions[0xbb]->AddStep(new Cat1<UnstableUnit<0xbb> >(this));             // just crash
    Instructions[0xbb]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xbc: LDY absolute,X 4* cycles
  Disassembled[0xbc] =Instruction("LDY",Instruction::Absolute_X,4);
  Instructions[0xbc]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xbc]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op,op+X->op
  Instructions[0xbc]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDYUnit>(this));           // (op)->op,op->Y
  Instructions[0xbc]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xbd: LDA absolute,X 4* cycles
  Disassembled[0xbd] =Instruction("LDA",Instruction::Absolute_X,4);
  Instructions[0xbd]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xbd]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op,op+X->op
  Instructions[0xbd]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDAUnit>(this));           // (op)->op,op->A
  Instructions[0xbd]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xbe: LDX absolute,Y 4* cycles
  Disassembled[0xbe] =Instruction("LDX",Instruction::Absolute_Y,4);
  Instructions[0xbe]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xbe]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op,op+Y->op
  Instructions[0xbe]->AddStep(new Cat2<IndirectionUnit<Adr> ,LDXUnit>(this));           // (op)->op,op->X
  Instructions[0xbe]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0xbf: BBS3 zpage,disp, 5 cycles
    Disassembled[0xbf] =Instruction("BBS3",Instruction::ZPage_Disp,5);
    Instructions[0xbf]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xbf]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xbf]->AddStep(new Cat1<BranchBitTestUnit<0x08,0x08> >(this));   // branch if bit #3 is 1
    Instructions[0xbf]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xbf: LAX absolute,Y 4* cycles
    Disassembled[0xbf] =Instruction("LDAX",Instruction::Absolute_Y,4);
    Instructions[0xbf]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xbf]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op,op+Y->op
    Instructions[0xbf]->AddStep(new Cat3<IndirectionUnit<Adr> ,LDAUnit,LDXUnit>(this)); // (op)->op,op->A,op->X
    Instructions[0xbf]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsC0
// Build instruction opcodes 0xC0 to 0xCf
template<class Adr>
void CPU::BuildInstructionsC0(void)
{
  //
  // 0xc0: CPY immediate, 2 cycles
  Disassembled[0xc0] =Instruction("CPY",Instruction::Immediate,2);
  Instructions[0xc0]->AddStep(new Cat2<ImmediateUnit,CPYUnit>(this));             // data->op,Y-op->flags
  Instructions[0xc0]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xc1: CMP (addr,X): 6 cycles
  Disassembled[0xc1] =Instruction("CMP",Instruction::Indirect_X,6);
  Instructions[0xc1]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xc1]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xc1]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));    // (op)->op
  Instructions[0xc1]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // (op)->op
  Instructions[0xc1]->AddStep(new Cat1<CMPUnit>(this));                           // A-op->flags
  Instructions[0xc1]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xc2: NOP immed, 2 cycles
  Disassembled[0xc2] =Instruction("NOPE",Instruction::Immediate,2);
  Instructions[0xc2]->AddStep(new Cat1<ImmediateUnit>(this));                     // data->op
  Instructions[0xc2]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // NOP
    Disassembled[0xc3] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xc3]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xc3: DCM (addr,X): 8 cycles
    Disassembled[0xc3] =Instruction("DECP",Instruction::Indirect_X,8);
    Instructions[0xc3]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xc3]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0xc3]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0xc3]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0xc3]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));// --op->op
    Instructions[0xc3]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));        // op->ea
    Instructions[0xc3]->AddStep(new Cat1<CMPUnit>(this));                         // A-op->flags
    Instructions[0xc3]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0xc4: CPY zpage, 3 cycles
  Disassembled[0xc4] =Instruction("CPY",Instruction::ZPage,3);
  Instructions[0xc4]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xc4]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,CPYUnit>(this)); // (op)->op,Y-op->flags
  Instructions[0xc4]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0xc5: CMP zpage, 3 cycles
  Disassembled[0xc5] =Instruction("CMP",Instruction::ZPage,3);
  Instructions[0xc5]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xc5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,CMPUnit>(this)); // (op)->op,A-op->A
  Instructions[0xc5]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xc6: DEC zpage, 5 cycles
  Disassembled[0xc6] =Instruction("DEC",Instruction::ZPage,5);
  Instructions[0xc6]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xc6]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0xc6]->AddStep(new Cat1<DECUnit>(this));                           // --op->op
  Instructions[0xc6]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->ea
  Instructions[0xc6]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  if (Emulate65C02) {
    // 0xc7: SMB4 zpage, 5 cycles
    Disassembled[0xc7] =Instruction("SMB4",Instruction::ZPage,5);
    Instructions[0xc7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xc7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xc7]->AddStep(new Cat1<SMBUnit<0x10> >(this));                  // set bit #4
    Instructions[0xc7]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0xc7]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xc7: DCM zpage, 5 cycles
    Disassembled[0xc7] =Instruction("DECP",Instruction::ZPage,5);
    Instructions[0xc7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xc7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xc7]->AddStep(new Cat1<DECUnit>(this));                         // op<<1->op
    Instructions[0xc7]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,CMPUnit>(this,Emulate65C02)); // op->ea,A-op->flags
    Instructions[0xc7]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0xc8: INY 2 cycles
  Disassembled[0xc8] =Instruction("INY",Instruction::NoArgs,2);
  Instructions[0xc8]->AddStep(new Cat3<YUnit,INCUnit,LDYUnit>(this));
  Instructions[0xc8]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0xc9: CMP immediate, 2 cycles
  Disassembled[0xc9] =Instruction("CMP",Instruction::Immediate,2);
  Instructions[0xc9]->AddStep(new Cat2<ImmediateUnit,CMPUnit>(this));             // #->op,A-op->A
  Instructions[0xc9]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0xca: DEX 2 cycles
  Disassembled[0xca] =Instruction("DEX",Instruction::NoArgs,2);
  Instructions[0xca]->AddStep(new Cat3<XUnit,DECUnit,LDXUnit>(this));
  Instructions[0xca]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  if (Emulate65C02) {
    // 0cb: WAI Wait for interrupt
    Disassembled[0xcb] =Instruction("WAI",Instruction::NoArgs,2);
    Instructions[0xcb]->AddStep(new Cat1<HaltUnit>(this));
    Instructions[0xcb]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xcb: A and X Sub -> X 
    Disassembled[0xcb] =Instruction("ANXS",Instruction::Immediate,2);
    Instructions[0xcb]->AddStep(new Cat3<ANXUnit,LDXUnit,ImmediateUnit>(this));
    Instructions[0xcb]->AddStep(new Cat3<CPXUnit,LDXUnit,DecodeUnit>(this));
  }
  //
  // 0xcc: CPY abs, 4 cycles
  Disassembled[0xcc] =Instruction("CPY",Instruction::Absolute,4);
  Instructions[0xcc]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xcc]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0xcc]->AddStep(new Cat2<IndirectionUnit<Adr> ,CPYUnit>(this));           // (op)->op,Y-op->flags
  Instructions[0xcc]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xcd: CMP abs, 4 cycles
  Disassembled[0xcd] =Instruction("CMP",Instruction::Absolute,4);
  Instructions[0xcd]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xcd]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0xcd]->AddStep(new Cat2<IndirectionUnit<Adr> ,CMPUnit>(this));           // (op)->op,A-op->A
  Instructions[0xcd]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xce: DEC abs, 6 cycles
  Disassembled[0xce] =Instruction("DEC",Instruction::Absolute,6);
  Instructions[0xce]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xce]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0xce]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0xce]->AddStep(new Cat1<DECUnit>(this));                         // --op->op
  else
    Instructions[0xce]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));      // --op->op
  Instructions[0xce]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0xce]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xcf: BBS4 zpage,disp, 5 cycles
    Disassembled[0xcf] =Instruction("BBS4",Instruction::ZPage_Disp,5);
    Instructions[0xcf]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xcf]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xcf]->AddStep(new Cat1<BranchBitTestUnit<0x10,0x10> >(this));   // branch if bit #4 is 1
    Instructions[0xcf]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xcf: DCM abs, 6 cycles
    Disassembled[0xcf] =Instruction("DECP",Instruction::Absolute,6);
    Instructions[0xcf]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xcf]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
    Instructions[0xcf]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0xcf]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));      // --op->op
    Instructions[0xcf]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,CMPUnit>(this,Emulate65C02));      // op->(ea)
    Instructions[0xcf]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsD0
// Build instruction opcodes 0xD0 to 0xDf
template<class Adr>
void CPU::BuildInstructionsD0(void)
{
  //
  // 0xd0: BNE abs, 2 cycles
  Disassembled[0xd0] =Instruction("BNE",Instruction::Disp,2);
  Instructions[0xd0]->AddStep(new Cat1<BranchDetectUnit<Z_Mask,0> >(this));       // branch if Z flag cleared
  Instructions[0xd0]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xd1: CMP (indirect),Y 5* cycles
  Disassembled[0xd1] =Instruction("CMP",Instruction::Indirect_Y,5);
  Instructions[0xd1]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xd1]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnit>(this));// (op)->op,op+Y->op
  Instructions[0xd1]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // (op)->op
  Instructions[0xd1]->AddStep(new Cat1<CMPUnit>(this));                           // A-op->flags
  Instructions[0xd1]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  // 0xd2: JAM
  if (Emulate65C02) {  
    // CMP (zpage)
    Disassembled[0xd2] =Instruction("CMP",Instruction::Indirect_Z,5);
    Instructions[0xd2]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xd2]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0xd2]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // (op)->op
    Instructions[0xd2]->AddStep(new Cat1<CMPUnit>(this));                         // A-op->flags
    Instructions[0xd2]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0xd2] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0xd2]->AddStep(new Cat1<JAMUnit<0xd2> >(this));                  // just crash
    Instructions[0xd2]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0xd3] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xd3]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xd3: DCM (indirect),Y 8 cycles
    Disassembled[0xd3] =Instruction("DECP",Instruction::Indirect_Y,8);
    Instructions[0xd3]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xd3]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this));// (op)->op,op+Y->op
    Instructions[0xd3]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0xd3]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));// --op->op
    Instructions[0xd3]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,CMPUnit>(this,Emulate65C02));// op->(ea)
    Instructions[0xd3]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xd4: NOP zpage,X 4 cycles
  Disassembled[0xd4] =Instruction("NOPE",Instruction::ZPage_X,4);
  Instructions[0xd4]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xd4]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xd4]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // (op)->op
  Instructions[0xd4]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xd5: CMP zpage,X 4 cycles
  Disassembled[0xd5] =Instruction("CMP",Instruction::ZPage_X,4);
  Instructions[0xd5]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xd5]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xd5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,CMPUnit>(this)); // (op)->op,A-op->A
  Instructions[0xd5]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xd6: DEC zpage,X 6 cycles
  Disassembled[0xd6] =Instruction("DEC",Instruction::ZPage_X,6);
  Instructions[0xd6]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xd6]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xd6]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0xd6]->AddStep(new Cat1<DECUnit>(this));                           // --op->op
  Instructions[0xd6]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->(ea)
  Instructions[0xd6]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xd7: SMB5 zpage, 5 cycles
    Disassembled[0xd7] =Instruction("SMB5",Instruction::ZPage,5);
    Instructions[0xd7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xd7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xd7]->AddStep(new Cat1<SMBUnit<0x20> >(this));                  // set bit #5
    Instructions[0xd7]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0xd7]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xd7: DCM zpage,X 6 cycles
    Disassembled[0xd7] =Instruction("DECP",Instruction::ZPage_X,6);
    Instructions[0xd7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xd7]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0xd7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xd7]->AddStep(new Cat1<DECUnit>(this));                         // --op->op
    Instructions[0xd7]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,CMPUnit>(this,Emulate65C02)); // op->(ea),A-op->A
    Instructions[0xd7]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xd8: CLD, 2 cycles
  Disassembled[0xd8] =Instruction("CLD",Instruction::NoArgs,2);
  Instructions[0xd8]->AddStep(new Cat1<AndToStatusUnit<NOT(D_Mask)> >(this));     // P & (~D) -> P
  Instructions[0xd8]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xd9: CMP absolute,Y 4* cycles
  Disassembled[0xd9] =Instruction("CMP",Instruction::Absolute_Y,4);
  Instructions[0xd9]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xd9]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op, op+Y->op
  Instructions[0xd9]->AddStep(new Cat2<IndirectionUnit<Adr> ,CMPUnit>(this));           // (op)->op,A-op->flags
  Instructions[0xd9]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // PHX, 3 cycles
    Disassembled[0xda] =Instruction("PHX",Instruction::NoArgs,3);
    Instructions[0xda]->AddStep(new Cat1<XUnit>(this));                           // Y->op
    Instructions[0xda]->AddStep(new Cat1<PushUnit<Adr> >(this));                  // op->stack
    Instructions[0xda]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xda: NOP, 2 cycles  
    Disassembled[0xda] =Instruction("NOPE",Instruction::NoArgs,2);
    Instructions[0xda]->AddStep(new Cat1<WaitUnit>(this));                        // delay a cycle
    Instructions[0xda]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  if (Emulate65C02) {
    // 0xdb: STP stop CPU. Jamming pretty much does the same.
    Disassembled[0xdb] =Instruction("STP",Instruction::NoArgs,3);
    Instructions[0xdb]->AddStep(new Cat1<JAMUnit<0xdb> >(this));                  // just crash
    Instructions[0xdb]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xdb: DCM absolute,Y 7 cycles
    Disassembled[0xdb] =Instruction("DECP",Instruction::Absolute_Y,7);
    Instructions[0xdb]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xdb]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this)); // hi->op, op+Y->op
    Instructions[0xdb]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0xdb]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));      // --op->op
    Instructions[0xdb]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,CMPUnit>(this,Emulate65C02));      // op->(ea),A-op->flags
    Instructions[0xdb]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xdc: NOP absolute,X 4* cycles
  Disassembled[0xdc] =Instruction("NOPE",Instruction::Absolute_X,4);
  Instructions[0xdc]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xdc]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
  Instructions[0xdc]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  Instructions[0xdc]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xdd: CMP absolute,X 4* cycles
  Disassembled[0xdd] =Instruction("CMP",Instruction::Absolute_X,4);
  Instructions[0xdd]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xdd]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
  Instructions[0xdd]->AddStep(new Cat2<IndirectionUnit<Adr> ,CMPUnit>(this));           // op->ea,A-op->op
  Instructions[0xdd]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xde: DEC absolute,X 7 cycles (even on 65c02)
  Disassembled[0xde] =Instruction("DEC",Instruction::Absolute_X,7);
  Instructions[0xde]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xde]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
  Instructions[0xde]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0xde]->AddStep(new Cat1<DECUnit>(this));                         // --op->op
  else
    Instructions[0xde]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));      // --op->op
  Instructions[0xde]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0xde]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xdf: BBS5 zpage,disp, 5 cycles
    Disassembled[0xdf] =Instruction("BBS5",Instruction::ZPage_Disp,5);
    Instructions[0xdf]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xdf]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xdf]->AddStep(new Cat1<BranchBitTestUnit<0x20,0x20> >(this));   // branch if bit #5 is 1
    Instructions[0xdf]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xdf: DCM absolute,X 7 cycles
    Disassembled[0xdf] =Instruction("DECP",Instruction::Absolute_X,7);
    Instructions[0xdf]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xdf]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
    Instructions[0xdf]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0xdf]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,DECUnit>(this,Emulate65C02));      // --op->op
    Instructions[0xdf]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,CMPUnit>(this,Emulate65C02));      // op->(ea),A-op->flags
    Instructions[0xdf]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsE0
// Build instruction opcodes 0xE0 to 0xEf
template<class Adr>
void CPU::BuildInstructionsE0(void)
{
  //
  // 0xe0: CPX immediate, 2 cycles
  Disassembled[0xe0] =Instruction("CPX",Instruction::Immediate,2);
  Instructions[0xe0]->AddStep(new Cat2<ImmediateUnit,CPXUnit>(this));             // data->op,X-op->flags
  Instructions[0xe0]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xe1: SBC (addr,X): 6 cycles
  Disassembled[0xe1] =Instruction("SBC",Instruction::Indirect_X,6);
  Instructions[0xe1]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xe1]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xe1]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));    // (op)->op
  Instructions[0xe1]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // (op)->op
  if (Emulate65C02) {
    Instructions[0xe1]->AddStep(new Cat1<SBCUnitFixed>(this));                    // A-op->A
  } else {
    Instructions[0xe1]->AddStep(new Cat1<SBCUnit>(this));                         // A-op->A
  }
  Instructions[0xe1]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xe2: NOP immed, 2 cycles
  Disassembled[0xe2] =Instruction("NOPE",Instruction::Immediate,2);
  Instructions[0xe2]->AddStep(new Cat1<ImmediateUnit>(this));                     // data->op
  Instructions[0xe2]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0xe3] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xe3]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xe3: INS (addr,X): 8 cycles
    Disassembled[0xe3] =Instruction("INSB",Instruction::Indirect_X,8);
    Instructions[0xe3]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xe3]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0xe3]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0xe3]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // op->ea,(op)->op
    Instructions[0xe3]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));// ++op->op
    Instructions[0xe3]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));        // op->ea
    Instructions[0xe3]->AddStep(new Cat1<SBCUnit>(this));                         // A-op->A
    Instructions[0xe3]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0xe4: CPX zpage, 3 cycles
  Disassembled[0xe4] =Instruction("CPX",Instruction::ZPage,3);
  Instructions[0xe4]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xe4]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,CPXUnit>(this)); // (op)->op,X-op->flags
  Instructions[0xe4]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0xe5: SBC zpage, 3 cycles
  Disassembled[0xe5] =Instruction("SBC",Instruction::ZPage,3);
  Instructions[0xe5]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  if (Emulate65C02) {
    Instructions[0xe5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,SBCUnitFixed>(this)); // A-op->A
  } else {
    Instructions[0xe5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,SBCUnit>(this));// A-op->A
  }
  Instructions[0xe5]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xe6: INC zpage, 5 cycles
  Disassembled[0xe6] =Instruction("INC",Instruction::ZPage,5);
  Instructions[0xe6]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xe6]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0xe6]->AddStep(new Cat1<INCUnit>(this));                           // ++op->op
  Instructions[0xe6]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->ea
  Instructions[0xe6]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  if (Emulate65C02) {
    // 0xe7: SMB6 zpage, 5 cycles
    Disassembled[0xe7] =Instruction("SMB6",Instruction::ZPage,5);
    Instructions[0xe7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xe7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xe7]->AddStep(new Cat1<SMBUnit<0x40> >(this));                  // set bit #6
    Instructions[0xe7]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0xe7]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xe7: INS zpage, 5 cycles
    Disassembled[0xe7] =Instruction("INSB",Instruction::ZPage,5);
    Instructions[0xe7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xe7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xe7]->AddStep(new Cat1<INCUnit>(this));                         // ++op->op
    Instructions[0xe7]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,SBCUnit>(this,Emulate65C02)); // op->ea,A-op->A
    Instructions[0xe7]->AddStep(new Cat1<DecodeUnit>(this));                      // decode next instruction
  }
  //
  // 0xe8: INX 2 cycles
  Disassembled[0xe8] =Instruction("INX",Instruction::NoArgs,2);
  Instructions[0xe8]->AddStep(new Cat3<XUnit,INCUnit,LDXUnit>(this));
  Instructions[0xe8]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0xe9: SBC immediate, 2 cycles
  Disassembled[0xe9] =Instruction("SBC",Instruction::Immediate,2);
  if (Emulate65C02) {
    Instructions[0xe9]->AddStep(new Cat2<ImmediateUnit,SBCUnitFixed>(this));      // A-op->A
  } else {
    Instructions[0xe9]->AddStep(new Cat2<ImmediateUnit,SBCUnit>(this));           // A-op->A
  }
  Instructions[0xe9]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  //
  // 0xea: NOP 2 cycles
  Disassembled[0xea] =Instruction("NOP",Instruction::NoArgs,2);                   // the one and only real
  Instructions[0xea]->AddStep(new Cat1<WaitUnit>(this));                          // just delay
  Instructions[0xea]->AddStep(new Cat1<DecodeUnit>(this));                        // decode next instruction
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0xeb] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xeb]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xeb: A workalike of SBC
    Disassembled[0xeb] =Instruction("SUBC",Instruction::Immediate,2);
    Instructions[0xeb]->AddStep(new Cat2<ImmediateUnit,SBCUnit>(this));
    Instructions[0xeb]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xec: CPX abs, 4 cycles
  Disassembled[0xec] =Instruction("CPX",Instruction::Absolute,4);
  Instructions[0xec]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xec]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0xec]->AddStep(new Cat2<IndirectionUnit<Adr> ,CPXUnit>(this));           // (op)->op,X-op->flags
  Instructions[0xec]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xed: SBC abs, 4 cycles
  Disassembled[0xed] =Instruction("SBC",Instruction::Absolute,4);
  Instructions[0xed]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xed]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  if (Emulate65C02) {
    Instructions[0xed]->AddStep(new Cat2<IndirectionUnit<Adr> ,SBCUnitFixed>(this));    // A-op->A
  } else {
    Instructions[0xed]->AddStep(new Cat2<IndirectionUnit<Adr> ,SBCUnit>(this));         // A-op->A
  }
  Instructions[0xed]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xee: INC abs, 6 cycles
  Disassembled[0xee] =Instruction("INC",Instruction::Absolute,6);
  Instructions[0xee]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xee]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));        // hi->op
  Instructions[0xee]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0xee]->AddStep(new Cat1<INCUnit>(this));                         // ++op->op
  else
    Instructions[0xee]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));      // ++op->op
  Instructions[0xee]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0xee]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xef: BBS6 zpage,disp, 5 cycles
    Disassembled[0xef] =Instruction("BBS6",Instruction::ZPage_Disp,5);
    Instructions[0xef]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xef]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xef]->AddStep(new Cat1<BranchBitTestUnit<0x40,0x40> >(this));   // branch if bit #6 is 1
    Instructions[0xef]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xef: INS abs, 6 cycles
    Disassembled[0xef] =Instruction("INSB",Instruction::Absolute,6);
    Instructions[0xef]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xef]->AddStep(new Cat1<ImmediateWordExtensionUnit>(this));      // hi->op
    Instructions[0xef]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0xef]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));      // ++op->op
    Instructions[0xef]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,SBCUnit>(this,Emulate65C02));      // op->(ea)
    Instructions[0xef]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsF0
// Build instruction opcodes 0xF0 to 0xFf
template<class Adr>
void CPU::BuildInstructionsF0(void)
{
  //
  // 0xf0: BEQ abs, 2 cycles
  Disassembled[0xf0] =Instruction("BEQ",Instruction::Disp,2);
  Instructions[0xf0]->AddStep(new Cat1<BranchDetectUnit<Z_Mask,Z_Mask> >(this));  // branch if Z flag set
  Instructions[0xf0]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xf1: SBC (indirect),Y 5* cycles
  Disassembled[0xf1] =Instruction("SBC",Instruction::Indirect_Y,5);
  Instructions[0xf1]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xf1]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnit>(this));  // (op)->op,op+Y->op
  Instructions[0xf1]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // (op)->op
  if (Emulate65C02) {
    Instructions[0xf1]->AddStep(new Cat1<SBCUnitFixed>(this));                    // A-op->A
  } else {
    Instructions[0xf1]->AddStep(new Cat1<SBCUnit>(this));                         // A-op->A
  }
  Instructions[0xf1]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xf2: JAM
  if (Emulate65C02) {
    // SBC (zpage)
    Disassembled[0xf2] =Instruction("SBC",Instruction::Indirect_Z,5);
    Instructions[0xf2]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xf2]->AddStep(new Cat1<ZPageWordIndirectionUnit<Adr> >(this));  // (op)->op
    Instructions[0xf2]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));           // (op)->op
    Instructions[0xf2]->AddStep(new Cat1<SBCUnitFixed>(this));                    // A-op->A
    Instructions[0xf2]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    Disassembled[0xf2] =Instruction("HALT",Instruction::NoArgs,0);
    Instructions[0xf2]->AddStep(new Cat1<JAMUnit<0xf2> >(this));                  // just crash
    Instructions[0xf2]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //  
  if (Emulate65C02) {
    // NOP
    Disassembled[0xf3] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xf3]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xf3: INS (indirect),Y 8 cycles
    Disassembled[0xf3] =Instruction("INSB",Instruction::Indirect_Y,8);
    Instructions[0xf3]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
    Instructions[0xf3]->AddStep(new Cat2<ZPageWordIndirectionUnit<Adr>,AddYUnitWait>(this)); // (op)->op,op+Y->op
    Instructions[0xf3]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));             // op->ea,(op)->op
    Instructions[0xf3]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));  // --op->op
    Instructions[0xf3]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,SBCUnit>(this,Emulate65C02));  // A-op->A
    Instructions[0xf3]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xf4: NOP zpage,X 4 cycles
  Disassembled[0xf4] =Instruction("NOPE",Instruction::ZPage_X,4);
  Instructions[0xf4]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xf4]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xf4]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // (op)->op
  Instructions[0xf4]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xf5: SBC zpage,X 4 cycles
  Disassembled[0xf5] =Instruction("SBC",Instruction::ZPage_X,4);
  Instructions[0xf5]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xf5]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  if (Emulate65C02) {
    Instructions[0xf5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,SBCUnitFixed>(this)); // (op)->op,A-op->A
  } else {
    Instructions[0xf5]->AddStep(new Cat2<ZPageIndirectionUnit<Adr>,SBCUnit>(this));      // (op)->op,A-op->A
  }
  Instructions[0xf5]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xf6: INC zpage,X 6 cycles
  Disassembled[0xf6] =Instruction("INC",Instruction::ZPage_X,6);
  Instructions[0xf6]->AddStep(new Cat1<ImmediateUnit>(this));                     // ZPage address->op
  Instructions[0xf6]->AddStep(new Cat1<AddXUnitZero>(this));                      // op+X->op
  Instructions[0xf6]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));        // op->ea,(op)->op
  Instructions[0xf6]->AddStep(new Cat1<INCUnit>(this));                           // ++op->op
  Instructions[0xf6]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this,Emulate65C02));     // op->(ea)
  Instructions[0xf6]->AddStep(new Cat1<DecodeUnit>(this));
  // 
  if (Emulate65C02) {
    // 0xf7: SMB7 zpage, 5 cycles
    Disassembled[0xf7] =Instruction("SMB7",Instruction::ZPage,5);
    Instructions[0xf7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xf7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xf7]->AddStep(new Cat1<SMBUnit<0x80> >(this));                  // set bit #7
    Instructions[0xf7]->AddStep(new Cat1<ZPageIndirectWriterUnit<Adr> >(this));   // op->ea
    Instructions[0xf7]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xf7: INS zpage,X 6 cycles
    Disassembled[0xf7] =Instruction("INSB",Instruction::ZPage,6);
    Instructions[0xf7]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xf7]->AddStep(new Cat1<AddXUnitZero>(this));                    // op+X->op
    Instructions[0xf7]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xf7]->AddStep(new Cat1<INCUnit>(this));                         // ++op->op
    Instructions[0xf7]->AddStep(new Cat2<ZPageIndirectWriterUnit<Adr>,SBCUnit>(this,Emulate65C02)); // op->(ea),A-op->A
    Instructions[0xf7]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xf8: SED, 2 cycles
  Disassembled[0xf8] =Instruction("SED",Instruction::NoArgs,2);
  Instructions[0xf8]->AddStep(new Cat1<OrToStatusUnit<D_Mask> >(this));           // P | D -> P
  Instructions[0xf8]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xf9: SBC absolute,Y 4* cycles
  Disassembled[0xf9] =Instruction("SBC",Instruction::Absolute_Y,4);
  Instructions[0xf9]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xf9]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnit>(this)); // hi->op, op+Y->op
  if (Emulate65C02) {
    Instructions[0xf9]->AddStep(new Cat2<IndirectionUnit<Adr> ,SBCUnitFixed>(this));    // A-op->A
  } else {
    Instructions[0xf9]->AddStep(new Cat2<IndirectionUnit<Adr> ,SBCUnit>(this));         // A-op->A
  }
  Instructions[0xf9]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // PLX, 4 cycles
    Disassembled[0xfa] =Instruction("PLX",Instruction::NoArgs,4);
    Instructions[0xfa]->AddStep(new Cat1<PullUnit<Adr> >(this));                  // stack->op
    Instructions[0xfa]->AddStep(new Cat1<LDXUnit>(this));                         // X->op
    Instructions[0xfa]->AddStep(new Cat1<WaitUnit>(this));                        // used for whatever
    Instructions[0xfa]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xfa: NOP, 2 cycles 
    Disassembled[0xfa] =Instruction("NOPE",Instruction::NoArgs,2);
    Instructions[0xfa]->AddStep(new Cat1<WaitUnit>(this));                        // delay a cycle
    Instructions[0xfa]->AddStep(new Cat1<DecodeUnit>(this));
  }
  // 
  if (Emulate65C02) {
    // NOP
    Disassembled[0xfb] =Instruction("NOPE",Instruction::NoArgs,1);
    Instructions[0xfb]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xfb: INS absolute,Y 7 cycles
    Disassembled[0xfb] =Instruction("INSB",Instruction::Absolute_Y,7);
    Instructions[0xfb]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xfb]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddYUnitWait>(this)); // hi->op, op+Y->op
    Instructions[0xfb]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0xfb]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));      // ++op->op
    Instructions[0xfb]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,SBCUnit>(this,Emulate65C02));      // A-op->A
    Instructions[0xfb]->AddStep(new Cat1<DecodeUnit>(this));
  }
  //
  // 0xfc: NOP absolute,X 4* cycles
  Disassembled[0xfc] =Instruction("NOPE",Instruction::Absolute_X,4);
  Instructions[0xfc]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xfc]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
  Instructions[0xfc]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  Instructions[0xfc]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xfd: SBC absolute,X 4* cycles
  Disassembled[0xfd] =Instruction("SBC",Instruction::Absolute_X,4);
  Instructions[0xfd]->AddStep(new Cat1<ImmediateUnit>(this));                     // lo->op
  Instructions[0xfd]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnit>(this)); // hi->op, op+X->op
  if (Emulate65C02) {
    Instructions[0xfd]->AddStep(new Cat2<IndirectionUnit<Adr> ,SBCUnitFixed>(this));    // A-op->A
  } else {
    Instructions[0xfd]->AddStep(new Cat2<IndirectionUnit<Adr> ,SBCUnit>(this));         // A-op->A
  }
  Instructions[0xfd]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // 0xfe: INC absolute,X 7 cycles (even on 65C02)
  Disassembled[0xfe] =Instruction("INC",Instruction::Absolute_X,7);
  Instructions[0xfe]->AddStep(new Cat1<ImmediateUnit>(this));                           // lo->op
  Instructions[0xfe]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
  Instructions[0xfe]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                   // op->ea,(op)->op
  if (Emulate65C02)
    Instructions[0xfe]->AddStep(new Cat1<INCUnit>(this));                               // ++op->op
  else
    Instructions[0xfe]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));      // ++op->op
  Instructions[0xfe]->AddStep(new Cat1<IndirectWriterUnit<Adr> >(this,Emulate65C02));                // op->(ea)
  Instructions[0xfe]->AddStep(new Cat1<DecodeUnit>(this));
  //
  if (Emulate65C02) {
    // 0xff: BBS7 zpage,disp, 5 cycles
    Disassembled[0xff] =Instruction("BBS7",Instruction::ZPage_Disp,5);
    Instructions[0xff]->AddStep(new Cat1<ImmediateUnit>(this));                   // ZPage address->op
    Instructions[0xff]->AddStep(new Cat1<ZPageIndirectionUnit<Adr> >(this));      // op->ea,(op)->op
    Instructions[0xff]->AddStep(new Cat1<BranchBitTestUnit<0x80,0x80> >(this));   // branch if bit #7 is 1
    Instructions[0xff]->AddStep(new Cat1<DecodeUnit>(this));
  } else {
    // 0xff: INS absolute,X 7 cycles
    Disassembled[0xff] =Instruction("INSB",Instruction::Absolute_X,7);
    Instructions[0xff]->AddStep(new Cat1<ImmediateUnit>(this));                   // lo->op
    Instructions[0xff]->AddStep(new Cat2<ImmediateWordExtensionUnit,AddXUnitWait>(this)); // hi->op, op+X->op
    Instructions[0xff]->AddStep(new Cat1<IndirectionUnit<Adr> >(this));                 // op->ea,(op)->op
    Instructions[0xff]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,INCUnit>(this,Emulate65C02));      // ++op->op
    Instructions[0xff]->AddStep(new Cat2<IndirectWriterUnit<Adr> ,SBCUnit>(this,Emulate65C02));      // op->(ea),A-op->A
    Instructions[0xff]->AddStep(new Cat1<DecodeUnit>(this));
  }
}
///

/// CPU::BuildInstructionsExtra
// Build internal pseudo-instructions for interrupt and reset
// handling.
template<class Adr>
void CPU::BuildInstructionsExtra(void)
{
  // Additional pseudo-instruction slots follow: 
  // We still need: RESET processing, IRQ processing and NMI processing.
  //
  // First, the reset cycle.
  Instructions[0x100]->AddStep(new Cat1<LoadVectorUnit<0xfffc,I_Mask> >(this));  // fetch lo
  Instructions[0x100]->AddStep(new Cat1<LoadVectorUnitExtend<0xfffc> >(this));   // fetch hi
  Instructions[0x100]->AddStep(new Cat1<JMPUnit<0> >(this));                     // jmp to position
  Instructions[0x100]->AddStep(new Cat1<DecodeUnit>(this));
  //
  // NMI reaction. Pushes P, return address onto the stack and executes from the 0xfffa vector
  // Takes 7 cycles.
  Instructions[0x101]->AddStep(new Cat2<LoadPCUnit<0>,PushUnitExtend<Adr> >(this,Emulate65C02)); // PC->op,op.hi->Stack
  Instructions[0x101]->AddStep(new Cat1<PushUnit<Adr> >(this,Emulate65C02));                     // op.lo->Stack
  Instructions[0x101]->AddStep(new Cat2<AndToStatusUnit<NOT(B_Mask)>,PushUnit<Adr> >(this,Emulate65C02)); // P->stack
  if (Emulate65C02) {
    Instructions[0x101]->AddStep(new Cat2<AndToStatusUnit<NOT(D_Mask)>,LoadVectorUnitExtend<0xfffa> >(this));      // IRQ_Vector.hi->op
    // The 65C02 clears the D flag.
    Instructions[0x101]->AddStep(new Cat1<LoadVectorUnit<0xfffa,I_Mask> >(this));
  } else {
    Instructions[0x101]->AddStep(new Cat1<LoadVectorUnitExtend<0xfffa> >(this));      // IRQ_Vector.hi->op
    Instructions[0x101]->AddStep(new Cat1<LoadVectorUnit<0xfffa,I_Mask> >(this));  // IRQ_Vector.lo->op
  }
  Instructions[0x101]->AddStep(new Cat2<NMIResetUnit,JMPUnit<0> >(this));
  Instructions[0x101]->AddStep(new Cat1<DecodeUnit>(this));                     // decode next instruction
  //
  // IRQ reaction. Pushes P, return address onto the stack and executes from the 0xfffe vector
  // Takes 7 cycles.
  Instructions[0x102]->AddStep(new Cat2<LoadPCUnit<0>,PushUnitExtend<Adr> >(this,Emulate65C02));// PC->op,op.hi->Stack
  Instructions[0x102]->AddStep(new Cat1<PushUnit<Adr> >(this,Emulate65C02));                    // op.lo->Stack
  Instructions[0x102]->AddStep(new Cat2<AndToStatusUnit<NOT(B_Mask)>,PushUnit<Adr> >(this,Emulate65C02)); // P->stack
  if (Emulate65C02) {
    // The 65C02 clears the D flag
    Instructions[0x102]->AddStep(new Cat2<AndToStatusUnit<NOT(D_Mask)>,LoadVectorUnitExtend<0xfffe> >(this));// IRQ_Vector.hi->op
    Instructions[0x102]->AddStep(new Cat1<LoadVectorUnit<0xfffe,I_Mask> >(this));
    Instructions[0x102]->AddStep(new Cat1<JMPUnit<0> >(this));
  } else { 
    Instructions[0x102]->AddStep(new Cat1<LoadVectorUnitExtend<0xfffe> >(this));// IRQ_Vector.hi->op
    Instructions[0x102]->AddStep(new Cat1<LoadVectorUnitQuirk<0xfffe,0xfffa,I_Mask> >(this));  // IRQ_Vector.lo->op
    Instructions[0x102]->AddStep(new Cat2<NMIResetUnit,JMPUnit<0> >(this));
  }
  Instructions[0x102]->AddStep(new Cat1<DecodeUnit>(this));                     // decode next instruction
}
///

/// CPU::ColdStart
// Run the coldstart of the CPU now.
void CPU::ColdStart(void)
{
  // All we do here is to extract the CPU ram space from the
  // MMU
  monitor  = machine->Monitor();
  Ram      = machine->MMU()->CPURAM();
  DebugRam = machine->MMU()->DebugRAM();
  //
  // Fill in ZPage and Stack
  ZPage    = Ram->ZeroPage();
  Stack    = Ram->StackPage();
  //
  BuildInstructions(EnableWatch);
  //
  // and then run into a warmstart
  WarmStart();
}
///

/// CPU::WarmStart
// Reset all registers of the CPU and capture the reset vector
void CPU::WarmStart(void)
{
  IRQMask        = 0x00;
  NMI            = false; 
  IRQPending     = false;
  HaltStart      = ClocksPerLine;
  GlobalA        = 0x00;
  GlobalX        = 0x00;
  GlobalY        = 0x00;
  GlobalP        = 0x20;  // the unused bit is always on
  GlobalS        = 0xff;  // reset stack pointer to top of stack
  CycleCounter   = 0;
  ProfileCounter = 0;
  // Fetch the reset vector and run from there.
  ExecutionSteps = Instructions[0x100]->Sequence;
  NextStep       = *ExecutionSteps++;
#ifdef BUILD_MONITOR
  if (TraceOnReset) {
    EnableTrace();
  }
#endif
  HBI();
}
///

/// CPU::SetBreakPoint
// Install/set a new break point at an indicated position,
// return the id or -1 if no break point is available.
int CPU::SetBreakPoint(ADR where)
{
  int i;

  for(i=0;i<NumBreakPoints;i++) {
    if (BreakPoints[i].Free) {  
      EnableBreak            = true;
      BreakPoints[i].Free    = false;
      BreakPoints[i].Enabled = true;
      BreakPoints[i].BreakPC = UWORD(where);
      return i;
    }
  }
  return -1;
}
///

/// CPU::ClearBreakPoint
// Release the breakpoint register #i
void CPU::ClearBreakPoint(int i)
{
  BreakPoints[i].Free    = true;
  BreakPoints[i].Enabled = false;
  BreakPoints[i].BreakPC = 0x0000;  
  
  for(i=0;i<NumBreakPoints;i++) {
    if (!BreakPoints[i].Free) {  
      EnableBreak            = true;
      return;
    }
  }

  // Disable this feature otherwise as it costs time
  EnableBreak = EnableStacking || EnableTracing || EnableUntil || EnableWatch;
}
///

/// CPU::EnableBreakPoint
// Enable the breakpoint register #i
void CPU::EnableBreakPoint(int i)
{
  if (!BreakPoints[i].Free) {
    BreakPoints[i].Enabled = true;
  }
}
///

/// CPU::DisableBreakPoint
// Disable the breakpoint register #i
void CPU::DisableBreakPoint(int i)
{
  if (!BreakPoints[i].Free) {
    BreakPoints[i].Enabled = false;
  }
}
///

/// CPU::IfBreakPoint
// Test whether a breakpoint has been set
// at the specified address
bool CPU::IfBreakPoint(ADR where)
{
  int i;

  for(i=0;i<NumBreakPoints;i++) {
    if (BreakPoints[i].Enabled          &&
	BreakPoints[i].Free    == false &&
	BreakPoints[i].BreakPC == UWORD(where)) {
      return true;
    }
  }

  return false;
}
///

/// CPU::EnableWatchPoints
// Enable all watch points and start the logic that checks the watch points.
void CPU::EnableWatchPoints(void)
{
  if (!EnableWatch) {
    BuildInstructions(true);
    EnableWatch    = true;  
    //
    // Fetch the NOP and restart from there.
    ExecutionSteps = Instructions[0xea]->Sequence;
    NextStep       = *ExecutionSteps++;
  }
  EnableBreak = true;
}
///

/// CPU::DisableWatchPoints
// Disable all watch points and disable the logic that checks them.
void CPU::DisableWatchPoints(void)
{
  if (EnableWatch) {
    BuildInstructions(false);
    EnableWatch = false;  
    //
    // Fetch the NOP and restart from there.
    ExecutionSteps = Instructions[0xea]->Sequence;
    NextStep       = *ExecutionSteps++;
  }
  EnableBreak = EnableStacking || EnableTracing || EnableUntil || EnableWatch;
}
///

/// CPU::EnableTrace
// Enable tracing for the CPU
void CPU::EnableTrace(void)
{
  EnableTracing = true;
  EnableBreak   = true;
}
///

/// CPU::DisableTrace
// Disable tracing for the CPU
void CPU::DisableTrace(void)
{
  EnableTracing = false;
}
///

/// CPU::EnableStack
// Enable the stack checking. We generate a trace
// exception in case the stack pointer gets higher
// or equal than the current stack.
void CPU::EnableStack(void)
{
  EnableStacking = true;
  EnableBreak    = true;
  TraceS         = GlobalS;
}
///

/// CPU::DisableStack
// Disable the stack tracing.
void CPU::DisableStack(void)
{
  EnableStacking = false;
}
///

/// CPU::EnablePC
// Enable the PC checking. We generate a trace
// exception in case the program counter gets higher
// than the current program counter.
void CPU::EnablePC(void)
{
  EnableUntil = true;
  EnableBreak = true;
  TracePC     = GlobalPC;
  TraceS      = GlobalS;
}
///

/// CPU::DisablePC
// Disable the PC checking.
void CPU::DisablePC(void)
{
  EnableUntil = false;
}
///

/// CPU::Go
// Emulate the CPU for n cycles. This is the main loop of the
// 6502 emulation. It returns the number of cycles left (if any).
// As this emulation is now cycle-precise, the number of remaining
// steps will be always zero.
int CPU::Go(int cycles)
{
  while(cycles) {
    Step();
    cycles--;
  }
  return 0;
}
///

/// CPU::Sync
// Enforce the CPU to synchronize to the next instruction fetch cycle.
// This is required to be able to save the CPU state and for internal
// synchronization.
void CPU::Sync(void)
{
  ISync = true;
  // Reset the CPU to the beginning of the scanline. This is required
  // to prevent overflows.
  HBI();
  do {
    // Inject a NOP in case we lost the next step due to
    // an exception in the instruction execution itself
    if (NextStep == NULL) {
      class MicroCode **aeu;
      aeu            = Instructions[0xea]->Sequence;
      NextStep       = aeu[0];
      ExecutionSteps = aeu + 1;
    }
    Step();
  } while(ISync);
  HBI();
}
///

/// CPU::StealCycles
// Steal cycles from the CPU. We need to supply the DMASlot definition for it.
void CPU::StealCycles(const struct DMASlot &slot)
{
  int cnt;
  
  if ((cnt = slot.NumCycles)) {
    const UBYTE *in = slot.CycleMask;
    UBYTE *out      = StolenCycles + slot.FirstCycle;
    UBYTE *last     = StolenCycles + slot.LastCycle;
    do {
      *out |= *in;
      out++,in++;
    } while(out < last && --cnt);
  }
}
///

/// CPU::StealMemCycles
// Steal DMA cycles with two cycles elasticity, used for memory refresh.
// If no cycle is available whatsoever, use a cycle cycle at the last slot.
void CPU::StealMemCycles(const struct DMASlot &slot)
{
  int cnt;
  
  if ((cnt = slot.NumCycles)) {
    const UBYTE *in = slot.CycleMask;
    UBYTE *out      = StolenCycles + slot.FirstCycle;
    UBYTE *last     = StolenCycles + slot.LastCycle;
    UBYTE  cycle    = 0;
    do {
      // Still requests for memory refresh slots requested?
      if (cnt) {
	cycle |= *in;
	in++,cnt--;
      }
      // A free slot available? If so, possibly fill in a delayed
      // request.
      if ((*out & 0x01) == 0) {
	*out |= cycle;
	cycle = 0;
      }
      out++;
      // Until all requested slots are filled, or there
      // are no slots left were we could allocate.
    } while(out < last && (cycle || cnt));
  }
}
///

/// CPU::WSyncStop
// Stop on a WSync write: This blocks the
// CPU until the next WSYNC release position
// (or the next line if we're over it)
void CPU::WSyncStop(void)
{
  UBYTE *cycle,*last;
  //
  // This logic detects double-WSYNC writes due to INC/DEC
  // and thus works around the Spaceshit demo.
  cycle     = CurCycle;
  last      = StolenCycles + WSyncPosition;
  //
  // Plus one blocks the next available cycle (the one that will be used by the CPU)
  // We *are* currently executing "cycle". "cycle+1" must remain free, start
  // blocking at cycle + 2.
  cycle += 2;
  if (cycle > last) {
#if CHECK_LEVEL > 0
    if (cycle >= StolenCycles + sizeof(StolenCycles)) {
      Throw(OutOfRange,"CPU::WSyncStop","detected out of bounds CPU cycle");
    }
#endif 
    // Block to end of line, we're past WSYNC stop. Use a different
    // value here to ensure that the memory refresh allocator does
    // not confuse the stopped cycles with something else allocating
    // the bus.
    last      = StolenCycles + ClocksPerLine;
    if (cycle < last) {
      do {
	*cycle++ |= 0x02;
      } while(cycle < last);
      //
      // Carry over to the next line.
      HaltStart = 0;
    } else {
      // Carry over to the next line.
      HaltStart = UBYTE(cycle - last);
    }
  } else if (cycle < last) {
    // Block start of line up to the next possible HPos
    do {
      *cycle++ |= 0x02;
    } while(cycle < last);
  }
}
///

/// CPU::HBI
// Signal a horizontal blank for the CPU, reseting the cycles stolen so far
void CPU::HBI(void)
{
  CurCycle = StolenCycles;
  //
  // Release all cycles up to the 114th cycle which is the HSync position
  // All other cycles must remain allocated to avoid unnecessary tests.
  memset(StolenCycles,0,ClocksPerLine);
  // Check whether we are still halted. If so, then block
  // the cycles up to the WSYNC position.
  if (HaltStart < WSyncPosition) {
    UBYTE *cycle = StolenCycles + HaltStart;
    UBYTE *last  = StolenCycles + WSyncPosition;
    do {
      *cycle++ |= 0x02;
    } while(cycle < last);
    // Done.
    HaltStart = ClocksPerLine;
  }
}
///

/// CPU::StartProfiling
// Start profiling CPU execution.
void CPU::StartProfiling(void)
{
  if (ProfilingCounters == NULL)
    ProfilingCounters = new ULONG[1L << 16];

  if (CumulativeCounters == NULL)
    CumulativeCounters = new ULONG[1L << 16];

  memset(ProfilingCounters ,0,(1L << 16) * sizeof(ULONG));
  memset(CumulativeCounters,0,(1L << 16) * sizeof(ULONG));
}
///

/// CPU::StopProfiling
// Stop profiling CPU execution.
void CPU::StopProfiling(void)
{
  delete[] ProfilingCounters;
  delete[] CumulativeCounters;
  
  ProfilingCounters  = NULL;
  CumulativeCounters = NULL;
}
///

/// CPU::ParseArgs
// Parse off command line arguments for the CPU
void CPU::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector cputypevector[] = 
    { {"6502"          ,0  },
      {"WD65C02"       ,1  },
      {NULL    ,0}
    };
  LONG cputype  = ((Emulate65C02)?(1):(0));
  LONG oldtype  = cputype;
  
  args->DefineTitle("CPU");
  //args->DefineLong("WSyncPosition","position of the WSYNC release cycle",80,ClocksPerLine,WSyncPosition); 
#ifdef BUILD_MONITOR
  args->DefineBool("TraceOnReset","enable tracing in the reset phase",TraceOnReset);
  args->DefineBool("TraceInterrupts","enable stepping/tracing of interrupts",TraceInterrupts);
#endif
  // 
  args->DefineSelection("CPUType","CPU variant to use",cputypevector,cputype);
  Emulate65C02  = (cputype & 1)?(true):(false);
  //
  if (cputype != oldtype)
    args->SignalBigChange(ArgParser::ColdStart);
}
///

/// CPU::DisplayStatus
// Display the status of the CPU core over the monitor
void CPU::DisplayStatus(class Monitor *mon)
{
  int i;
  UBYTE *slot;
  UBYTE cycleline[65];
  int HPos;

  HPos = int(CurCycle - StolenCycles);
  
  mon->PrintStatus("%s status:\n"
		   "PC   : %04x\tA    : %02x\tX    : %02x\tY    : %02x\n"
		   "P    : %02x\t%c%c%c%c%c%c%c%c\tS    : 01%02x\n"
		   "HPos          : %d \t\tVPos         : %d\n"
		   "WSyncPosition : " ATARIPP_LD "\t\tTraceOnReset : %s\t\tTraceInterrupts: %s\n",
		   (Emulate65C02)?"65C02":"6502",
		   GlobalPC,GlobalA,GlobalX,GlobalY,GlobalP,
		   (GlobalP & 0x80)?('N'):('_'),
		   (GlobalP & 0x40)?('V'):('_'),
		   (GlobalP & 0x20)?('X'):('_'),
		   (GlobalP & 0x10)?('B'):('_'),
		   (GlobalP & 0x08)?('D'):('_'),
		   (GlobalP & 0x04)?('I'):('_'),
		   (GlobalP & 0x02)?('Z'):('_'),
		   (GlobalP & 0x01)?('C'):('_'),
		   GlobalS,
		   HPos,machine->Antic()->CurrentYPos(),WSyncPosition,
		   TraceOnReset?("on"):("off"),
		   TraceInterrupts?("on"):("off")
		   );

  mon->PrintStatus("CPU cycles stolen:\n");
  memset(cycleline,' ',64);
  cycleline[64] = 0;
  if ((HPos & 0x01) == 0) {
    cycleline[HPos >> 1] = 'v';
  }
  mon->PrintStatus("%s\n",cycleline);
  for(i=0,slot = StolenCycles;i<64;i++) {
    char c = '?';
    switch((slot[0] & 0x01) | ((slot[1] & 0x01) << 1)) {
    case 0:
      c = ':';
      break;
    case 1:
      c = '!';
      break;
    case 2:
      c = 'i';
      break;
    case 3:
      c = '|';
      break;
    }
    cycleline[i] = c;
    slot += 2;
  }
  mon->PrintStatus("%s\n",cycleline);
  memset(cycleline,' ',64);
  if ((HPos & 0x01) == 1) {
    cycleline[HPos >> 1] = '^';
  }
  mon->PrintStatus("%s\n"
		   "NMI pending: %s  "
		   "IRQ pending: %s\n",
		   cycleline,
		   NMI?("yes"):("no "),
		   IRQPending?("yes"):("no "));

  for(i=0;i<NumBreakPoints;i++) {
    if (!BreakPoints[i].Free) {
      mon->PrintStatus("\tBreakpoint #%d at %04x (%s)\n",
		       i,BreakPoints[i].BreakPC,
		       (BreakPoints[i].Enabled)?("enabled"):("disabled"));
    }
  }
}
///

/// CPU::State
// Read or set the internal status
void CPU::State(class SnapShot *sn)
{
  sn->DefineTitle("CPU");
  sn->DefineLong("PC","CPU program counter",0x0000,0xffff,GlobalPC);
  sn->DefineLong("A","CPU accumulator",0x00,0xff,GlobalA);
  sn->DefineLong("X","CPU X index register",0x00,0xff,GlobalX);
  sn->DefineLong("Y","CPU Y index register",0x00,0xff,GlobalY);
  sn->DefineLong("P","CPU processor status",0x00,0xff,GlobalP);
  sn->DefineLong("S","CPU stack pointer",0x00,0xff,GlobalS);
}
///
