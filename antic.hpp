/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: antic.hpp,v 1.71 2021/08/16 10:31:01 thor Exp $
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
  static const int DisplayHeight       INIT(248);
  // Lines up to the start of the VBI
  static const int VBIStart            INIT(248);
  // Total lines in an NTSC display
  static const int NTSCTotal           INIT(262);
  // Total lines in an PAL display (312)
  static const int PALTotal            INIT(312);
  // Total number of lines visible in a window.
  static const int WindowHeight        INIT(DisplayHeight - DisplayStart);
  // Total number of rows visible.
  static const int WindowWidth         INIT(DisplayWidth - 32);
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
  // The DMA Info for a single slot.
  struct DMAAllocator {
    int FirstCycle;   // Where the first cycle has to be allocated
    int NumCycles;    // number of cycles to allocate.
  };
  //
  // The DMA generator: Defines where to load the cycles.
  struct DMAGenerator {
    struct DMAAllocator Playfield;  // where playfield data fetches start
    struct DMAAllocator Glyph;      // glyph data
    struct DMAAllocator Character;  // character graphics data.
    int                 FillInOffset; // where to place the character data in the buffer.
  };
  //
  // A DMA timing consists of a pair of timings, one for non-scrolled and
  // one for scrolled data.
  struct DMATimingPair {
    struct DMAGenerator Regular;
    struct DMAGenerator Scrolled;
  } *ActiveDMATiming;
  //
  //
  // DMA Timings for narrow, normal and wide displays.
  struct DMATimingPair DMA_None,DMA_Narrow,DMA_Normal,DMA_Wide;
  //
  // The following mini-structure describes the character generator.
  // We have two of them, a 20 characters/row and a 40 characters/row generator.
  // We must supply both as the character bases are different. The 40Char modes
  // ignore more bits for the base generation.
public:
  struct CharacterGenerator {
    // Character generator: Orientation in vertical direction: +1 or -1
    class AdrSpace *Ram;          // where to fetch characters from
    UBYTE           UpsideDown;   // if 7 then characters are displayed upside down, otherwise 0
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
  UBYTE CharCtrl;     // Character control register shaddow of the character generator
  UBYTE HScroll;      // Horizontal scroll offset
  UBYTE VScroll;      // Vertical scroll offset
  UBYTE PlayerData[5];// Antic service for GTIA: Data fetched or seen as p/m data.
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
  // Scanline generator: This helper is responsible for drawing a single
  // scan line.
  struct Scanline {
    //
    // Last modeline data. We keep them here as to be able to rebuild the modeline
    // contents on the fly should the user change the antic settings during the
    // horizontal line.
    struct ModeLine *CurrentMode; // current mode of the scan line
    //
    // Output buffer position
    UBYTE           *LineBuffer;
    //
    // Fill-in position in the antic output buffer.
    UBYTE           *FillIn;
    //
    // Minimum and maximum border position
    int              XMin;
    int              XMax;
    int              Width;
    //
    // Generated line (for the character modes) of the mode line
    int              DisplayLine;
    //
    // The constructor: Just reset the entries.
    Scanline(void)
      : CurrentMode(NULL), LineBuffer(NULL), FillIn(NULL)
    {
    }
    //
    // Precompute the parameters for generating a single
    // scan line. First argument is the modeline,
    // then the active DMA settings for the scrolled screen,
    // followed by the regular parameters defining the nominal
    // borders where we clip.
    // Last are the output buffer itself, followed by the scroll
    // offset and the current line in the modeline.
    void ComputeLineParameters(struct ModeLine *mode,
			       struct DMAGenerator *dma,
			       struct DMAGenerator *borders,
			       UBYTE *buffer,int xscroll,int displayline);
    //
    // Reset the mode line, no current mode active.
    void NoMode(void)
    {
      CurrentMode = NULL;
      LineBuffer  = NULL;
    }
    //
    // Generate a single scan line.
    void GenerateScanline(void) const;
    //
    // Check whether the currently active mode is a fiddled mode, i.e. 
    // a hires mode generating half color clocks.
    bool isFiddled(void) const
    {
      if (CurrentMode)
	return CurrentMode->Fiddling;
      return false;
    }
    //
  }      ScanlineGenerator;
  //
  // Current instruction - stored for next line.
  UBYTE  PreviousIR;
  //
  // Some Antic Preferences
  bool NTSC;                 // true if this is an NTSC antic
  bool isAuto;               // true if video mode comes from the machine
  //
  LONG GTIAStart;            // Horizontal position where GTIA processing starts.
  LONG YPosIncSlot;          // horizontal position where YPos gets incremented
  LONG TotalLines;           // Number of total lines in this machine
  //
  // Some Antic built-ins:
  //
  // Load data from the playfield into the scanline buffer
  void FetchPlayfield(struct ModeLine *mode,struct DMAGenerator *dma);
  //
  // Fetch the player-missile graphics
  void FetchPlayerMissiles(void);
  //
  // Generate a complete modeline.
  void Modeline(int ir,int vscroll,int nlines,struct ModeLine *gen);
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
  virtual void  ComplexWrite(ADR mem,UBYTE val);
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
  // Return the width of the display in Mode 2 characters
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
