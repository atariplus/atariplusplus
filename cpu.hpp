/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: cpu.hpp,v 1.79 2021/08/16 10:31:01 thor Exp $
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
  // The register set of the CPU follows here.
  UWORD GlobalPC;   // program counter
  UBYTE GlobalA;    // Accumulator
  UBYTE GlobalX;    // X index register
  UBYTE GlobalY;    // Y index register
  UBYTE GlobalP;    // P processor status register
  UBYTE GlobalS;    // S stack pointer
  UWORD PreviousPC; // for watch point management: The PC of the instruction that caused the hit
  //
#ifdef HAS_MEMBER_INIT
  static const int NumBreakPoints = 8;
  static const int ClocksPerLine  = 114;
#else
#define            NumBreakPoints   8
#define            ClocksPerLine    114
#endif
  //
  // The following might be set by the monitor: It is a break point
  bool   EnableBreak;
  bool   EnableTracing;
  bool   EnableStacking;
  bool   EnableUntil;
  bool   TraceInterrupts;
  bool   EnableWatch;
  int    HitWatchPoint; // the watch point number we run into.
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
  // A pointer to the monitor 
  class Monitor       *monitor;
  //
  // A pointer to the RAM we need for faster reference
  class AdrSpace      *Ram;
  //
  // The debug RAM which implements watch points.
  class DebugAdrSpace *DebugRam;
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
  // If a halt is carried over to the next line, this is the number of
  // cycles to remain clear of at its beginning.
  UBYTE HaltStart;
  //
  // This flag is set if an IRQ is pending, but not yet served.
  bool IRQPending;
  //
  // Pointer to the current DMA slot.
  UBYTE *CurCycle;
  //
  // Pointer to the last available cycle.
  UBYTE *LastCycle;
  //
  // In case this is non-NULL, it is a set of counters that is used for
  // profiling code. Counters are increased as soon as the CPU
  // reaches a given PC. There is one counter per memory location.
  ULONG *ProfilingCounters;
  //
  // If this is non-NULL, it points to profiling counters that accumulate
  // over subroutine calls.
  ULONG *CumulativeCounters;
  //
  // Current CPU cycle counter. This counts the number of cycles to the next
  // cycle reset and is used to control the sound output.
  ULONG  CycleCounter;
  //
  // Profile counter, counts steps for the profiler.
  ULONG  ProfileCounter;
  //
  // CPU preferences
  LONG   WSyncPosition; // horizontal position of the WSync release slot. Defaults to 104.
  //
  // Set this to enable tracing on a reset
  bool   TraceOnReset;
  //
  // Set this to emulate a WDC 65C02 instead.
  bool   Emulate65C02;
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
  template<class Adr>
  class AtomicExecutionUnit {
  protected:
    // The address space we have to read from and write to
    Adr            *const Ram;
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
    //
    // The stop mask. Defines on which conditions this
    // code can be halted. If 0x01, only on HALT, if 0x03, also on RDY.
    UBYTE m_ucStopMask;
    //
  public:
    // Constructor: The argument is true (which should be the default)
    // if the code should also stop on an RDY signal. The NMOS 6502
    // does not do this on memory writes.
    MicroCode(bool haltonrdy)
      : m_ucStopMask((haltonrdy)?(0x03):(0x01))
    { }
    //
    virtual ~MicroCode(void)
    { }
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
    //
    // The stop mask. Can be 0x03 to stop on RDY (WSYNC) and HALT (DMA) or
    // 0x01 to stop on HALT only. The latter goes for write instructions.
    UBYTE StopMask(void) const
    {
      return m_ucStopMask;
    }
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
    Cat2(class CPU *cpu, bool haltonrdy = true)
      : MicroCode(haltonrdy), First(cpu), Second(cpu)
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
    Cat3(class CPU *cpu,bool haltonrdy = true)
      : MicroCode(haltonrdy), First(cpu), Second(cpu), Third(cpu)
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
    Cat1(class CPU *cpu,bool haltonrdy = true)
      : MicroCode(haltonrdy), First(cpu)
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
    class MicroCode *Sequence[9]; // There are no instructions with more than eight cycles
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
  class WaitUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    WaitUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class ImmediateUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ImmediateUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class ImmediateWordExtensionUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ImmediateWordExtensionUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class AddXUnitWait : public AtomicExecutionUnit<class AdrSpace> {
    Cat1<WaitUnit> Wait;
  public:
    AddXUnitWait(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
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
  // This is a variant of the above unit except that the result is
  // always truncated to the zero page and no wait state is inserted
  class AddXUnitZero : public AtomicExecutionUnit<class AdrSpace> {
  public:
    AddXUnitZero(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {  
      return UWORD(operand + Cpu->GlobalX) & 0xff;
    }
  };   
  //
  // The same as above, except that a wait slot is only added if a page
  // boundary is crossed. This is the second step in an absolute,X addressing
  // step.
  class AddXUnit : public AtomicExecutionUnit<class AdrSpace> {
    Cat1<WaitUnit> Wait;
  public:
    AddXUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
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
  class AddYUnitWait : public AtomicExecutionUnit<class AdrSpace> {
    Cat1<WaitUnit> Wait;
  public:
    AddYUnitWait(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
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
  // This is a variant of the above unit except that the result is
  // always truncated to the zero page and no wait state is inserted
  class AddYUnitZero : public AtomicExecutionUnit<class AdrSpace> {
  public:
    AddYUnitZero(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {  
      return UWORD(operand + Cpu->GlobalY) & 0xff;
    }
  };
  //
  // The AddY unit takes its operand and adds the Y register to it, returning
  // its operand. This is the thrid step in an (indirect),y addressing mode
  // and *may* add a wait state.
  class AddYUnit : public AtomicExecutionUnit<class AdrSpace> {
    Cat1<WaitUnit> Wait;
  public:
    AddYUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
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
  template<class Adr>
  class IndirectionUnit : public AtomicExecutionUnit<Adr> {
  public:
    IndirectionUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      // Keep the effective address in the CPU for a possible last store
      this->Cpu->EffectiveAddress = operand;
      return UWORD(this->Ram->ReadByte(operand));
    }
  };
  //
  // The IndirectionUnitExtend extends the indirection unit by supplying a high-byte.
  // This is only required for the JMP (indirect) instruction, and for
  // that reason, we emulate a 6502 bug: It forgets to carry over to
  // the high-byte.
  class IndirectionUnitExtend : public AtomicExecutionUnit<class AdrSpace> {
  public:
    IndirectionUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class IndirectionUnitExtendFixed : public AtomicExecutionUnit<class AdrSpace> {
    // Used to create a wait-state
    Cat1<WaitUnit> Wait;
  public:
    IndirectionUnitExtendFixed(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
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
  // last step in a ZPage, ZPage,X or ZPage,Y addressing. Note that this
  // always remains in the ZPage.
  template<class Adr>
  class ZPageIndirectionUnit : public AtomicExecutionUnit<Adr> {
  public:
    ZPageIndirectionUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      operand               = UBYTE(operand);
      // Keep the effective address in the CPU for a possible last store
      this->Cpu->EffectiveAddress = operand;
      if (sizeof(Adr) == sizeof(AdrSpace))
	return this->ZPage[operand];
      else
	return this->Ram->ReadByte(operand);
    }
  };
  //
  // This one works like the above, but accesses to bytes for indirect addressing
  // like (ZPage,X) and (ZPage),Y.
  template<class Adr>
  class ZPageWordIndirectionUnit : public AtomicExecutionUnit<Adr> {
  public:
    ZPageWordIndirectionUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      operand               = UBYTE(operand);
      // Keep the effective address in the CPU for a possible last store
      this->Cpu->EffectiveAddress = operand;
      if (sizeof(Adr) == sizeof(AdrSpace))
	return UWORD(this->ZPage[operand] | 
		     (UWORD(this->ZPage[UBYTE(operand + 1)]) << 8));
      else
	return UWORD(this->Ram->ReadByte(operand)) | 
	  (UWORD(this->Ram->ReadByte(UBYTE(operand+1))) << 8);
    }
  };  
  //
  // The IndirectWriter unit is a helper unit that requires an EA from outside
  // to perform its operation.
  template<class Adr>
  class IndirectWriterUnit : public AtomicExecutionUnit<Adr> {
  public:
    IndirectWriterUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      // The WSYNC checking could happen here by testing for
      // the result code of the WriteByte position. Though
      // this is not needed here, Antic stops the CPU now.
      this->Ram->WriteByte(this->Cpu->EffectiveAddress,
					       UBYTE(operand));
      return operand;
    }
  };
  //
  // The ZPageIndirectWriter unit works as above, except that it truncates the
  // EA to the zero page. This is limited to the ZPage.
  template<class Adr>
  class ZPageIndirectWriterUnit : public AtomicExecutionUnit<Adr> {
  public:
    ZPageIndirectWriterUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {  
      if (sizeof(Adr) == sizeof(AdrSpace)) {
	this->ZPage[UBYTE(this->Cpu->EffectiveAddress)] = UBYTE(operand);
      } else {
	this->Ram->WriteByte(UBYTE(this->Cpu->EffectiveAddress),UBYTE(operand));
      }
      return operand;
    }
  };
  //
  // The load accumulator unit. This takes the operand and stores it in the
  // accumulator. This is the main operation sequence for an LDA instruction
  // It also sets the condition codes.
  class LDAUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LDAUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The accumulator EA unit returns the contents of the accumulator for the A
  // addressing modes.
  // It also writes the operand into the EA for store instruction
  class AccuUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    AccuUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class LDXUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LDXUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The X unit returns the contents of the X index register, used for STX and TXA
  // It also writes the operand into the EA for store instruction
  class XUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    XUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class ANXUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ANXUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class LDYUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LDYUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //  
  // The Y unit returns the contents of the Y index register, used for STY and TYA
  // It also writes the operand into the EA for store instruction
  class YUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    YUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class ZeroUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ZeroUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class JMPUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    JMPUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class LoadPCUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LoadPCUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class LoadVectorUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LoadVectorUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //  
  // The LoadVector unit: This loads the contents of a fixed address (lo-byte) into the
  // operand. This is one of the processing steps of a BRK intruction or of an interrupt
  // processing step. It also or's the P register with a fixed value.
  // This is the quirky version which runs an alterantive vector if an NMI is there.
  template<UWORD vector,UWORD alternative,UBYTE statusmask>
  class LoadVectorUnitQuirk : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LoadVectorUnitQuirk(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The LoadVectorExtendUnit: 
  // This is the high-byte extender of the LoadVector unit loading the high-byte of
  // the vector into the operand and then into the PC. This is the last step for interrupt
  // processing
  template<UWORD vector>
  class LoadVectorUnitExtend : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LoadVectorUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The BIT test unit. Implements the BIT instruction test: And the accumulator with
  // the operand, set Z bit. Set N,V from the bits 7,6 of the tested
  // operand.
  class BITUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    BITUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The extra instructions wierd BIT test unit. Sets C
  // from bit 6 and V as the exor of 5 and 6.
  class BITWierdUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    BITWierdUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // Another wierd unit for extra instructions:
  // AND the Y unit with the high-byte of the operand+1,
  // add the X register to the operand, store as effective address,
  // then return the result of the AND.
  class AndHiPlusOneYAddXUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    AndHiPlusOneYAddXUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // Another wierd unit for extra instructions:
  // AND the X unit with the high-byte of the operand+1,
  // add the Y register to the operand, store as effective address,
  // then return the result of the AND.
  class AndHiPlusOneXAddYUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    AndHiPlusOneXAddYUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //  
  // The TRB unit for the 65C02 "Test and Reset Bit" instruction.
  class TRBUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    TRBUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The TSB unit for the 65C02 "Test and Set Bit" instruction
  class TSBUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    TSBUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The RMB unit for the R65C02 "reset memory bit" instruction
  template<UBYTE bitmask>
  class RMBUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    RMBUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The SMB unit for the 65C02 "set memory bit" instruction
  template<UBYTE bitmask>
  class SMBUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    SMBUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ORA unit. This takes the operand, or's it into the accumulator
  // and stores the result in the accumulator. It also sets the condition
  // codes.
  class ORAUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ORAUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  }; 
  //
  // The AND unit. This takes the operand, and's it into the accumulator
  // and stores the result in the accumulator. It also sets the condition
  // codes.
  class ANDUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ANDUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The EOR unit. This takes the operand, eor's it into the accumulator
  // and stores the result in the accumulator. It also sets the condition
  // codes.
  class EORUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    EORUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The LSR unit. It takes the operand, shifts it right and sets the condition
  // codes. It does not touch the accumulator.
  class LSRUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    LSRUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The ASL unit. It takes the operand, shifts it left and sets the condition
  // codes. It does not touch the accumulator.
  class ASLUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ASLUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The ROR unit. It takes the operand, rotates it right and sets the condition
  // codes. It does not touch the accumulator.
  class RORUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    RORUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The ROL unit. It takes the operand, rotates it left and sets the condition
  // codes. It does not touch the accumulator.
  class ROLUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ROLUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ADC unit. It takes the operand, and adds it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register.
  class ADCUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ADCUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ADC unit. It takes the operand, and adds it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register. This is the version that is used
  // for the western design center 65c02 which fixes some bugs of the
  // 6502 version, mostly on BCD arithmetics.
  class ADCUnitFixed : public AtomicExecutionUnit<class AdrSpace> {
    // The wait state we may insert here for the 65C02
    // in decimal mode.
    Cat1<WaitUnit> Wait;
  public:
    ADCUnitFixed(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The SBC unit. It takes the operand, and subtracts it with carry to the
  // accumulator. It also sets the condition codes, and keeps care of the
  // decimal flag in the P register.
  class SBCUnit : public AtomicExecutionUnit<class AdrSpace> { 
  public:
    SBCUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class SBCUnitFixed : public AtomicExecutionUnit<class AdrSpace> { 
    // The wait state we may insert here for the 65C02
    // in decimal mode.
    Cat1<WaitUnit> Wait;
  public:
    SBCUnitFixed(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The INC unit. It takes the argument and adds one to it. It does not
  // set the accumulator. It sets the Z and N condition codes, but does
  // not touch the C condition code.
  class INCUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    INCUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };  
  //
  // The DEC unit. It takes the argument and adds one to it. It does not
  // set the accumulator. It sets the Z and N condition codes, but does
  // not touch the C condition code.
  class DECUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    DECUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The CMP unit. It compares the A register with the operand and sets the
  // condition codes
  class CMPUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    CMPUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The CPX unit. It compares the X register with the operand and sets the
  // condition codes
  class CPXUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    CPXUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The CPY unit. It compares the Y register with the operand and sets the
  // condition codes
  class CPYUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    CPYUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The branch unit. This unit reads an displacement off the PC and jumps to a relative
  // location. It may add a wait state if a page boundary is crossed. This step should never
  // be inserted directly, but it should be part of the BranchDetectUnit step.
  class BranchUnit : public AtomicExecutionUnit<class AdrSpace> {
    // The wait state we may insert here.
    Cat1<WaitUnit> Wait;
  public:
    BranchUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The branch detect unit. It reads a displacement from the current PC and may or may
  // not branch. This is the first step of a two-step branch execution unit which
  // may or may not insert additional steps.
  template<UBYTE mask,UBYTE value>
  class BranchDetectUnit : public AtomicExecutionUnit<class AdrSpace> {
    Cat1<BranchUnit> Branch;
  public:
    BranchDetectUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Branch(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The BranchBitTestUnit, used for the Rockwell BBR/BBS instructions.
  template<UBYTE bitmask,UBYTE bitvalue>
  class BranchBitTestUnit : public AtomicExecutionUnit<class AdrSpace> {
    Cat1<BranchUnit> Branch;
    Cat1<WaitUnit>   Wait;
  public:
    BranchBitTestUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu), Branch(cpu), Wait(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The Push unit. This unit takes its operand and pushes the lo-byte onto the stack.
  // This is one of the steps of PHA,PHP,BRK and JSR.
  template<class Adr>
  class PushUnit : public AtomicExecutionUnit<Adr> {
  public:
    PushUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      if (sizeof(Adr) == sizeof(AdrSpace)) {
	this->Stack[UBYTE(this->Cpu->GlobalS--)] = 
	  UBYTE(operand);
      } else {
	this->Ram->WriteByte(0x100 + UBYTE(this->Cpu->GlobalS--),UBYTE(operand));
      }
      return operand;
    }
  };
  //
  // The push unit extender. This pushes the high-byte of the operand onto the stack.
  template<class Adr>
  class PushUnitExtend : public AtomicExecutionUnit<Adr> {
  public:
    PushUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      if (sizeof(Adr) == sizeof(AdrSpace)) {
	this->Stack[UBYTE(this->Cpu->GlobalS--)] = 
	  UBYTE(operand >> 8);
      } else {
	this->Ram->WriteByte(0x100 + UBYTE(this->Cpu->GlobalS--),UBYTE(operand >> 8));
      }
      return operand;
    }
  };
  //
  // The Pull unit. This pulls one byte off the stack and inserts it as low-byte of the
  // operand.
  template<class Adr>
  class PullUnit : public AtomicExecutionUnit<Adr> {
  public:
    PullUnit(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      if (sizeof(Adr) == sizeof(AdrSpace)) {
	return UWORD((operand & 0xff00) | this->Stack[UBYTE(++this->Cpu->GlobalS)]);
      } else {	
	return UWORD((operand & 0xff00) | 
		     this->Ram->ReadByte(0x100 + UBYTE(++this->Cpu->GlobalS)));
      }
    }
  };
  //
  // The Pull unit extender. This pulls the high-byte of the operand off the stack.
  template<class Adr>
  class PullUnitExtend : public AtomicExecutionUnit<Adr> {
  public:
    PullUnitExtend(class CPU *cpu)
      : AtomicExecutionUnit<Adr>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      if (sizeof(Adr) == sizeof(AdrSpace)) {
	return UWORD((operand & 0x00ff) | (UWORD(this->Stack[UBYTE(++this->Cpu->GlobalS)]) << 8));
      } else {
	return UWORD((operand & 0x00ff) | (UWORD(this->Ram->ReadByte(0x100 + UBYTE(++this->Cpu->GlobalS))) << 8));
      }
    }
  };
  //
  // The get status unit: This reads the P register of the CPU and returns it as
  // opcode. This is a required step for the PHP and BRK/interrupt processing
  class GetStatusUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    GetStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class SetStatusUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    SetStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->GlobalP = UBYTE(operand);
      //
      // This checks the IRQ right now.
      if (Cpu->IRQMask && ((Cpu->GlobalP & I_Mask) == 0)) {
	// Delay for the next instruction - wierdo - but not here.
	Cpu->IRQPending   = true;
      }
      return operand;
    }
  };
  //
  // The OrToStatus unit: This sets a specific bit in the P register
  // and returns the new contents of the register.
  template<UBYTE mask>
  class OrToStatusUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    OrToStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class AndToStatusUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    AndToStatusUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    inline UWORD Execute(UWORD)
    {
      return Cpu->GlobalP &= mask;
    }
  };
  //
  // Make a copy of a status bit. This is actually a kludge and is the only
  // operation left from the ROL unit that is masked by the AND unit.
  class CopyNToCUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    CopyNToCUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    inline UWORD Execute(UWORD operand)
    {
      Cpu->GlobalP = (Cpu->GlobalP & 0xfe) | (Cpu->GlobalP >> 7);
      return operand;
    }
  };
  //
  // The get stack unit: This reads the S register of the CPU and returns it as
  // opcode. This is a required step for the TSX processing
  class GetStackUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    GetStackUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class SetStackUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    SetStackUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  class HaltUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    HaltUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The JAM unit: If this is called, the monitor is entered because on the
  // real machine, instruction execution would come to a halt.
  template<UBYTE instruction>
  class JAMUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    JAMUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The unstable unit: If this is called, the monitor is entered because on the
  // real machine, instruction execution would be unreliable.
  template<UBYTE instruction>
  class UnstableUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    UnstableUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // The ESCUnit: This implements an ESC code mechanism that will run emulator
  // specific actions.
  class ESCUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    ESCUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand);
  };
  //
  // Another wierdo: Processing an IRQ while an NMI comes in disables the NMI.
  class NMIResetUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    NMIResetUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
    { }
    //
    UWORD Execute(UWORD operand)
    {
      Cpu->NMI = false;
      
      return operand;
    }
  };
  //
  // The instruction decoder: This is the last step in each execution sequence: It pulls
  // the next instruction off the PC and feeds its instruction sequence back into
  // the CPU class.
  class DecodeUnit : public AtomicExecutionUnit<class AdrSpace> {
  public:
    DecodeUnit(class CPU *cpu)
      : AtomicExecutionUnit<AdrSpace>(cpu)
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
  // state machine of all instructions we have. This requires a flag
  // whether memory watchpoints are enabled. If so, an enhanced memory interface
  // is used, namely the DebugAdrSpace instead of the standard AdrSpace.
  void BuildInstructions(bool watchpoints);
  // Remove all instructions for the CPU, for reset and
  // coldstart.
  void ClearInstructions(void);
  //
  // Run the microcode decoder, fetch the next instruction.
  // This is here and not in an atomic step because the monitor
  // could replace the latter on the fly when inserting
  // watch-points.
  UWORD DecodeInstruction(void);
  //
  // Since the instruction builder is too LONG for some architectures
  // that would require here out-of-range branches, we split it up into
  // several groups.
  template <class Adr>
  void BuildInstructions00(void);
  template <class Adr>
  void BuildInstructions10(void);
  template <class Adr>
  void BuildInstructions20(void);
  template <class Adr>
  void BuildInstructions30(void);
  template <class Adr>
  void BuildInstructions40(void);
  template <class Adr>
  void BuildInstructions50(void);
  template <class Adr>
  void BuildInstructions60(void);
  template <class Adr>
  void BuildInstructions70(void);
  template <class Adr>
  void BuildInstructions80(void);
  template <class Adr>
  void BuildInstructions90(void);
  template <class Adr>
  void BuildInstructionsA0(void);
  template <class Adr>
  void BuildInstructionsB0(void);
  template <class Adr>
  void BuildInstructionsC0(void);
  template <class Adr>
  void BuildInstructionsD0(void);
  template <class Adr>
  void BuildInstructionsE0(void);
  template <class Adr>
  void BuildInstructionsF0(void);
  template <class Adr>
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
  // Steal DMA cycles with two cycles elasticity, used for memory refresh.
  // If no cycle is available whatsoever, use a cycle cycle at the last slot.
  void StealMemCycles(const struct DMASlot &slot);
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
    class MicroCode *current = NextStep; // get the next step to be performed
    //
    // Check whether we have a legal pointer here.
#if CHECK_LEVEL > 0
    if (size_t(CurCycle - StolenCycles) >= sizeof(StolenCycles)) {
      Throw(OutOfRange,"CPU::Step","execution HPOS out of range");
    }
    if (current == NULL) {
      machine->PutWarning("GlobalPC = %04x, IR = %02x\n",GlobalPC,LastIR);
      Throw(ObjectDoesntExist,"CPU::Step","no current execution step");
    }
#endif
    //
    // Check whether there is a CPU slot available (and not stolen by DMA)
    // or blocked by WSync wait
    if ((*CurCycle & current->StopMask()) == 0) {
      // Get already the step following this step such that the AEU may insert a step
      // between this and the next. This might be required to implement some data
      // dependent delay slots.
      NextStep = *ExecutionSteps;
      ExecutionSteps++;
      AtomicExecutionOperand = current->Execute(AtomicExecutionOperand);
    }
    // Bump the horizontal position.
    CurCycle++;
    //CurCycle - StolenCycles
    //
    // Advance the rest of the hardware by a single cycle
    if (CurCycle <= LastCycle) {
      CycleCounter++;
      // Bump the profile counter
      ProfileCounter++;
      machine->Step();
    }
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
    return HaltStart < WSyncPosition;
  }
  //
  // Return the current X position within a horizontal line.
  int CurrentXPos(void) const
  {
    return int(CurCycle - StolenCycles);
  }
  //
  // Return the number of cycles since the last time the cycle counter was read,
  // and reset it.
  ULONG ElapsedCycles(void)
  {
    ULONG cnt = CycleCounter;
    
    CycleCounter = 0;

    return cnt;
  }
  //
  // Generate a maskable interrupt, given a mask that identifies the source of the
  // IRQ.
  void GenerateIRQ(ULONG devicemask)
  {
    IRQMask  |= devicemask;
  }
  //
  // Release the IRQ from a given device, identified by this mask.
  void ReleaseIRQ(ULONG devicemask)
  {
    IRQMask &= ~devicemask;
  }
  //
  // Generate a non-maskable interrupt
  void GenerateNMI(void)
  {
    NMI = true; // that's all!
  }
  //
  // Generate a watch point interrupt.
  void GenerateWatchPoint(UBYTE idx)
  {
    HitWatchPoint = idx;
  }
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
  // Enable or disable watch points.
  void EnableWatchPoints(void);
  void DisableWatchPoints(void);
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
  //
  // Start profiling CPU execution.
  void StartProfiling(void);
  //
  // Stop profiling CPU execution.
  void StopProfiling(void);
  //
  // Return the profile conters, one per possible PC location.
  // Returns NULL in case profiling is disabled.
  const ULONG *ProfilingCountersOf(void) const
  {
    return ProfilingCounters;
  }
  //
  // Return the cumulative profiling counters that accumulate
  // over subroutine calls.
  const ULONG *CumulativeProfilingCountersOf(void) const
  {
    return CumulativeCounters;
  }
};
///

///
#endif
    
