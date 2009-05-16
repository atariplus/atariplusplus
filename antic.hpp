/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: antic.hpp,v 1.57 2008-03-21 21:21:47 thor Exp $
 **
 ** In this module: Antic graphics emulation
 **
 ** CREDIT NOTES:
 ** This code is influenced by David Firth's original Atari800 (v 0.86) 
 ** emulator Antic core, specifically the antic mode line emulation core shares 
 ** some similarities in its build-up strategy. Currently, enhanced lookup mechanisms 
 ** are used within the core that differ from David's initial code, and the 
 ** control structure of the Antic modeline construction is also quite different.
 ** Specifically, this code here is scanline oriented, whereas the original
 ** v0.86 code was modeline based. This code here rather reads the screen data
 ** from an antic internal scanline buffer that is pre-fetched before the mode
 ** gets build up - similar to the real hardware - and provides its processed out-
 ** put on a line-by-line basis to the GTIA graphics postprocessor, which also
 ** implements the antic "delay line" for horizontal scrolling by means of 
 ** pointer offsets and a windowing technique.
 **********************************************************************************/

#ifndef ANTIC_HPP
#define ANTIC_HPP

/// Includes
#include "stdlib.hpp"
#include "types.hpp"
#include "page.hpp"
#include "machine.hpp"
#include "chip.hpp"
#include "adrspace.hpp"
#include "hbiaction.hpp"
#include "argparser.hpp"
#include "saveable.hpp"
#include "cpu.hpp"
///

/// Forward declarations
class Monitor;
class GTIA;
///

/// Class Antic
// This class describes the atari Display DMA controller, ANTIC
class Antic : public Chip, public Page, public Saveable, private HBIAction {
  // Lots and lots of private data
  //
public:
  // We displace data fill-in by these many half-color clocks for
  // convenient horizontal scrolling
  static const int FillInOffset        INIT(32);
  // Another scrolling offset required to fill-in PM graphics conventiently
  static const int PlayerMissileOffset INIT(64);
  // The visible width of the Atari display
  static const int DisplayWidth        INIT(384);
  // The total modulo from one row to another
  static const int DisplayModulo       INIT(DisplayWidth + FillInOffset + PlayerMissileOffset);
  // The first generated scan line
  static const int DisplayStart        INIT(8);
  // The total height of the display in rows. 
  static const int DisplayHeight       INIT(249);
  // Lines up to the start of the VBI
  static const int VBIStart            INIT(248);
  // Total lines in an NTSC display
  static const int NTSCTotal           INIT(262);
  // Total lines in an PAL display (312)
  static const int PALTotal            INIT(312);
  // Total number of lines visible in a window.
  static const int WindowHeight        INIT(DisplayHeight - DisplayStart - 1);
  // Total number of rows visible.
  static const int WindowWidth         INIT(DisplayWidth - 32);
#ifndef HAS_PRIVATE_ACCESS
public:
#endif
  //
  // DMA slot allocation models for various ANTIC DMA activities:
  // Pre-allocated DMA slots for memory refresh
  static const struct CPU::DMASlot MemRefreshSlot;
  // Pre-allocated DMA slots for display list fetch
  static const struct CPU::DMASlot DListFetchSlot;
  // Pre-allocated DMA slots for DL-LoadScanPointer fetch
  static const struct CPU::DMASlot DLScanFetchSlot;
  // Pre-allocated DMA slots for player graphics
  static const struct CPU::DMASlot PlayerFetchSlot;
  // Pre-allocated DMA slots for missile graphics
  static const struct CPU::DMASlot MissileFetchSlot;
  // Pre-allocated DMA slots for playfield graphics requiring at most 12 bytes data
  static const UBYTE Playfield12Fetch[103];
  // Pre-allocated DMA slots for playfield graphics requiring at most 24 bytes data
  static const UBYTE Playfield24Fetch[103];
  // Pre-allocated DMA slots for playfield graphics requiring at most 48 bytes data
  static const UBYTE Playfield48Fetch[103];
  //
#ifdef HAS_PRIVATE_ACCESS
private:
#endif
  //
  // Pointer to the CPU for the computations
  class CPU        *Cpu;
  //
  // Pointer to the address space antic uses for fetching memory.
  // This is *not* the CPU address space.
  class AdrSpace   *Ram;
  //
  // Pointer to GTIA for scanline activity
  class GTIA       *Gtia;
  //
  // The following variables describe the left and right border of the
  // display DMA, depending on whether horizontal scrolling is enabled
  // or not. This is measured in half color clocks
  int XMinNoScroll,XMaxNoScroll;
  int XMinScroll,XMaxScroll;
  int DMAWidthNoScroll;// Width of the display as base for the DMAShift of the mode lines
  int DMAWidthScroll;
  int FirstDMACycleNoScroll; // first cycle on screen that will require DMA slots w/o scrolling
  int LastDMACycleNoScroll;  // last cycle on screen that will require DMA slots w/o scrolling
  int FirstDMACycleScroll;
  int LastDMACycleScroll;
  //
  // CPU cycles stolen for player/missile DMA
  int PMDMACycles;
  //
  // The following mini-structure describes the character generator.
  // We have two of them, a 20 characters/row and a 40 characters/row generator.
  // We must supply both as the character bases are different. The 40Char modes
  // ignore more bits for the base generation.
public:
  struct CharacterGenerator {
    // Character generator: Orientation in vertical direction: +1 or -1
    class AdrSpace *Ram;          // where to fetch characters from
    bool            UpsideDown;   // if true, then characters are displayed upside down
    int             InvertMask;   // if code & this is non-null, then it gets inverted
    int             BlankMask;    // if code & this is non-null, then it gets blanked out
    ADR             CharBase;     // Character base
  } Char20,Char40;
private:
  //
  // Player/Missile DMA generator: Keeps bases for the PM graphics.
  // We have two of them: One for low resultion, another for hi-resolution
  // and a thrid pointer to the currently active one.
  struct PlayerMissileGenerator {
    class AdrSpace *Ram;            // where to fetch PM graphics from
    ADR             MissileBase;    // where Missile graphics come from
    ADR             PlayerBase[4];  // where Player DMA graphics come from
    int             YPosShift;      // bit displacement for YPos to DMA pos: 0 or 1
  } PMGeneratorLow,PMGeneratorHigh,*PMActive;
  //
  ADR   AnticPC;      // Antic's program counter for the display list
  ADR   AnticPCShadow;// Shadow register for debugging purposes (not used otherwise)
  ADR   AnticPCCur;   // Shadow register for debugging purposes (not used otherwise)
  ADR   PMBase;       // Base address for Player/Missile DMA
  ADR   PFBase;       // Base address for Playfield DMA
  ADR   ChBase;       // Shadow register for the Character DMA
  int   YPos;         // Current display Y position
  UBYTE NMIEnable;    // NMI masking/enable register
  UBYTE NMIStat;      // NMI status register
  UBYTE DMACtrl;      // DMA control register
  UBYTE DMACtrlShadow;// DMA control register, updated at the end of the scanline
  UBYTE CharCtrl;     // Character control register shaddow of the character generator
  UBYTE HScroll;      // Horizontal scroll offset
  UBYTE VScroll;      // Vertical scroll offset
  UBYTE BusNoise[5];  // latest BusNoise rolled out at the start of each mode-line
  //
  //
  // Antic intermediate scanline buffer. Data is DMA'd from memory into
  // here to have a quick access for the screen built-up functions.
  UBYTE ScanBuffer[64];
  //
  // The line output buffer. This buffer contains the display line as constructed by antic.
  UBYTE *LineBuffer;
  //
  // An admistration structure describing the properties of an
  // Antic Mode.
  struct ModeLine {
    const int    ScanLines;     // number of scan lines per mode line
    const UBYTE *DMACycles;     // DMA cycles to pre-allocate for playfield DMA
    const UBYTE *FontCycles;    // DMA cycles to pre-allocate for character DMA. NULL for non-character mode
    const UBYTE  DMAShift;      // converts the DMAWidth to the number of bytes to fetch by left-shifting
    const bool   Fiddling;      // if true, this is a hires mode that requires color fiddling
  protected:
    const UBYTE *ScanBuffer;    // input buffer: This is Antic's input buffer
    const struct CharacterGenerator *CharGen; // Character generator: ANTIC provides this
  public:
    ModeLine(int scanlines,const UBYTE *sdma,const UBYTE *fdma,UBYTE shift,bool fiddling,
	     UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ScanLines(scanlines), DMACycles(sdma), FontCycles(fdma), DMAShift(shift),
	Fiddling(fiddling), ScanBuffer(scanbuffer), CharGen(chgen)
    { }
    //
    virtual ~ModeLine(void)
    {}
    //
    // The real character generator
    virtual void Generator(ULONG *p,int width,int scanline) = 0;
  };
  //
  // ********************************************************************************
  // ** Derived modeline generators of Antic                                       **
  // ********************************************************************************
  //
  struct ModeLine0 : public ModeLine {
    ModeLine0(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,NULL,NULL,0,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine2 : public ModeLine {
    ModeLine2(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield48Fetch+7,Playfield48Fetch+7,3,true,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine3 : public ModeLine {
    ModeLine3(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield48Fetch+7,Playfield48Fetch+7,3,true,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine4 : public ModeLine {
    ModeLine4(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield48Fetch+7,Playfield48Fetch+7,3,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine5 : public ModeLine {
    ModeLine5(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield48Fetch+7,Playfield48Fetch+7,3,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine6 : public ModeLine {
    ModeLine6(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield24Fetch+7,Playfield24Fetch+7,2,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine7 : public ModeLine {
    ModeLine7(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield24Fetch+7,Playfield24Fetch+7,2,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine8 : public ModeLine {
    ModeLine8(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield12Fetch+7,NULL,1,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLine9 : public ModeLine {
    ModeLine9(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield12Fetch+7,NULL,1,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLineA : public ModeLine {
    ModeLineA(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield24Fetch+7,NULL,2,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLineB : public ModeLine {
    ModeLineB(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield24Fetch+7,NULL,2,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLineD : public ModeLine {
    ModeLineD(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield48Fetch+7,NULL,3,false,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  struct ModeLineF : public ModeLine {
    ModeLineF(int scanlines,
	      UBYTE *scanbuffer,struct CharacterGenerator *chgen)
      : ModeLine(scanlines,Playfield48Fetch+7,NULL,3,true,scanbuffer,chgen)
    { };
    //
    virtual void Generator(ULONG *fillin,int width,int scanline);
  };
  //
  //
  // ******************************************************************************
  // ** End Antic modeline prototypes                                            **
  // ******************************************************************************
  //
  // Antic keeps mode line generators for 16 modes, depending on the
  // mode type.
  struct ModeLine *ModeLines[16];
  // 
  // Last modeline data. We keep them here as to be able to rebuild the modeline
  // contents on the fly should the user change the antic settings during the
  // horizontal line.
  struct ModeLine *CurrentMode; // current mode of the scan line
  UBYTE *FillIn;             // target position where to fill-in output data. This represents the GTIA input.
  int    Width;              // # of bytes that modeline generator fills in
  int    DisplayLine;        // in case this modeline has more than one scanline, this is the scanline #
  int    HShift;             // precomputed horizontal shift
  bool   NMIFlag;            // set if an NMI/DLI has to be generated on the next line
  bool   NMILevel;           // true if NMI is currently low and thus active
  //
  // Some Antic Preferences
  bool NTSC;                 // true if this is an NTSC antic
  //
  LONG BeforeDLICycles;      // horizontal position of the DLI in CPU clocks.
  LONG BeforeDisplayClocks;  // Number of half cpu cycles in front of the display.
  LONG YPosIncSlot;          // horizontal position where YPos gets incremented
  //
  // Some Antic built-ins:
  // 
  // Antic::ScanLine (the complex and touchy one)
  // Generates one scan line of a mode line
  // screen is the start of the target scanline and has to be bumped by this function.
  // Generate one scanline of the Antic display
  // If nmi is true, then a DLI has to generated.
  // modeline is the mode line generator for this scanline.
  // fillin is where the playfield data shall go. It may be offset to screen.
  // width is the number of pixels the modeline shall fill in, shift the
  // shift offset. 
  // displayline is the number of the scanline within the modeline this method
  // generates.
  // Emulation here starts at value zero of the "horizontal register" as defined
  // by the technical manual of Antic, sheet 5. The horizontal register itself
  // counts half color clocks, similar to to our pixel based emulation.
  // Measurements show that the DLI reaches the CPU about 8 cycles after the
  // STA WSYNC position at hpos = 208. That fits well to the hypothesis that
  // a NMI is generated at hpos = 0 since we have 228 half color clocks.
  void Scanline(bool nmi,struct ModeLine *mode,
		UBYTE *fillin,int width,bool scroll,int displayline,int first);
  //
  // Generate a complete modeline.
  void Modeline(int ir,int first,int last,int nlines,struct ModeLine *gen);
  //
  // Re-generate a modeline if CHBase/CHAttr changed in the middle.
  void RegenerateModeline(void);
  //
  // Prototypes for the antic read byte implementations
  UBYTE VCountRead(void);          // Read the vertical counter
  UBYTE LightPenHRead(void);       // Read the horizontal light pen position
  UBYTE LightPenVRead(void);       // Read the vertical light pen position
  UBYTE NMIRead(void);             // Read the NMI status register
  // Prototypes for the antic write byte implementations
  void ChBaseWrite(UBYTE val);     // write character base address
  void ChCtrlWrite(UBYTE val);     // write character control
  void DListLoWrite(UBYTE val);    // write display list low
  void DListHiWrite(UBYTE val);    // write display list high
  void DMACtrlWrite(UBYTE val);    // write DMA control
  void HScrollWrite(UBYTE val);    // write HScroll
  void VScrollWrite(UBYTE val);    // write VScroll
  void NMIEnableWrite(UBYTE val);  // write NMIEn
  void NMIResetWrite(void);        // write NMIRst
  void PMBaseWrite(UBYTE val);     // write PMBase
  void WSyncWrite(void);           // write WSync
  //
  //
  // The following methods implement reading and writing from custom
  // chip addresses
  virtual UBYTE ComplexRead(ADR mem);
  virtual bool ComplexWrite(ADR mem,UBYTE val);
  //
  // Implementation of the HBI activity.
  virtual void HBI(void);
public:
  // Constructors and desctructors for this class
  Antic(class Machine *mach);
  ~Antic(void);
  //
  // Coldstart and warmstart ANTIC
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Parse off relevant command line arguments for Antic
  virtual void ParseArgs(class ArgParser *args);
  //
  // Print the current chip status for the monitor over the line
  virtual void DisplayStatus(class Monitor *mon);
  // Read or set the internal status
  virtual void State(class SnapShot *);
  //
  // Start the special Reset Key NMI that is only available at the Atari800 and Atari400.
  void ResetNMI(void);
  //
  // Private for GTIA: Read Player DMA data
  void PlayerDMAChannel(int player,int delay,UBYTE &target)
  {
    if (DMACtrlShadow & 0x08) {
      target = UBYTE(Ram->ReadByte(PMActive->PlayerBase[player]
				   + (YPos >> PMActive->YPosShift) 
				   - delay));
    } else {
      // Interesting side case: If Antic DMA is off, then GTIA does not fetch data
      // from the bus (Test: Basic BUNDES.BAS)
      if (DMACtrlShadow & 0x20) {
	// Otherwise: Return random bus noise
	target = BusNoise[player];
      }
    }
  }
  //
  // Private for GTIA: Read Missile DMA data
  void MissileDMAChannel(int delay,UBYTE &target)
  {
    if (DMACtrlShadow & 0x0c) {
      // Missile DMA is turned on if player DMA is available.
      // Test case: POP-Demo, graphics part. (POP.BAS)
      target = UBYTE(Ram->ReadByte(PMActive->MissileBase
				   + (YPos >> PMActive->YPosShift) 
				   - delay));
    } else {
      // Interesting side case: If Antic DMA is off, then GTIA does not fetch data
      // from the bus (Test: Basic BUNDES.BAS)
      if (DMACtrlShadow & 0x20) {
	// random bus noise if no DMA channel allocated for it
	target = BusNoise[4];
      }
    }
  }
  //
  // Return the current YPos
  int CurrentYPos(void) 
  {
    return YPos;
  }
  //
  // Return the required modulo = width and height of a suitable display
  // buffer for any kind of interface function.
  void DisplayDimensions(UWORD &width,UWORD &height)
  {
    width  = DisplayModulo;
    height = PALTotal;
  }
  //
  // Run the display list generator: This also emulates a complete VBI
  void RunDisplayList(void);
  //
  // Return the top address of the display list.
  ADR DisplayList(void) const
  {
    return AnticPCShadow;
  }
  //
  // Return the width of the display in Gr.0 characters
  int CharacterWidth(void) const
  {
    if (DMACtrl & 0x20) {
      switch(DMACtrl & 0x03) {
      case 0x00:
	return 0;
      case 0x01:
	return 32;
      case 0x02:
	return 40;
      case 0x03:
	return 48;
      }
    }
    return 0;
  }
  //
  // Return the horizontal scroll register.
  UBYTE HScrollOffset(void) const
  {
    return HScroll;
  }
};
///

///
#endif
