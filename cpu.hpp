/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cpu.hpp,v 1.52 2008/05/22 13:03:54 thor Exp $
 **
 ** In this module: CPU 6502 emulator
 **********************************************************************************/

#ifndef CPU_HPP
#define CPU_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "machine.hpp"
#include "chip.hpp"
#include "adrspace.hpp"
#include "argparser.hpp"
#include "hbiaction.hpp"
#include "saveable.hpp"
#include "instruction.hpp"
///

/// Forward declarations
class Monitor;
class Patch;
///

/// Class CPU
class CPU : public Chip, public Saveable, private HBIAction {
#ifndef HAS_PRIVATE_ACCESS
  // Compiler workaround: Substructures of a class are part of the class and have hence
  // access to private data. This bug has been fixed in the GCC 3.xx series.
public:
#endif
  // The register set of the CPU follows here.
  UWORD GlobalPC; // program counter
  UBYTE GlobalA;  // Accumulator
  UBYTE GlobalX;  // X index register
  UBYTE GlobalY;  // Y index register
  UBYTE GlobalP;  // P processor status register
  UBYTE GlobalS;  // S stack pointer
  //
#ifdef HAS_MEMBER_INIT
  static const int NumBreakPoints = 8;
#else
#define            NumBreakPoints   8
#endif
  //
  // The following might be set by the monitor: It is a break point
  bool   EnableBreak;
  bool   EnableTracing;
  bool   EnableStacking;
  bool   EnableUntil;
  bool   TraceInterrupts;
  struct BreakPoint {
    bool Enabled;  // If this break point is activated
    bool Free;     // if this slot is still available
    UWORD BreakPC;
  } BreakPoints[NumBreakPoints];
  //
public:
  // Position of the CPU flags within the P register
  enum StatusFlags {
    N_Flag   = 7,  // negative
    V_Flag   = 6,  // overflow
    B_Flag   = 4,  // software breakpoint
    D_Flag   = 3,  // decimal arithmetic
    I_Flag   = 2,  // interrupt suspended?
    Z_Flag   = 1,  // zero result code?
    C_Flag   = 0   // carry
  };
  //
  // And now the same with flags
  enum StatusMask {
    N_Mask   = 0x80,  // negative
    V_Mask   = 0x40,  // overflow
    B_Mask   = 0x10,  // software breakpoint
    D_Mask   = 0x08,  // decimal arithmetic
    I_Mask   = 0x04,  // interrupt suspended?
    Z_Mask   = 0x02,  // zero result code?
    C_Mask   = 0x01   // carry
  };  
  //
  // The management structure that allows to reserve cycles for the CPU
  // for DMA. Hence, this defines where to steal cycles.
  struct DMASlot {
    int          FirstCycle;   // The first cycle in the horizontal line to steal
    int          NumCycles;    // The number of cycles to steal.
    int          LastCycle;    // The first cycle not to touch any more.
    const UBYTE *CycleMask;    // set a byte here to one to indicate that the cycle has to be stolen
  };
  //
private:
  //
  // The structure used to exchange clock information between the main thread
  // and the side thread.
  struct CycleInfo {
    int          NewCycles;        // number of cycles the CPU may now run.
    int          OverWSyncCycles;  // number of cycles from this go that are beyond the WSync release position.
    bool         ReleaseHalt;      // set by the main thread if WSync gets released.
  };
  //
#ifndef HAS_PRIVATE_ACCESS
  // Compiler workaround: Substructures of a class are part of the class and have hence
  // access to private data. This bug has been fixed in the GCC 3.xx series.
public:
#endif
  // A pointer to the monitor 
  class Monitor  *monitor;
  //
  // A pointer to the RAM we need for faster reference
  class AdrSpace *Ram;
  //
  // Pointer to the Z-Page and the stack: These do not
  // take the LONG routes thru the pager, but are rather
  // addressed directly for speed reasons.
  UBYTE *ZPage,*Stack;
  //
  // Reference program counter, used to break on either PCs
  // larger or smaller than the indicated value.
  UWORD TracePC;
  //
  // Last S (stack) pointer. If the stack pointer grows larger or
  // equal than this, we run into a stack traceback. This is 
  // useful for subroutine tracing.
  UBYTE TraceS;
  //
  // The following copy of the stack pointer is kept as soon as we
  // enter an interrupt. It is used to disable tracing of
  // interrupts until the interrupt returns.
  UBYTE InterruptS;
  //
  // The following flag identifies all pending IRQ devices.
  ULONG IRQMask;
  //
  // The following flag is set if an NMI is pending.
  bool NMI;
  //
  // Set the following flag to enforce a synchronization of the CPU to
  // an instruction fetch cycle.
  bool ISync;
  //
  // The following flag is set if the CPU is halted up to the WSYNC release position.
  bool Halted;
  //
  // The following flag gets cleared on each instruction boundary, used by the WSYNC
  // logic to detect double WSYNC writes due to instruction-double writes.
  bool IFlag;
  //
  // Pointer to the current DMA slot.
  UBYTE *CurCycle;
  //
  // CPU preferences
  LONG WSyncPosition; // horizontal position of the WSync release slot. Defaults to 104.
  //
  // Set this to enable tracing on a reset
  bool TraceOnReset;
  //
  // Set this to emulate a WDC 65C02 instead.
  bool Emulate65C02;
  //
  // The following table contains (for quick-access) pre-computed
  // flag-updates depending on the operand. It provides Z and N
  // flags quickly without a branch.
  static const UBYTE FlagUpdate[256];
  //
  //
  //  
  // A single cycle as taken by the CPU. This is a base class several other
  // execution units derive from.
  class AtomicExecutionUnit {
  protected:
    // The address space we have to read from and write to
    class AdrSpace *const Ram;
    // Special address space shortcuts for faster processing:
    // Pointer to the Z-Page and the stack: These do not
    // take the LONG routes thru the pager, but are rather
    // addressed directly for speed reasons.
    UBYTE          *const ZPage;
    UBYTE          *const Stack;
    // Pointer to the CPU for reading/writing of registers.
    class CPU      *const Cpu;
    //
  public:
    AtomicExecutionUnit(class CPU *);
  };
  //
  // A wrapper for the above that supplies a virtual execution method.
  class MicroCode {
  public:
    virtual ~MicroCode(void)
    {}
    // The instruction step this execution unit performs
    // This is why we implement this, after all. It returns a value
    // required for the next execution unit. As registers are at most
    // 16 bit wide in the 6502, this operand size should be sufficent.
    // The next state argument is the next state to be executed. We may
    // alter it in case a wait state has to be inserted.
    virtual UWORD Execute(UWORD operand) = 0;
    // Insert another slot intermediately into the execution order. Might be called
    // dependent on the execution data.
    void Insert(class CPU *cpu);
  };
  //
  // The concatenation unit: This is a templated class that takes two basic instructions
  // and executes them in one step instead of two.
  template<class Step1,class Step2> 
  class Cat2 : public MicroCode {
    Step1 First;
    Step2 Second;
    //
  public:
    Cat2(class CPU *cpu)
      : First(cpu), Second(cpu)
    { }
    //
    UWORD Execute(UWORD operand)
    {
      return Second.Execute(First.Execute(operand));
    }
  };
  //
  // The same, but with three steps.
  template<class Step1,class Step2,class Step3> 
  class Cat3 : public MicroCode {
    Step1 First;
    Step2 Second;
    Step3 Third;
    //
  public:
    Cat3(class CPU *cpu)
      : First(cpu), Second(cpu), Third(cpu)
    { }
    //
    UWORD Execute(UWORD operand)
    {
      return Third.Execute(Second.Execute(First.Execute(operand)));
    }
  };
  // The same, but a single step. This is what is really found in the instruction
  // sequence.
  template<class Step>
  class Cat1 : public MicroCode {
    Step First;
    //
  public:
    Cat1(class CPU *cpu)
      : First(cpu)
    { }
    //
    UWORD Execute(UWORD operand)
    {
      return First.Execute(operand);
    }
  };
  //
  // An execution sequence: A sequence = an array of steps. As total, an
  // instruction with its instruction slots.
  struct ExecutionSequence {
    // An array of execution sequences
    class MicroCode *Sequence[8]; // There are no instructions with more than eight cycles
    // 
  public:
    ExecutionSequence(void);
    ~ExecutionSequence(void);
    //
    // For construction of an execution sequence: Add an atomic
    // execution sequence here.
    void AddStep(class MicroCode *step);
  }                        **Instructions;
  //
  // The pointer to the next automic execution unit to be performed. This
  // is read by the step dispatcher
  class MicroCode  *NextStep;
  // Pointer to an array of execution steps to be performed for a specific
  // instruction. Each new step is read off this array and the Execute method
  // of each AEU is performed step by step on this.
  class MicroCode **ExecutionSteps;
  // The operand of the current AEU. Each AEU will read this operand, perform
  // an operation on it and return it modified.
  UWORD AtomicExecutionOperand;
  // Some EAUs require a second operand called the effective address. It is kept
  // here.
  UWORD EffectiveAddress;
  //
  // Various intermediate execution units for address calculations:
  //
  // The Wait Unit just implements a delay slot in the pipeline.
  class WaitUnit : public AtomicExecutionUnit {
  public:
    WaitUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      return operand;
    }
  };
  //
  // The immediate addressing mode execution unit. It reads the next byte after
  // the PC and returns it as operand. This can be also used as the first step
  // in a complex addressing unit requiring multiple steps and one extension
  // byte.
  class ImmediateUnit : public AtomicExecutionUnit {
  public:
    ImmediateUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return UWORD(Ram->ReadByte(Cpu->GlobalPC++));
    }
  };
  //
  // The immediate word extension unit. It reads the high-byte of a two-byte
  // sized operand from the current PC and inserts it as high-byte into the
  // operand. This is the second step for the absolute, absolute,X and absolute,Y
  // addressing modes.
  class ImmediateWordExtensionUnit : public AtomicExecutionUnit {
  public:
    ImmediateWordExtensionUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      return UWORD((operand & 0x00ff) | (UWORD(Ram->ReadByte(Cpu->GlobalPC++)) << 8));
    }
  };
  //
  // The AddXWait unit takes its operand and adds the X register to it, returning
  // its operand. This is the second step in an (indirect,x) addressing mode.
  // It always adds one additional wait.
  class AddXUnitWait : public AtomicExecutionUnit {
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    AddXUnitWait(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {  
      // Add a wait state here.
      Wait.Insert(Cpu);
      return UWORD(operand + Cpu->GlobalX);
    }
  };  
  //
  // The same as above, except that a wait slot is only added if a page
  // boundary is crossed. This is the second step in an absolute,X addressing
  // step.
  class AddXUnit : public AtomicExecutionUnit {
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    AddXUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {  
      UWORD result = UWORD(operand + Cpu->GlobalX);
      // Add a wait state here if we cross a page boundary
      if ((result ^ operand) & 0xff00) {
	Wait.Insert(Cpu);
      }
      return result;
    }
  };
  //  
  // The AddY unit takes its operand and adds the Y register to it, returning
  // its operand. This is the thrid step in an (indirect),y addressing mode
  // and *may* add a wait state.
  class AddYUnitWait : public AtomicExecutionUnit {
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    AddYUnitWait(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      // Add a wait state here always
      Wait.Insert(Cpu);
      return UWORD(operand + Cpu->GlobalY);
    }
  };
  //
  // The AddY unit takes its operand and adds the Y register to it, returning
  // its operand. This is the thrid step in an (indirect),y addressing mode
  // and *may* add a wait state.
  class AddYUnit : public AtomicExecutionUnit {
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    AddYUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    { 
      UWORD result = UWORD(operand + Cpu->GlobalY);
      // Add a wait state here if we cross a page boundary
      if ((result ^ operand) & 0xff00) {
	Wait.Insert(Cpu);
      }
      return result;
    }
  };
  //
  // The IndirectionUnit takes its argument, interprets it as an effective
  // address and fetches its contents. This is the third step in the
  // (indirect,X) mode and the second for (indirect),Y
  class IndirectionUnit : public AtomicExecutionUnit {
  public:
    IndirectionUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      // Keep the effective address in the CPU for a possible last store
      Cpu->EffectiveAddress = operand;

      return UWORD(Ram->ReadByte(operand));
    }
  };
  //
  // The IndirectionUnitExtend extends the indirection unit by supplying a high-byte.
  // This is only required for the JMP (indirect) instruction, and for
  // that reason, we emulate a 6502 bug: It forgets to carry over to
  // the high-byte.
  class IndirectionUnitExtend : public AtomicExecutionUnit {
  public:
    IndirectionUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      UWORD address;
      // Addition with "forget to carry over".
      address = UWORD((Cpu->EffectiveAddress & 0xff00) | ((Cpu->EffectiveAddress + 1) & 0x00ff));
      return UWORD((operand & 0xff) | (UWORD(Ram->ReadByte(address)) << 8));
    }
  };
  //
  // The 65C02 fixes this behaivour
  class IndirectionUnitExtendFixed : public AtomicExecutionUnit {
    // Used to create a wait-state
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    IndirectionUnitExtendFixed(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      UWORD address = Cpu->EffectiveAddress + 1; 
      // Add a wait state here if we cross a page boundary
      if ((address & 0xff) == 0) {
	Wait.Insert(Cpu);
      }
      return UWORD((operand & 0xff) | (UWORD(Ram->ReadByte(address)) << 8));
    }
  };
  //
  // The ZPageIndirection unit takes its argument, truncates its a single
  // byte and reads the ZPage byte addressed by this argument. This is the
  // last step in a ZPage, ZPage,X or ZPage,Y addressing.
  class ZPageIndirectionUnit : public AtomicExecutionUnit {
  public:
    ZPageIndirectionUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      operand               = UBYTE(operand);
      // Keep the effective address in the CPU for a possible last store
      Cpu->EffectiveAddress = operand;
      return UWORD(ZPage[operand] | (UWORD(ZPage[UBYTE(operand + 1)]) << 8));
    }
  };  
  //
  // The fixed version for the 65C02
  class ZPageIndirectionUnitFixed : public AtomicExecutionUnit {
  public:
    ZPageIndirectionUnitFixed(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      // Keep the effective address in the CPU for a possible last store
      Cpu->EffectiveAddress = operand;
      return UWORD(ZPage[operand] | (UWORD(Ram->ReadByte(operand + 1)) << 8));
    }
  };
  //
  // The IndirectWriter unit is a helper unit that requires an EA from outside
  // to perform its operation.
  class IndirectWriterUnit : public AtomicExecutionUnit {
  public:
    IndirectWriterUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      // The WSYNC checking could happen here by testing for
      // the result code of the WriteByte position. Though
      // this is not needed here, Antic stops the CPU now.
      Ram->WriteByte(Cpu->EffectiveAddress,UBYTE(operand));
      return operand;
    }
  };
  //
  // The ZPageIndirectWriter unit works as above, except that it truncates the
  // EA to the zero page.
  class ZPageIndirectWriterUnit : public AtomicExecutionUnit {
  public:
    ZPageIndirectWriterUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {  
      ZPage[UBYTE(Cpu->EffectiveAddress)] = UBYTE(operand);
      return operand;
    }
  };
  //
  // The load accumulator unit. This takes the operand and stores it in the
  // accumulator. This is the main operation sequence for an LDA instruction
  // It also sets the condition codes.
  class LDAUnit : public AtomicExecutionUnit {
  public:
    LDAUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The accumulator EA unit returns the contents of the accumulator for the A
  // addressing modes.
  // It also writes the operand into the EA for store instruction
  class AccuUnit : public AtomicExecutionUnit {
  public:
    AccuUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->EffectiveAddress = operand;
      return Cpu->GlobalA;
    }
  };
  //
  // The load X register unit. This takes the operand and stores it in the
  // X register. This is the main operation sequence for an LDX instruction.
  // It also sets the condition codes.
  class LDXUnit : public AtomicExecutionUnit {
  public:
    LDXUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The X unit returns the contents of the X index register, used for STX and TXA
  // It also writes the operand into the EA for store instruction
  class XUnit : public AtomicExecutionUnit {
  public:
    XUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand) {
      Cpu->EffectiveAddress = operand;
      return Cpu->GlobalX;
    }
  };
  //
  // The A & X unit: This unit and's the contents of A and X and returns the result.
  // This happens in the real 6502 for some "extra instructions" that put A and X
  // on the same time onto the bus. You'd better not rely on this mess.
  class ANXUnit : public AtomicExecutionUnit {
  public:
    ANXUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->EffectiveAddress = operand;
      return UWORD(Cpu->GlobalA & Cpu->GlobalX);
    }
  };
  //
  // The load Y register unit. This takes the operand and stores it in the
  // Y register. This is the main operation sequence for an LDY instruction.
  // It also sets the condition codes.
  class LDYUnit : public AtomicExecutionUnit {
  public:
    LDYUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //  
  // The Y unit returns the contents of the Y index register, used for STY and TYA
  // It also writes the operand into the EA for store instruction
  class YUnit : public AtomicExecutionUnit {
  public:
    YUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->EffectiveAddress = operand;
      return Cpu->GlobalY;
    }
  };
  //
  // The Zero-unit returns just zero for the 65C02 STZ instructions
  class ZeroUnit : public AtomicExecutionUnit {
  public:
    ZeroUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->EffectiveAddress = operand;
      return 0;
    }
  };
  //
  // The jump unit. This takes the operand and stores it in the
  // PC. This is the main operation sequence for JMP absolute and JMP indirect.
  template<WORD displacement>
  class JMPUnit : public AtomicExecutionUnit {
  public:
    JMPUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand){
      Cpu->GlobalPC = UWORD(operand + displacement);
      return operand;
    }
  };
  //
  // The LoadPCUnit: This places the PC plus a displacement into the operand. This
  // is one step in interrupt processing and in JSR.
  template<WORD displacement>
  class LoadPCUnit : public AtomicExecutionUnit {
  public:
    LoadPCUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return UWORD(Cpu->GlobalPC + displacement);
    }
  };
  //
  // The LoadVector unit: This loads the contents of a fixed address (lo-byte) into the
  // operand. This is one of the processing steps of a BRK intruction or of an interrupt
  // processing step. It also or's the P register with a fixed value.
  template<UWORD vector,UBYTE statusmask>
  class LoadVectorUnit : public AtomicExecutionUnit {
  public:
    LoadVectorUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The LoadVectorExtendUnit: This is the high-byte extender of the LoadVector unit loading the high-byte of
  // the vector into the operand and then into the PC. This is the last step for interrupt
  // processing
  template<UWORD vector>
  class LoadVectorUnitExtend : public AtomicExecutionUnit {
  public:
    LoadVectorUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The BIT test unit. Implements the BIT instruction test: And the accumulator with
  // the operand, set Z bit. Set N,V from the bits 7,6 of the tested
  // operand.
  class BITUnit : public AtomicExecutionUnit {
  public:
    BITUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The TRB unit for the 65C02 "Test and Reset Bit" instruction.
  class TRBUnit : public AtomicExecutionUnit {
  public:
    TRBUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The TSB unit for the 65C02 "Test and Set Bit" instruction
  class TSBUnit : public AtomicExecutionUnit {
  public:
    TSBUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The RMB unit for the R65C02 "reset memory bit" instruction
  template<UBYTE bitmask>
  class RMBUnit : public AtomicExecutionUnit {
  public:
    RMBUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The SMB unit for the 65C02 "set memory bit" instruction
  template<UBYTE bitmask>
  class SMBUnit : public AtomicExecutionUnit {
  public:
    SMBUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ORA unit. This takes the operand, or's it into the accumulator
  // and stores the result in the accumulator. It also sets the condition
  // codes.
  class ORAUnit : public AtomicExecutionUnit {
  public:
    ORAUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  }; 
  //
  // The AND unit. This takes the operand, and's it into the accumulator
  // and stores the result in the accumulator. It also sets the condition
  // codes.
  class ANDUnit : public AtomicExecutionUnit {
  public:
    ANDUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The EOR unit. This takes the operand, eor's it into the accumulator
  // and stores the result in the accumulator. It also sets the condition
  // codes.
  class EORUnit : public AtomicExecutionUnit {
  public:
    EORUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The LSR unit. It takes the operand, shifts it right and sets the condition
  // codes. It does not touch the accumulator.
  class LSRUnit : public AtomicExecutionUnit {
  public:
    LSRUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The ASL unit. It takes the operand, shifts it left and sets the condition
  // codes. It does not touch the accumulator.
  class ASLUnit : public AtomicExecutionUnit {
  public:
    ASLUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The ROR unit. It takes the operand, rotates it right and sets the condition
  // codes. It does not touch the accumulator.
  class RORUnit : public AtomicExecutionUnit {
  public:
    RORUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The ROL unit. It takes the operand, rotates it left and sets the condition
  // codes. It does not touch the accumulator.
  class ROLUnit : public AtomicExecutionUnit {
  public:
    ROLUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ADC unit. It takes the operand, and adds it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register.
  class ADCUnit : public AtomicExecutionUnit {
  public:
    ADCUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ADC unit. It takes the operand, and adds it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register. This is the version that is used
  // for the western design center 65c02 which fixes some bugs of the
  // 6502 version.
  class ADCUnitFixed : public AtomicExecutionUnit {
    // The wait state we may insert here for the 65C02
    // in decimal mode.
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    ADCUnitFixed(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The SBC unit. It takes the operand, and subtracts it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register.
  class SBCUnit : public AtomicExecutionUnit { 
  public:
    SBCUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The SBC unit. It takes the operand, and subtracts it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register. This is the version that is used
  // for the western design center 65c02 which fixes some bugs of the
  // 6502 version.
  class SBCUnitFixed : public AtomicExecutionUnit { 
    // The wait state we may insert here for the 65C02
    // in decimal mode.
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    SBCUnitFixed(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The INC unit. It takes the argument and adds one to it. It does not
  // set the accumulator. It sets the Z and N condition codes, but does
  // not touch the C condition code.
  class INCUnit : public AtomicExecutionUnit {
  public:
    INCUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The DEC unit. It takes the argument and adds one to it. It does not
  // set the accumulator. It sets the Z and N condition codes, but does
  // not touch the C condition code.
  class DECUnit : public AtomicExecutionUnit {
  public:
    DECUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The CMP unit. It compares the A register with the operand and sets the
  // condition codes
  class CMPUnit : public AtomicExecutionUnit {
  public:
    CMPUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The CPX unit. It compares the X register with the operand and sets the
  // condition codes
  class CPXUnit : public AtomicExecutionUnit {
  public:
    CPXUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The CPY unit. It compares the Y register with the operand and sets the
  // condition codes
  class CPYUnit : public AtomicExecutionUnit {
  public:
    CPYUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The branch unit. This unit reads an displacement off the PC and jumps to a relative
  // location. It may add a wait state if a page boundary is crossed. This step should never
  // be inserted directly, but it should be part of the BranchDetectUnit step.
  class BranchUnit : public AtomicExecutionUnit {
    // The wait state we may insert here.
    T(CPU,Cat1)<WaitUnit> Wait;
  public:
    BranchUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The branch detect unit. It reads a displacement from the current PC and may or may
  // not branch. This is the first step of a two-step branch execution unit which
  // may or may not insert additional steps.
  template<UBYTE mask,UBYTE value>
  class BranchDetectUnit : public AtomicExecutionUnit {
    T(CPU,Cat1)<BranchUnit> Branch;
  public:
    BranchDetectUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Branch(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The BranchBitTestUnit, used for the Rockwell BBR/BBS instructions.
  template<UBYTE bitmask,UBYTE bitvalue>
  class BranchBitTestUnit : public AtomicExecutionUnit {
    T(CPU,Cat1)<BranchUnit> Branch;
  public:
    BranchBitTestUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu), Branch(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The Push unit. This unit takes its operand and pushes the lo-byte onto the stack.
  // This is one of the steps of PHA,PHP,BRK and JSR.
  class PushUnit : public AtomicExecutionUnit {
  public:
    PushUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Stack[UBYTE(Cpu->GlobalS--)] = UBYTE(operand);
      return operand;
    }
  };
  //
  // The push unit extender. This pushes the high-byte of the operand onto the stack.
  class PushUnitExtend : public AtomicExecutionUnit {
  public:
    PushUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Stack[UBYTE(Cpu->GlobalS--)] = UBYTE(operand >> 8);
      return operand;
    }
  };
  //
  // The Pull unit. This pulls one byte off the stack and inserts it as low-byte of the
  // operand.
  class PullUnit : public AtomicExecutionUnit {
  public:
    PullUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      return UWORD((operand & 0xff00) | Stack[UBYTE(++Cpu->GlobalS)]);
    }
  };  
  //
  // The Pull unit extender. This pulls the high-byte of the operand off the stack.
  class PullUnitExtend : public AtomicExecutionUnit {
  public:
    PullUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      return UWORD((operand & 0x00ff) | (UWORD(Stack[UBYTE(++Cpu->GlobalS)]) << 8));
    }
  };
  //
  // The get status unit: This reads the P register of the CPU and returns it as
  // opcode. This is a required step for the PHP and BRK/interrupt processing
  class GetStatusUnit : public AtomicExecutionUnit {
  public:
    GetStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return Cpu->GlobalP;
    }
  };
  //
  // The set status unit: Places the operand into the P register of the CPU.
  // This is a step of the PLP and RTI instruction sequence
  class SetStatusUnit : public AtomicExecutionUnit {
  public:
    SetStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->GlobalP = UBYTE(operand);
      return operand;
    }
  };
  //
  // The OrToStatus unit: This sets a specific bit in the P register
  // and returns the new contents of the register.
  template<UBYTE mask>
  class OrToStatusUnit : public AtomicExecutionUnit {
  public:
    OrToStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return Cpu->GlobalP |= mask;
    }
  };
  //
  // The AndToStatus unit: This clears a specific bit in the P register
  // and returns the new contents of the register
  template<UBYTE mask>
  class AndToStatusUnit : public AtomicExecutionUnit {
  public:
    AndToStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return Cpu->GlobalP &= mask;
    }
  };
  //
  // The get stack unit: This reads the S register of the CPU and returns it as
  // opcode. This is a required step for the TSX processing
  class GetStackUnit : public AtomicExecutionUnit {
  public:
    GetStackUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return Cpu->GlobalS;
    }
  };
  //
  // The set stack unit: Places the operand into the S register of the CPU.
  // This is a step of the TXS instruction sequence
  class SetStackUnit : public AtomicExecutionUnit {
  public:
    SetStackUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->GlobalS = UBYTE(operand);
      return operand;
    }
  };
  //
  // The Halt unit: This cycles the CPU until an interrupt (NMI or IRQ) is
  // detected.
  class HaltUnit : public AtomicExecutionUnit {
  public:
    HaltUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The JAM unit: If this is called, the monitor is entered because on the
  // real machine, instruction execution would come to a halt.
  template<UBYTE instruction>
  class JAMUnit : public AtomicExecutionUnit {
  public:
    JAMUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The unstable unit: If this is called, the monitor is entered because on the
  // real machine, instruction execution would be unreliable.
  template<UBYTE instruction>
  class UnstableUnit : public AtomicExecutionUnit {
  public:
    UnstableUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ESCUnit: This implements an ESC code mechanism that will run emulator
  // specific actions.
  class ESCUnit : public AtomicExecutionUnit {
  public:
    ESCUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //

  //
  // The instruction decoder: This is the last step in each execution sequence: It pulls
  // the next instruction off the PC and feeds its instruction sequence back into
  // the CPU class.
  class DecodeUnit : public AtomicExecutionUnit {
  public:
    DecodeUnit(class CPU *cpu)
      : AtomicExecutionUnit(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //  
  // The mask of available/free cycles. A one-byte in here indicates that
  // the cycle is not available.
  UBYTE StolenCycles[256];
  //
  // The list of the instructions of the current CPU. This is a service for
  // the disassembler.
  struct Instruction Disassembled[256];
  //
  // GNU C++ 2.95 workaround: Access point for the machine. Actually, substructures
  // should be able to read private data from here, but they aren't under 2.59 so
  // we have to provide this accessor.
  class Machine *Machine(void) const
  {
    return machine;
  }
  //
  // Build up all the instruction sequences for the CPU, namely the full
  // state machine of all instructions we have.
  void BuildInstructions(void);
  // Remove all instructions for the CPU, for reset and
  // coldstart.
  void ClearInstructions(void);
  //
  // Since the instruction builder is too LONG for some architectures
  // that would require here out-of-range branches, we split it up into
  // several groups.
  void BuildInstructions00(void);
  void BuildInstructions10(void);
  void BuildInstructions20(void);
  void BuildInstructions30(void);
  void BuildInstructions40(void);
  void BuildInstructions50(void);
  void BuildInstructions60(void);
  void BuildInstructions70(void);
  void BuildInstructions80(void);
  void BuildInstructions90(void);
  void BuildInstructionsA0(void);
  void BuildInstructionsB0(void);
  void BuildInstructionsC0(void);
  void BuildInstructionsD0(void);
  void BuildInstructionsE0(void);
  void BuildInstructionsF0(void);
  void BuildInstructionsExtra(void);
  //
  // Signal a horizontal blank for the CPU, reseting the cycles stolen so far
  virtual void HBI(void);
  //  
  //
#if CHECK_LEVEL > 0
  // The currently decoded instruction
  UBYTE LastIR;
#endif
public:
  //
  // constructors and destructors
  CPU(class Machine *mach);
  ~CPU(void);
  //
  // Steal cycles from the CPU. We need to supply the DMASlot definition for it.
  void StealCycles(const struct DMASlot &slot);
  //
  // Check whether the given cycle, starting at the left edge, is busy.
  UBYTE isBusy(int cycle) const
  {
    return StolenCycles[cycle];
  }
  //
  // The following are inherited from the Chip class
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //  
  // Read or set the internal status
  virtual void State(class SnapShot *);
  //
  // For the monitor: Return register contents
  UWORD &PC(void)
  {
    return GlobalPC;
  }
  UBYTE &A(void)
  {
    return GlobalA;
  }
  UBYTE &X(void)
  {
    return GlobalX;
  }
  UBYTE &Y(void)
  {
    return GlobalY;
  }
  UBYTE &S(void)
  {
    return GlobalS;
  }
  UBYTE &P(void)
  {
    return GlobalP;
  }
  //
  // A service for the disassembler: Return the disassembly information
  // for the given instruction, correctly for the currently emulated CPU.
  const struct Instruction &Disassemble(UBYTE instruction) const
  {
    return Disassembled[instruction];
  }
  //  
  // Perform a single step of the CPU emulation cycle. This is run by the
  // CPU driver to emulate a single cycle of the CPU.
  void Step(void)
  {
    //
    // Check whether we have a legal pointer here.
#if CHECK_LEVEL > 0
    if (size_t(CurCycle - StolenCycles) >= sizeof(StolenCycles)) {
      Throw(OutOfRange,"CPU::Step","execution HPOS out of range");
    }
#endif
    // Check whether there is a CPU slot available (and not stolen by DMA)
    // or blocked by WSync wait
    if (*CurCycle == 0) {
      class MicroCode *current = NextStep; // get the next step to be performed
      // Get already the step following this step such that the AEU may insert a step
      // between this and the next. This might be required to implement some data
      // dependent delay slots.
      NextStep = *ExecutionSteps;
      ExecutionSteps++;
#if CHECK_LEVEL > 0
      if (current == NULL) {
	machine->PutWarning("GlobalPC = %04x, IR = %02x\n",GlobalPC,LastIR);
	Throw(ObjectDoesntExist,"CPU::Step","no current execution step");
      }
#endif
      AtomicExecutionOperand = current->Execute(AtomicExecutionOperand);
    }
    // Bump the horizontal position.
    CurCycle++;
  }
  //
  // Go for an indicated number of cycles and return the
  // number of cycles left from it.
  int Go(int cycles);
  //
  // Enforce the CPU to synchronize to the next instruction fetch cycle.
  // This is required to be able to save the CPU state
  void Sync(void);
  //
  // Stop on a WSync write: This blocks the
  // CPU until the next WSYNC release position
  // (or the next line if we're over it)
  void WSyncStop(void);
  //
  // Check whether the CPU is currently halted (blocked) by means of
  // a still pending WSYNC.
  bool IsHalted(void) const
  {
    return Halted;
  }
  //
  // Return the current X position within a horizontal line.
  int CurrentXPos(void) const
  {
    return int(CurCycle - StolenCycles);
  }
  //
  // Generate a maskable interrupt, given a mask that identifies the source of the
  // IRQ.
  void GenerateIRQ(ULONG devicemask);
  //
  // Release the IRQ from a given device, identified by this mask.
  void ReleaseIRQ(ULONG devicemask);
  //
  // Generate a non-maskable interrupt
  void GenerateNMI(void);
  //
  // Miscellaneous breakpoint stuff.
  //
  // Install/set a new break point at an indicated position,
  // return the id or -1 if no break point is available.
  int SetBreakPoint(ADR where);
  void ClearBreakPoint(int bpk);
  void EnableBreakPoint(int bpk);
  void DisableBreakPoint(int bpk);
  // Test whether a breakpoint has been set
  // at the specified address
  bool IfBreakPoint(ADR where);
  //
  // Enable and disable tracing
  void EnableTrace(void);
  void DisableTrace(void);
  void EnableStack(void);
  void DisableStack(void);
  void EnablePC(void);
  void DisablePC(void);
  //
  // Parse off command line arguments
  virtual void ParseArgs(class ArgParser *args);
  //
  // Print the internal status of the CPU
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Returns true in case we emulate a 65C02 instead of a 6502
  bool is65C02(void) const
  {
    return Emulate65C02;
  }
};
///

///
#endif
    