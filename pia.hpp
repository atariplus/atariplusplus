/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: pia.hpp,v 1.16 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: PIA emulation module
 **********************************************************************************/

#ifndef PIA_HPP
#define PIA_HPP

/// Includes
#include "types.hpp"
#include "page.hpp"
#include "mmu.hpp"
#include "argparser.hpp"
#include "machine.hpp"
#include "chip.hpp"
#include "saveable.hpp"
#include "irqsource.hpp"
///

/// Forward declarations
class MMU;
class Monitor;
///

/// Class PIA
class PIA : public Chip, public Page, public Saveable, private IRQSource {
  // 
  // Link to the MMU for all the XL bank switching
  class MMU    *mmu;
  //
  // The following define the control registers of PIA
  UBYTE PortACtrl;
  UBYTE PortBCtrl;
  //
  // Port registers of PIA
  UBYTE PortA; 
  UBYTE PortB;
  //
  // Data direction registers of PIA
  UBYTE PortAMask;
  UBYTE PortBMask;
  //
  // CA2 and CB2 output states.
  bool  CA2State;
  bool  CB2State;
  //
  // Flags that store whether an active edge was detected by CA2 or CB2.
  bool  CA2LowEdge;
  bool  CA2HighEdge;
  bool  CB2Edge;
  //
  // The following flag is set if MathPackDisable is controlled by
  // bit 6 of PortB.
  bool controlmathpack;
  //
  // Reading and writing bytes to PIA
  virtual UBYTE ComplexRead(ADR mem);
  virtual void  ComplexWrite(ADR mem,UBYTE val);
  //
  // Private register access functions
  UBYTE PortARead(void);
  UBYTE PortBRead(void);
  UBYTE PortACtrlRead(void);
  UBYTE PortBCtrlRead(void);
  //
  void PortAWrite(UBYTE val);
  void PortBWrite(UBYTE val);
  void PortACtrlWrite(UBYTE val);
  void PortBCtrlWrite(UBYTE val);
  //
  //
  // Modify the setting and forward the changes to the MMU
  void ChangeMMUMapping(UBYTE portbits,UBYTE changedbits);
  //
public:
  PIA(class Machine *mach);
  ~PIA(void);
  //  
  // Coldstart and Warmstart PIA
  virtual void ColdStart(void);
  virtual void WarmStart(void);  
  //  
  // Read or set the internal status
  virtual void State(class SnapShot *);
  //
  // Parse off arguments
  virtual void ParseArgs(class ArgParser *args);
  //
  // Print the status of PIA
  virtual void DisplayStatus(class Monitor *mon);
};
///

///
#endif
