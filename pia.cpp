/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: pia.cpp,v 1.31 2013/05/31 22:08:00 thor Exp $
 **
 ** In this module: PIA emulation module
 **********************************************************************************/

/// Includes
#include "argparser.hpp"
#include "mmu.hpp"
#include "monitor.hpp"
#include "gamecontroller.hpp"
#include "pia.hpp"
#include "sio.hpp"
#include "exceptions.hpp"
#include "snapshot.hpp"
#include "ramextension.hpp"
///

/// PIA::PIA
// The constructor of the PIA class
PIA::PIA(class Machine *mach)
  : Chip(mach,"PIA"), Saveable(mach,"PIA"), IRQSource(mach),
    controlmathpack(false)
{ 
  // PIA interrupts: PIA itself can create an interrupt from state
  // changes at CA1,CA2,CB1 and CB2.
  // CA2 is connected by a transistor to the MOTOR output line and
  // hence cannot be used as input.
  // CA1 is directly connected to the PROCEED line of the SIO connector,
  // though it is unused by all peripherials I'm aware of.
  // CB1 is directly connected to the INTERRUPT line of the SIO connector,
  // which is also unused by all known peripherals.
  // CB2, finally, is the COMMAND output line for SIO. It can be used as
  // an input and then as an interrupt source, though there is no hardware
  // I'm aware of that makes use of this line as an interrupt source.
  //
  // We could trigger interrupts by selecting CA2 or CB2 as output,
  // enable interrupts there and then change the state of these
  // lines manually by program control. This is a tad stupid, but
  // it should be emulated.  
  CA2LowEdge  = false;
  CA2HighEdge = false;
  CB2Edge     = false;
  CA2State    = true;
  CB2State    = true;
}
///

/// PIA::~PIA
// The destructor
PIA::~PIA(void)
{ }
///

/// PIA::ChangeMMUMapping
// Read contents of port B and modify the MMU
// accordingly
void PIA::ChangeMMUMapping(UBYTE portbits,UBYTE changedbits)
{
  class RamExtension *ext;
  bool mapos,mapselftest;
  //
  switch(machine->MachType()) {
  case Mach_Atari800:
  case Mach_5200:    
    // no MMU here, PortB is used for something else!
    break;
  case Mach_AtariXE:
  case Mach_AtariXL:    
  case Mach_Atari1200:
    // Check for RAM extensions and forward the changes to them until
    // we find one that feels responsible. The XE extended RAM is one
    // kind of them and handled here.
    ext = mmu->FirstExtension();
    while(ext) {
      if (ext->PIAWrite(portbits))
	break;
      ext = ext->NextOf();
    }
    // Now for the classical bits.
    // Check for math pack disable.
    if ((changedbits & 0x40) && controlmathpack) {
      // Disable mathpack if bit 6 is zero
      mmu->SelectMathPack((portbits & 0x40)?true:false);
    }
    // Special trick: If selftest is enabled, but Os is disabled,
    // then enable the Os and disable the selftest. This will prevent
    // crashes on zero-writes into PIA (Tail of Beta Lyrae).
    // Enable the Os if bit 0 is on
    mapos       = (portbits & 0x01)?true:false;
    // Enable the self test if bit 7 is off (note the reverse logic)
    mapselftest = (portbits & 0x80)?false:true;
    //
    // Tested: If the os isn't mapped, the selftest isn't either.
    if (mapos == false) {
      mapselftest = false;
    }
    /*
    ** I've no idea what this should be good for, so
    ** disable it for the time being.
    **
    if (mapselftest && !mapos) {
      mapselftest = false;
      mapos       = true;
    }
    */
    if (changedbits & 0x81) {
      mmu->SelectXLOs(mapos);
    }
    if (changedbits & 0x02) {
      // Enable the basic if bit 1 is off (note the reverse logic)
      mmu->SelectXLBasic((portbits & 0x02)?false:true);
    }
    if (changedbits & 0x81) {
      mmu->SelectXLSelftest(mapselftest);
    }
    break;
  default:
    // This should not happen
    Throw(NotImplemented,"MMU:ChangeMMUMapping","unknown machine type");
  }
}
///

/// PIA::WarmStart
void PIA::WarmStart(void)
{
  PortA      = 0xff;
  PortB      = 0xff;
  PortACtrl  = 0x00;
  PortBCtrl  = 0x00;
  PortAMask  = 0x00; // all as input
  PortBMask  = 0x00;
  CA2LowEdge  = false;
  CA2HighEdge = false;
  CB2Edge     = false; 
  CA2State    = true;
  CB2State    = true;
  ChangeMMUMapping(0xff,0xff);
}
///

/// PIA::ColdStart
void PIA::ColdStart(void)
{
  mmu = machine->MMU();
  WarmStart(); // does the very same
}
///

/// PIA::PortARead
UBYTE PIA::PortARead(void)
{ 
  if ((PortACtrl & 0x04) == 0) {
    // Port A DDR access
    return PortAMask;
  } else {
    int stick0;
    int stick1;
    
    stick0  = machine->Joystick(0)->Stick();
    stick1  = machine->Joystick(1)->Stick();
    if (machine->Paddle(0)->Strig()) stick0 &= ~0x04;
    if (machine->Paddle(1)->Strig()) stick0 &= ~0x08;
    if (machine->Paddle(2)->Strig()) stick1 &= ~0x04;
    if (machine->Paddle(3)->Strig()) stick1 &= ~0x08;

    //
    // Reset port A interrupts
    PortACtrl &= 0x3f;
    DropIRQ();
    // Port A tries to output the bits in the output register,
    // leaves bits open that are not set, but the input may pull
    // these low because reading from port A reads the hardware
    // directly rather than the output buffer. This is different
    // for port B reading!
    
    return UBYTE(((PortA & (PortAMask)) | ~PortAMask) & ((stick0) | (stick1 << 4)));
  }
}
///

/// PIA::PortBRead
UBYTE PIA::PortBRead(void)
{ 
  if ((PortBCtrl & 0x04) == 0) {
    // Port A DDR access
    return PortBMask;
  } else {
    int stick2;
    int stick3;
    //
    // Reset port B interrupts
    PortBCtrl &= 0x3f;
    DropIRQ();
    
    switch(machine->MachType()) {
    case Mach_Atari800:
      stick2  = machine->Joystick(2)->Stick();
      stick3  = machine->Joystick(3)->Stick();
      if (machine->Paddle(0)->Strig()) stick2 &= ~0x04;
      if (machine->Paddle(1)->Strig()) stick2 &= ~0x08;
      if (machine->Paddle(2)->Strig()) stick3 &= ~0x04;
      if (machine->Paddle(3)->Strig()) stick3 &= ~0x08;
      
      return UBYTE(((PortB & (PortBMask)) | (((stick2) | (stick3 << 4)) & ~PortBMask)));
    case Mach_AtariXL:
    case Mach_AtariXE:
    case Mach_Atari1200:
      return UBYTE((PortB & (PortBMask)) | (~PortBMask));
    case Mach_5200: // The 5200 doesn't have a PIA.
      return 0xff;
    case Mach_None:
      // Shut up compiler
      Throw(NotImplemented,"PIA::PortBRead","Unknown machine type");
    }
    return 0; // shut up!
  }
}
///

/// PIA::PortACtrlRead
UBYTE PIA::PortACtrlRead(void)
{
  return PortACtrl;
}
///

/// PIA::PortBCtrlRead
UBYTE PIA::PortBCtrlRead(void)
{
  return PortBCtrl;
}
///

/// PIA::PortAWrite
void PIA::PortAWrite(UBYTE val)
{
  if ((PortACtrl & 0x04) == 0) {
    // DDR access here.
    PortAMask = val;
  } else {
    PortA     = val;
  }
}
///

/// PIA::PortBWrite
void PIA::PortBWrite(UBYTE val)
{
  // Get the effective PortB value. All input lines are read
  // as "high"
  UBYTE out     = UBYTE(PortB | (~PortBMask));
  //
  if ((PortBCtrl & 0x04) == 0) {
    // DDR access here.
    PortBMask  = val;
    // Now run into the following to get the changes from
    // DDR change forwarded to the PIA output. Input lines
    // go HI now
    val        = PortB;
  }
  {
    UBYTE newout  = UBYTE(val   | (~PortBMask));
    UBYTE changed = UBYTE(out^newout);
    // Data access here. May modify the MMU
    PortB         = val;
    // Must forward this to MMU now, depending on the
    // machine
    ChangeMMUMapping(newout,changed);
  }
}
///

/// PIA::PortACtrlWrite
void PIA::PortACtrlWrite(UBYTE val)
{ 
  //
  // Mask out the state of the interrupt flags
  PortACtrl    = (PortACtrl & 0xc0) | (val & 0x3f);
  //
  // Check for changes of the CA2 state.
  if (val & 0x20) {
    // Output mode for CA2.
    PortACtrl &= 0x3f; // Clear all interrupts.
    //
    switch(val & 0x18) {
    case 0x10: // output mode, set CA2 low.
      if (CA2State) { // high to low transition. Sets the trigger flag.
	CA2State   = false;
	CA2LowEdge = true;
	machine->SIO()->SetMotorLine(false);
      }
      break;
    case 0x18: // output mode, set CA2 high.
      if (!CA2State) { // low to high transition. Sets the trigger flag.
	CA2State    = true;
	CA2HighEdge = true;
	machine->SIO()->SetMotorLine(true);
      }
      break;
    case 0x08: // pulse output.
      CA2State    = true; // Keep it high, resets the trigger flag.
      CA2LowEdge  = false;
      CA2HighEdge = false;
      machine->SIO()->SetMotorLine(true);
      break;
    case 0x00: // handshake mode.
      break;
    }
  } else {
    // Input modes. CA2 edges set the IRQ flag.
    if (((PortACtrl & 0x10) && CA2LowEdge) || ((PortACtrl & 0x10) == 0 && CA2HighEdge)) {
      PortACtrl  |= 0x40;
    }
    CA2LowEdge  = false;
    CA2HighEdge = false;
    // CA2 on port A is latched, status comes from the latch, not the port.
  }
  //
  // Check whether IRQs are enabled.
  // If so, trigger now an interrupt.
  // Note that CA1 can never trigger an interrupt here since the input
  // is not under control of the CPU.
  if ((PortACtrl & 0x68) == 0x48) {
    // Input mode, interrupt pending and interrupt enabled.
    PullIRQ();
  } else {
    DropIRQ();
  }
}
///

/// PIA::PortBCtrlWrite
void PIA::PortBCtrlWrite(UBYTE val)
{ 
  //
  // Mask out the state of the interrupt flags
  PortBCtrl    = (PortBCtrl & 0xc0) | (val & 0x3f);
  //
  // Check for changes of the CB2 state.
  if (val & 0x20) {
    // Output mode for CB2.
    PortBCtrl &= 0x3f; // Clear all interrupts.
    //
    switch(val & 0x18) {
    case 0x10: // output mode, set CB2 low.
      if (CB2State) { // high to low transition. Resets the trigger flag.
	CB2State = false;
	CB2Edge  = false;
	// Set SIO command line.
	machine->SIO()->SetCommandLine(true);
      }
      break;
    case 0x18: // output mode, set CB2 high.
      if (!CB2State) { // low to high transition. Sets the trigger flag.
	CB2State = true;
	CB2Edge  = true;
	// Reset SIO command line.
	machine->SIO()->SetCommandLine(false);
      }
      break;
    case 0x08: // pulse output.
      CB2State = true; // Keep it high, resets the trigger flag.
      CB2Edge  = false;
      break;
    case 0x00: // handshake mode.
      break;
    }
  } else {
    // Input modes. CB2 edges set the IRQ flag.
    if (CB2Edge) {
      PortBCtrl |= 0x40;
      CB2Edge    = false;
    }
    CB2State = true; // Floating.
  }
  //
  // Check whether IRQs are enabled.
  // If so, trigger now an interrupt.
  // Note that CB1 can never trigger an interrupt here since the input
  // is not under control of the CPU.
  if ((PortBCtrl & 0x68) == 0x48) {
    // Input mode, interrupt pending and interrupt enabled.
    PullIRQ();
  } else {
    DropIRQ();
  }
}
///

/// PIA::ComplexRead
// Read a byte from PIA: Read dispatcher
UBYTE PIA::ComplexRead(ADR mem)
{
  switch(mem & 0x03) {
  case 0: // Port A read
    return PortARead();
  case 1:
    return PortBRead();
  case 2:
    return PortACtrlRead();
  case 3:
    return PortBCtrlRead();
  }
  // Shut up compiler;
  return 0;
}
///

/// PIA::ComplexWrite
// Write a byte into PIA: Write dispatcher
void PIA::ComplexWrite(ADR mem,UBYTE val)
{
  switch(mem & 0x03) {
  case 0:
    PortAWrite(val);
    break;
  case 1:
    PortBWrite(val);
    break;
  case 2:
    PortACtrlWrite(val);
    break;
  case 3:
    PortBCtrlWrite(val);
    break;
  }
}
///

/// PIA::ParseArgs
// Parse additional arguments relevant for PIA
void PIA::ParseArgs(class ArgParser *args)
{
  //
  // Check whether this is the real thing or just to give help
  args->DefineTitle("PIA");
  args->DefineBool("MathPackControl","enable control of MTE by PortB, bit 6",
		   controlmathpack);
}
///

/// PIA::DisplayStatus
// Display the status of the chip over the line
void PIA::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("PIA Status:\n"
		   "\tPortA     : %02x \tPortB     : %02x\n"
		   "\tPortACtrl : %02x \tPortBCtrl : %02x\n"
		   "\tPortADDR  : %02x \tPortBDDR  : %02x\n"
		   "\tConnect MathPackDisable by PortB, bit 6 : %s\n",
		   PortA,PortB,PortACtrl,PortBCtrl,PortAMask,PortBMask,
		   (controlmathpack)?("on"):("off")
		   );
}
///

/// PIA::State
// Read or set the internal status
void PIA::State(class SnapShot *sn)
{
  sn->DefineTitle("PIA");
  sn->DefineLong("PortACtrl","PIA port A control register",0x00,0xff,PortACtrl);
  sn->DefineLong("PortBCtrl","PIA port B control register",0x00,0xff,PortBCtrl);
  sn->DefineLong("PortA","PIA port A register contents",0x00,0xff,PortA);
  sn->DefineLong("PortB","PIA port B register contents",0x00,0xff,PortB);
  sn->DefineLong("PortADDR","PIA port A data direction mask",0x00,0xff,PortAMask);
  sn->DefineLong("PortBDDR","PIA port B data direction mask",0x00,0xff,PortBMask);
  //
  // We do not set the command line here. We leave the status of the command line as part
  // of the SIO state restoration.
  // We furthermore do not restore the MMU settings here as we leave this to the MMU
  // state machine.
}
///
