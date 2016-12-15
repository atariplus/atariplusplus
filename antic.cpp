/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: antic.cpp,v 1.127 2015/12/11 16:27:35 thor Exp $
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

/// Includes
#include "monitor.hpp"
#include "gtia.hpp"
#include "mmu.hpp"
#include "pokey.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "display.hpp"
#include "gamecontroller.hpp"
#include "cpu.hpp"
#include "antic.hpp"
#include "snapshot.hpp"
#include "new.hpp"
#include "string.hpp"
///

/// Defines
// Repeat a character for times
#define REPEAT4(byte) (ULONG(byte)|(ULONG(byte)<<8)|(ULONG(byte)<<16)|(ULONG(byte)<<24))
// The following define repeats the same byte four times and installs
// it into a pointer
#define POKE4(base,byte) (*((ULONG *)base) = REPEAT4(byte))
// Increment the antic display counter, and do not cross a 1K boundary.
#define INCPC  AnticPC = ((AnticPC + 1) & 0x03ff) | (AnticPC & 0xfc00)

#define GPFB GTIA::Background_Mask // intentionally something different!
#define GPF0 GTIA::Playfield_0
#define GPF1 GTIA::Playfield_1
#define GPF2 GTIA::Playfield_2
#define GPF3 GTIA::Playfield_3
#define GPFF GTIA::Playfield_1_Fiddled

//
// The following dirty trick makes casting from/to ULONG pointers endian
// independent since endian dependentnesses are resolved at compile
// time. Yuck!
union BytePack {
  struct Bytes {
    UBYTE b1,b2,b3,b4;
  }       b;
  ULONG   l;
};

#define PACK4(a,b,c,d)  {{a,b,c,d}}
#define PACK2(a,b)      PACK4(a,a,b,b)
#define PACK1(a)        PACK2(a,a)
///

/// Statics
#ifndef HAS_MEMBER_INIT
// We displace data fill-in by these many half-color clocks for
// convenient horizontal scrolling
const int Antic::FillInOffset        = 32;
// Another scrolling offset required to fill-in PM graphics conventiently
const int Antic::PlayerMissileOffset = 64;
// The visible width of the Atari display
const int Antic::DisplayWidth        = 384;
// The total modulo from one row to another
const int Antic::DisplayModulo       = DisplayWidth + FillInOffset + PlayerMissileOffset;
// The first generated scan line
const int Antic::DisplayStart        = 8;
// The total height of the display in rows. 
const int Antic::DisplayHeight       = 248;
// Lines up to the start of the VBI
const int Antic::VBIStart            = 248;
// Total lines in an NTSC display
const int Antic::NTSCTotal           = 262;
// Total lines in an PAL display
const int Antic::PALTotal            = 312;
// Total number of lines visible in a window.
const int Antic::WindowLines         = Antic::DisplayHeight - Antic::DisplayStart;
// Total number of rows visible.
const int Antic::WindowWidth         = Antic::DisplayWidth - 32;
#endif
///

/// Various DMA allocation slot tables
// Pre-allocated DMA slots for memory refresh
static const UBYTE Ones[] = {1,1,1,1,1,1,1,1,1}; // Used by various items below
// The following pointers have to be filled into a properly dimensioned DMASlot
// structure. Details depend on the playfield width.
// Pre-allocated DMA slots for playfield graphics requiring at most 12 bytes data
const UBYTE Antic::Playfield12Fetch[103] = {0,0,0,0,0,0,0,
					    1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
					    1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
					    1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0};
// Pre-allocated DMA slots for playfield graphics requiring at most 24 bytes data
const UBYTE Antic::Playfield24Fetch[103] = {0,0,0,0,0,0,0,
					    1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,
					    1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,
					    1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0};
// Pre-allocated DMA slots for playfield graphics requiring at most 48 bytes data
const UBYTE Antic::Playfield48Fetch[103] = {0,0,0,0,0,0,0,
					    1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,
					    1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,
					    1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
const struct CPU::DMASlot Antic::MemRefreshSlot    = {
  25,                  // first cycle to allocate (was: 25)
  36,                  // number of cycles to allocate
  107,                 // first cycle not to touch
  Playfield24Fetch + 7 // fetch every fourth cycle
};
// Pre-allocated DMA slots for display list fetch
const struct CPU::DMASlot Antic::DListFetchSlot    = {
  1,     // first cycle to allocate
  1,     // number of cycles
  107,   // first cycle not to touch
  Ones
};
// Pre-allocated DMA slots for DL-LoadScanPointer fetch
const struct CPU::DMASlot Antic::DLScanFetchSlot   = {
  6,     // first cycle to allocate
  2,     // number of cycles to fetch
  107,   // first cycle not to touch
  Ones   // fetch cycles six and seven
};
// Pre-allocated DMA slots for player graphics
const struct CPU::DMASlot Antic::PlayerFetchSlot   = {
  2,     // first cycle to allocate
  4,     // number of cycles to fetch
  107,   // first cycle not to touch
  Ones
};
// Pre-allocated DMA slots for missile graphics
const struct CPU::DMASlot Antic::MissileFetchSlot  = {
  0,     // first cycle to fetch
  1,     // number of cycles to fetch
  107,   // first cycle not to touch
  Ones
};
///

/// Antic::Antic
// The Antic class constructor
Antic::Antic(class Machine *mach)
  : Chip(mach,"Antic"), Saveable(mach,"Antic"), HBIAction(mach),
    LineBuffer(new UBYTE[DisplayModulo])
{
  int i;
  // 
  //
  PMGeneratorLow.YPosShift  = 1; // shift position by one to the right: Divide by two
  PMGeneratorHigh.YPosShift = 0;
  //
  // Now build-up the mode line generators. In the first
  // step, clear them all.
  for(i=0;i<16;i++) 
    ModeLines[i] = NULL;
  //
  // Reset the current modeline generator
  ScanlineGenerator.NoMode();
  //
  GTIAStart           = 16; // Start position of GTIA.
  YPosIncSlot         = 111;
  YPos                = 0;
  //
  NTSC                = false;
  isAuto              = true;
  TotalLines          = PALTotal;
  PreviousIR          = 0x0; // Blank lines
  //
  // Define the DMA timing. 
  // First, for DMA off, no data here.
  DMA_None.Regular.Playfield.FirstCycle   = 0;
  DMA_None.Regular.Playfield.NumCycles    = 0;
  DMA_None.Regular.Glyph                  = DMA_None.Regular.Playfield;
  DMA_None.Regular.Character              = DMA_None.Regular.Playfield;
  DMA_None.Regular.FillInOffset           = 0;
  //
  // Narrow playfield.
  DMA_Narrow.Regular.Playfield.FirstCycle = 28;
  DMA_Narrow.Regular.Playfield.NumCycles  = 64;
  DMA_Narrow.Regular.Glyph.FirstCycle     = 26;
  DMA_Narrow.Regular.Glyph.NumCycles      = 64;
  DMA_Narrow.Regular.Character.FirstCycle = 26 + 3;
  DMA_Narrow.Regular.Character.NumCycles  = 64;
  DMA_Narrow.Regular.FillInOffset         = 64;
  //
  // Regular playfield.
  DMA_Normal.Regular.Playfield.FirstCycle = 20;
  DMA_Normal.Regular.Playfield.NumCycles  = 80;
  DMA_Normal.Regular.Glyph.FirstCycle     = 18;
  DMA_Normal.Regular.Glyph.NumCycles      = 80;
  DMA_Normal.Regular.Character.FirstCycle = 18 + 3;
  DMA_Normal.Regular.Character.NumCycles  = 80;
  DMA_Normal.Regular.FillInOffset         = 32;
  //
  // Wide playfield.
  DMA_Wide.Regular.Playfield.FirstCycle   = 12;
  DMA_Wide.Regular.Playfield.NumCycles    = 96;
  DMA_Wide.Regular.Glyph.FirstCycle       = 10;
  DMA_Wide.Regular.Glyph.NumCycles        = 96;
  DMA_Wide.Regular.Character.FirstCycle   = 10 + 3;
  DMA_Wide.Regular.Character.NumCycles    = 96;
  DMA_Wide.Regular.FillInOffset           = 0;
  //
  // Scrolling playfields.
  DMA_None.Scrolled                       = DMA_None.Regular;
  DMA_Narrow.Scrolled                     = DMA_Normal.Regular;
  DMA_Normal.Scrolled                     = DMA_Wide.Regular;
  DMA_Wide.Scrolled                       = DMA_Wide.Regular;
  //
  ActiveDMATiming                         = &DMA_None;
}
///

/// Antic::~Antic
// The Antic destructor
Antic::~Antic(void)
{
  int i;
  // Dispose the line buffer
  delete[] LineBuffer;
  //
  // Now delete all mode line generators
  for(i=0;i<16;i++) {
    delete ModeLines[i];
  }
}
///

/// Antic::ColdStart
// Antic ColdStart method. Also initializes
// parts of the chip we cannot handle during
// construction since we could cause an
// unhindered throw.
void Antic::ColdStart(void)
{
  // Get references to various chips now we need for quick access
  Cpu   = machine->CPU();
  Gtia  = machine->GTIA();
  //
  // Setup the address space of Antic
  PMGeneratorLow.Ram = PMGeneratorHigh.Ram = 
    Char40.Ram = Char20.Ram = Ram = machine->MMU()->AnticRAM();
  
  if (ModeLines[0x00] == NULL)
    ModeLines[0x0] = new ModeLine0( 0,ScanBuffer,NULL   );
  
  if (ModeLines[0x01] == NULL)
    ModeLines[0x1] = new ModeLine0( 0,ScanBuffer,NULL   );
  
  if (ModeLines[0x02] == NULL)
    ModeLines[0x2] = new ModeLine2( 8,ScanBuffer,&Char40);

  if (ModeLines[0x03] == NULL)
    ModeLines[0x3] = new ModeLine3(10,ScanBuffer,&Char40);

  if (ModeLines[0x04] == NULL)
    ModeLines[0x4] = new ModeLine4( 8,ScanBuffer,&Char40);

  if (ModeLines[0x05] == NULL)  
    ModeLines[0x5] = new ModeLine5(16,ScanBuffer,&Char40);
  
  if (ModeLines[0x06] == NULL)
    ModeLines[0x6] = new ModeLine6( 8,ScanBuffer,&Char20);
  
  if (ModeLines[0x07] == NULL)  
    ModeLines[0x7] = new ModeLine7(16,ScanBuffer,&Char20);
  
  if (ModeLines[0x08] == NULL)
    ModeLines[0x8] = new ModeLine8( 8,ScanBuffer,NULL   );

  if (ModeLines[0x09] == NULL)
    ModeLines[0x9] = new ModeLine9( 4,ScanBuffer,NULL   );

  if (ModeLines[0x0a] == NULL) 
    ModeLines[0xa] = new ModeLineA( 4,ScanBuffer,NULL   );
  
  if (ModeLines[0x0b] == NULL)
    ModeLines[0xb] = new ModeLineB( 2,ScanBuffer,NULL   );

  if (ModeLines[0x0c] == NULL)
    ModeLines[0xc] = new ModeLineB( 1,ScanBuffer,NULL   );

  if (ModeLines[0x0d] == NULL)
    ModeLines[0xd] = new ModeLineD( 2,ScanBuffer,NULL   );

  if (ModeLines[0x0e] == NULL)
    ModeLines[0xe] = new ModeLineD( 1,ScanBuffer,NULL   );
  
  if (ModeLines[0x0f] == NULL)
    ModeLines[0xf] = new ModeLineF( 1,ScanBuffer,NULL   );

  WarmStart();
}
///

/// Antic::WarmStart
// Warmstart: Initialize all the Antic chip variables
// during reset
void Antic::WarmStart(void)
{
  int i;
  DMACtrlWrite(0x00);  // reset DMA to none
  ChBaseWrite(0x00);   // reset character generator
  PMBaseWrite(0x00);
  //
  // Reset the internal Antic state machine
  //
  for(i = 0;i < 5;i++) {
    PlayerData[i] = 0x00;
  }
  AnticPC        = 0x00;
  AnticPCShadow  = 0x00;
  AnticPCCur     = 0x00;
  PFBase         = 0x00;
  YPos           = 0x00;
  NMIEnable      = 0x1f;
  NMIStat        = 0x00;
  CharCtrl       = 0x00;
  HScroll        = 0x00;
  VScroll        = 0x00;
  PreviousIR     = 0x00; // Blank lines 
  //
  // Reset the current modeline generator
  ScanlineGenerator.NoMode();
  //
  if (NTSC) {
    TotalLines = NTSCTotal;
  } else {
    TotalLines = PALTotal;
  }
  //
  // Clear the scan buffer as well
  memset(ScanBuffer,0,sizeof(ScanBuffer));
}
///

/// Antic::ResetNMI
// Start the special Reset Key NMI that is only available at the Atari800 and Atari400.
void Antic::ResetNMI(void)
{
  NMIStat |= 0x20; // Trigger the reset key NMI.
  // This thing cannot get disabled.
  // I think... I don't have an Atari800...
  machine->CPU()->GenerateNMI();
}
///

/// Antic::ModeLine0::Generator
// Character generator for mode line 0: All blanks and jumps go here
// This just blanks out
void Antic::ModeLine0::Generator(ULONG *ptr,int width,int)
{
  memset(ptr,GTIA::Background,width);
}
///

/// Antic::ModeLine2::Generator
// Character generator for mode line 2
void Antic::ModeLine2::Generator(ULONG *ptr,int width,int scanline)
{  
  static const BytePack Lut[16] = { PACK4(GPF2,GPF2,GPF2,GPF2),PACK4(GPF2,GPF2,GPF2,GPFF),
				    PACK4(GPF2,GPF2,GPFF,GPF2),PACK4(GPF2,GPF2,GPFF,GPFF),
				    PACK4(GPF2,GPFF,GPF2,GPF2),PACK4(GPF2,GPFF,GPF2,GPFF),
				    PACK4(GPF2,GPFF,GPFF,GPF2),PACK4(GPF2,GPFF,GPFF,GPFF),
				    PACK4(GPFF,GPF2,GPF2,GPF2),PACK4(GPFF,GPF2,GPF2,GPFF),
				    PACK4(GPFF,GPF2,GPFF,GPF2),PACK4(GPFF,GPF2,GPFF,GPFF),
				    PACK4(GPFF,GPFF,GPF2,GPF2),PACK4(GPFF,GPFF,GPF2,GPFF),
				    PACK4(GPFF,GPFF,GPFF,GPF2),PACK4(GPFF,GPFF,GPFF,GPFF) };
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer; 
  static const UBYTE OffsetNormal[16] = {0,1,2,3,4,5,6,7,8,8,2,3,4,5,6,7};
  static const UBYTE OffsetLow[16]    = {0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7};
  static const UBYTE *const LineGenerator[8] = {OffsetNormal,OffsetNormal,OffsetNormal,OffsetLow,
						OffsetNormal,OffsetNormal,OffsetNormal,OffsetLow};
 
  scanline &= 0x0f;

  do {  
    ADR   chaddr,chline;
    UBYTE chdata,screendata;
    // get screen data from antic buffer
    screendata = *scan++; 
    chline     = LineGenerator[screendata >> 5][scanline];
    // Check whether we have a completely blank line. If so, supply zero
    if (chline >= 8) {
      chdata = 0;
    } else {
      chline ^= CharGen->UpsideDown;
      chaddr  = CharGen->CharBase + chline + ((screendata & 0x7f) << 3);
      chdata  = UBYTE(CharGen->Ram->ReadByte(chaddr));
    }
    
    if (screendata & CharGen->BlankMask)
      chdata  = 0;
    
    if (screendata & CharGen->InvertMask)
      chdata ^= 0xff;

    *ptr++ = Lut[chdata >> 4].l;
    *ptr++ = Lut[chdata & 0x0f].l;
    
  } while(--nchars);
}
///

/// Antic::ModeLine3::Generator
// Character generator for mode line 3
void Antic::ModeLine3::Generator(ULONG *ptr,int width,int scanline)
{
  static const BytePack Lut[16] = { PACK4(GPF2,GPF2,GPF2,GPF2),PACK4(GPF2,GPF2,GPF2,GPFF),
				    PACK4(GPF2,GPF2,GPFF,GPF2),PACK4(GPF2,GPF2,GPFF,GPFF),
				    PACK4(GPF2,GPFF,GPF2,GPF2),PACK4(GPF2,GPFF,GPF2,GPFF),
				    PACK4(GPF2,GPFF,GPFF,GPF2),PACK4(GPF2,GPFF,GPFF,GPFF),
				    PACK4(GPFF,GPF2,GPF2,GPF2),PACK4(GPFF,GPF2,GPF2,GPFF),
				    PACK4(GPFF,GPF2,GPFF,GPF2),PACK4(GPFF,GPF2,GPFF,GPFF),
				    PACK4(GPFF,GPFF,GPF2,GPF2),PACK4(GPFF,GPFF,GPF2,GPFF),
				    PACK4(GPFF,GPFF,GPFF,GPF2),PACK4(GPFF,GPFF,GPFF,GPFF) };
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;
  static const UBYTE OffsetNormal[16] = {0,1,2,3,4,5,6,7,8,8,2,3,4,5,6,7};
  static const UBYTE OffsetLow[16]    = {8,8,2,3,4,5,6,7,0,1,2,3,4,5,6,7};
  static const UBYTE *const LineGenerator[8] = {OffsetNormal,OffsetNormal,OffsetNormal,OffsetLow,
						OffsetNormal,OffsetNormal,OffsetNormal,OffsetLow};

  scanline &= 0x0f;
 
  do {  
    ADR   chaddr,chline;
    UBYTE chdata,screendata;
    // get screen data from antic buffer
    screendata = *scan++;
    chline     = LineGenerator[screendata >> 5][scanline];    
    // Check whether we have a completely blank line. If so, supply zero
    if (chline >= 8) {
      chdata = 0;
    } else {
      chline ^= CharGen->UpsideDown;
      chaddr  = CharGen->CharBase + chline + ((screendata & 0x7f) << 3);
      chdata  = UBYTE(CharGen->Ram->ReadByte(chaddr));
    }
    
    if (screendata & CharGen->BlankMask)
      chdata  = 0;

    if (screendata & CharGen->InvertMask)
      chdata ^= 0xff;
    
    *ptr++ = Lut[chdata >> 4].l;
    *ptr++ = Lut[chdata & 0x0f].l;
  } while(--nchars);
}
///

/// Antic::ModeLine4::Generator
// Character generator for mode line 4
void Antic::ModeLine4::Generator(ULONG *ptr,int width,int scanline)
{
  static const BytePack LutLo[16] = { PACK2(GPFB,GPFB), PACK2(GPFB,GPF0), PACK2(GPFB,GPF1), PACK2(GPFB,GPF2),
				      PACK2(GPF0,GPFB), PACK2(GPF0,GPF0), PACK2(GPF0,GPF1), PACK2(GPF0,GPF2),
				      PACK2(GPF1,GPFB), PACK2(GPF1,GPF0), PACK2(GPF1,GPF1), PACK2(GPF1,GPF2),
				      PACK2(GPF2,GPFB), PACK2(GPF2,GPF0), PACK2(GPF2,GPF1), PACK2(GPF2,GPF2) };
  static const BytePack LutHi[16] = { PACK2(GPFB,GPFB), PACK2(GPFB,GPF0), PACK2(GPFB,GPF1), PACK2(GPFB,GPF3),
				      PACK2(GPF0,GPFB), PACK2(GPF0,GPF0), PACK2(GPF0,GPF1), PACK2(GPF0,GPF3),
				      PACK2(GPF1,GPFB), PACK2(GPF1,GPF0), PACK2(GPF1,GPF1), PACK2(GPF1,GPF3),
				      PACK2(GPF3,GPFB), PACK2(GPF3,GPF0), PACK2(GPF3,GPF1), PACK2(GPF3,GPF3) };
  static const BytePack *const Luts[2] = {LutLo,LutHi};
  ADR chbase;
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;

  scanline &= 7; 
  scanline ^= CharGen->UpsideDown;
  chbase    = CharGen->CharBase + scanline;
  
  do {  
    ADR   chaddr;
    UBYTE chdata,screendata;
    const BytePack *lut;

    // get screen data from antic buffer
    screendata = *scan++;  
    // get character base address
    chaddr     = chbase + ((screendata & 0x7f) << 3);  
    // get character data
    chdata     = CharGen->Ram->ReadByte(chaddr);
    lut        = Luts[screendata >> 7];
    *ptr++     = lut[chdata >> 4].l;
    *ptr++     = lut[chdata & 0x0f].l;
  } while(--nchars);
}
///

/// Antic::ModeLine5::Generator
// Character generator for mode line 5
void Antic::ModeLine5::Generator(ULONG *ptr,int width,int scanline)
{
  static const BytePack LutLo[16] = { PACK2(GPFB,GPFB), PACK2(GPFB,GPF0), PACK2(GPFB,GPF1), PACK2(GPFB,GPF2),
				      PACK2(GPF0,GPFB), PACK2(GPF0,GPF0), PACK2(GPF0,GPF1), PACK2(GPF0,GPF2),
				      PACK2(GPF1,GPFB), PACK2(GPF1,GPF0), PACK2(GPF1,GPF1), PACK2(GPF1,GPF2),
				      PACK2(GPF2,GPFB), PACK2(GPF2,GPF0), PACK2(GPF2,GPF1), PACK2(GPF2,GPF2) };
  static const BytePack LutHi[16] = { PACK2(GPFB,GPFB), PACK2(GPFB,GPF0), PACK2(GPFB,GPF1), PACK2(GPFB,GPF3),
				      PACK2(GPF0,GPFB), PACK2(GPF0,GPF0), PACK2(GPF0,GPF1), PACK2(GPF0,GPF3),
				      PACK2(GPF1,GPFB), PACK2(GPF1,GPF0), PACK2(GPF1,GPF1), PACK2(GPF1,GPF3),
				      PACK2(GPF3,GPFB), PACK2(GPF3,GPF0), PACK2(GPF3,GPF1), PACK2(GPF3,GPF3) };
  static const BytePack *const Luts[2] = {LutLo,LutHi};
  ADR chbase;
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;
  
  scanline >>= 1;
  scanline  ^= CharGen->UpsideDown;
  chbase     = CharGen->CharBase + scanline;
  
  do {  
    ADR   chaddr;
    UBYTE chdata,screendata;
    const BytePack *lut;

    // get screen data from antic buffer
    screendata = *scan++;  
    // get character base address
    chaddr     = chbase + ((screendata & 0x7f) << 3);  
    // get character data
    chdata     = CharGen->Ram->ReadByte(chaddr);
    lut        = Luts[screendata >> 7];
    *ptr++     = lut[chdata >> 4].l;
    *ptr++     = lut[chdata & 0x0f].l;
  } while(--nchars);
}
///

/// Antic::ModeLine6::Generator
// Character generator for mode line 6
void Antic::ModeLine6::Generator(ULONG *ptr,int width,int scanline)
{
  static const BytePack Lut0[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF0),PACK2(GPF0,GPFB),PACK2(GPF0,GPF0)};
  static const BytePack Lut1[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF1),PACK2(GPF1,GPFB),PACK2(GPF1,GPF1)};
  static const BytePack Lut2[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF2),PACK2(GPF2,GPFB),PACK2(GPF2,GPF2)};
  static const BytePack Lut3[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF3),PACK2(GPF3,GPFB),PACK2(GPF3,GPF3)};
  static const BytePack * const Luts[4] = {Lut0,Lut1,Lut2,Lut3};
  ADR chbase;
  int nchars  = width >> 4;
  const UBYTE *scan = ScanBuffer;

  scanline &= 7;
  scanline ^= CharGen->UpsideDown;
  chbase    = CharGen->CharBase + scanline;
  
  do {  
    const BytePack *lut;
    ADR   chaddr;
    UBYTE chdata,screendata;

    // get screen data from antic buffer
    screendata = *scan++;  
    // get character base address
    chaddr     = chbase + ((screendata & 0x3f) << 3);  
    // get character data
    chdata     = CharGen->Ram->ReadByte(chaddr);
    // get cell color
    lut        = Luts[screendata >> 6];
    ptr[3]     = lut[chdata & 0x03].l,chdata >>= 2;
    ptr[2]     = lut[chdata & 0x03].l,chdata >>= 2;
    ptr[1]     = lut[chdata & 0x03].l,chdata >>= 2;
    ptr[0]     = lut[chdata].l;
    ptr       += 4;

  } while(--nchars);
}
///

/// Antic::ModeLine7::Generator
// Character generator for mode line 7
void Antic::ModeLine7::Generator(ULONG *ptr,int width,int scanline)
{
  static const BytePack Lut0[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF0),PACK2(GPF0,GPFB),PACK2(GPF0,GPF0)};
  static const BytePack Lut1[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF1),PACK2(GPF1,GPFB),PACK2(GPF1,GPF1)};
  static const BytePack Lut2[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF2),PACK2(GPF2,GPFB),PACK2(GPF2,GPF2)};
  static const BytePack Lut3[4]  = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF3),PACK2(GPF3,GPFB),PACK2(GPF3,GPF3)};
  static const BytePack * const Luts[4] = {Lut0,Lut1,Lut2,Lut3};
  ADR chbase;
  int nchars  = width >> 4;
  const UBYTE *scan = ScanBuffer;
  
  scanline >>= 1;
  scanline  ^= CharGen->UpsideDown;
  chbase     = CharGen->CharBase + scanline;
  
  do {  
    const BytePack *lut;
    ADR   chaddr;
    UBYTE chdata,screendata;

    // get screen data from antic buffer
    screendata = *scan++;  
    // get character base address
    chaddr     = chbase + ((screendata & 0x3f) << 3);  
    // get character data
    chdata     = CharGen->Ram->ReadByte(chaddr);
    // get cell color    
    lut        = Luts[screendata >> 6];

    ptr[3] = lut[chdata & 0x03].l,chdata >>= 2;
    ptr[2] = lut[chdata & 0x03].l,chdata >>= 2;
    ptr[1] = lut[chdata & 0x03].l,chdata >>= 2;
    ptr[0] = lut[chdata].l;
    ptr   += 4;

  } while(--nchars);
}
///

/// Antic::ModeLine8::Generator
// Character generator for mode line 8
void Antic::ModeLine8::Generator(ULONG *ptr,int width,int)
{
  static const BytePack Lut[4] = {PACK1(GPFB),PACK1(GPF0),PACK1(GPF1),PACK1(GPF2)};
  int nchars = width >> 5;
  const UBYTE *scan = ScanBuffer;
  
  do {
    UBYTE screendata;
    screendata = *scan++;
    // One pixel is 4 color clocks wide = 8 hi-res pixels = 2 ULONGs
    ptr[7] = ptr[6] = Lut[screendata & 0x03].l,screendata >>= 2;
    ptr[5] = ptr[4] = Lut[screendata & 0x03].l,screendata >>= 2;
    ptr[3] = ptr[2] = Lut[screendata & 0x03].l,screendata >>= 2;
    ptr[1] = ptr[0] = Lut[screendata].l;
    ptr += 8;
  } while(--nchars);
}
///

/// Antic::ModeLine9::Generator
// Character generator for mode line 9
void Antic::ModeLine9::Generator(ULONG *ptr,int width,int)
{  
  static const BytePack Lut[2] = {PACK1(GPFB),PACK1(GPF0)};
  int nchars = width >> 5;
  const UBYTE *scan = ScanBuffer;
  
  do {
    UBYTE screendata;
    screendata = *scan++;
    
    ptr[7] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[6] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[5] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[4] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[3] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[2] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[1] = Lut[screendata & 0x01].l,screendata >>= 1;
    ptr[0] = Lut[screendata].l;
    ptr   += 8;
    
  } while(--nchars);
}
///

/// Antic::ModeLineA::Generator
// Character generator for mode line A
void Antic::ModeLineA::Generator(ULONG *ptr,int width,int)
{ 
  static const BytePack Lut[4] = {PACK1(GPFB),PACK1(GPF0),PACK1(GPF1),PACK1(GPF2)};
  int nchars  = width >> 4;
  const UBYTE *scan = ScanBuffer;
  
  do {
    UBYTE screendata;

    screendata = *scan++;
    ptr[3] = Lut[screendata & 0x03].l,screendata >>= 2;
    ptr[2] = Lut[screendata & 0x03].l,screendata >>= 2;
    ptr[1] = Lut[screendata & 0x03].l,screendata >>= 2;
    ptr[0] = Lut[screendata].l;
    ptr   += 4;
  } while(--nchars);
}
///

/// Antic::ModeLineB::Generator
// Character generator for mode line B,C
void Antic::ModeLineB::Generator(ULONG *ptr,int width,int)
{
  static const BytePack Lut[4] = {PACK2(GPFB,GPFB), PACK2(GPFB,GPF0), PACK2(GPF0,GPFB), PACK2(GPF0,GPF0) };
  int nchars  = width >> 4;
  const UBYTE *scan = ScanBuffer;
  
  do {
    UBYTE screendata;

    screendata = *scan++;

    ptr[3] = Lut[screendata & 0x03].l; screendata >>= 2;
    ptr[2] = Lut[screendata & 0x03].l; screendata >>= 2;
    ptr[1] = Lut[screendata & 0x03].l; screendata >>= 2;
    ptr[0] = Lut[screendata].l;
    
    ptr   += 4;
  } while(--nchars);
}
///

/// Antic::ModeLineD::Generator
// Character generator for mode line D,E
void Antic::ModeLineD::Generator(ULONG *ptr,int width,int)
{  
  static const BytePack Lut[16] = { PACK2(GPFB,GPFB), PACK2(GPFB,GPF0), PACK2(GPFB,GPF1), PACK2(GPFB,GPF2),
				    PACK2(GPF0,GPFB), PACK2(GPF0,GPF0), PACK2(GPF0,GPF1), PACK2(GPF0,GPF2),
				    PACK2(GPF1,GPFB), PACK2(GPF1,GPF0), PACK2(GPF1,GPF1), PACK2(GPF1,GPF2),
				    PACK2(GPF2,GPFB), PACK2(GPF2,GPF0), PACK2(GPF2,GPF1), PACK2(GPF2,GPF2) };
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;
  
  do {
    UBYTE screendata;

    screendata = *scan++;

    *ptr++ = Lut[screendata >> 4].l;
    *ptr++ = Lut[screendata & 0x0f].l;
  } while(--nchars);
}
///

/// Antic::ModeLineF::Generator
// Character generator for mode line F
void Antic::ModeLineF::Generator(ULONG *ptr,int width,int)
{
  static const BytePack Lut[16] = { PACK4(GPF2,GPF2,GPF2,GPF2),PACK4(GPF2,GPF2,GPF2,GPFF),
				    PACK4(GPF2,GPF2,GPFF,GPF2),PACK4(GPF2,GPF2,GPFF,GPFF),
				    PACK4(GPF2,GPFF,GPF2,GPF2),PACK4(GPF2,GPFF,GPF2,GPFF),
				    PACK4(GPF2,GPFF,GPFF,GPF2),PACK4(GPF2,GPFF,GPFF,GPFF),
				    PACK4(GPFF,GPF2,GPF2,GPF2),PACK4(GPFF,GPF2,GPF2,GPFF),
				    PACK4(GPFF,GPF2,GPFF,GPF2),PACK4(GPFF,GPF2,GPFF,GPFF),
				    PACK4(GPFF,GPFF,GPF2,GPF2),PACK4(GPFF,GPFF,GPF2,GPFF),
				    PACK4(GPFF,GPFF,GPFF,GPF2),PACK4(GPFF,GPFF,GPFF,GPFF) };
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;

  do {
    UBYTE screendata;

    screendata = *scan++;
      
    *ptr++ = Lut[screendata >> 4].l;
    *ptr++ = Lut[screendata & 0x0f].l;
  } while(--nchars);
}
///

/// Antic::Scanline::ComputeLineParameters
// Precompute the parameters for generating a single
// scan line. First argument is the modeline,
// then the active DMA settings for the scrolled screen,
// followed by the regular parameters defining the nominal
// borders where we clip.
// Last are the output buffer itself, followed by the scroll
// offset and the current line in the modeline.
void Antic::Scanline::ComputeLineParameters(struct ModeLine *mode,
					    struct DMAGenerator *dma,
					    struct DMAGenerator *borders,
					    UBYTE *buffer,int xscroll,int displayline)
{ 
  CurrentMode = mode;
  Width       = dma->Playfield.NumCycles << 2;
  XMin        = borders->FillInOffset + (Antic::FillInOffset - xscroll);
  if (XMin < Antic::FillInOffset)
    XMin = Antic::FillInOffset;
  XMax        = XMin + (borders->Playfield.NumCycles << 2);
  FillIn      = buffer + Antic::FillInOffset + dma->FillInOffset;
  LineBuffer  = buffer;
  DisplayLine = displayline;
}
///

/// Antic::Scanline::GenerateScanline
// Generate a single scan line.
void Antic::Scanline::GenerateScanline(void) const
{
  if (LineBuffer) {
    if (Width) {
      CurrentMode->Generator((ULONG *)(FillIn),Width,DisplayLine);
      //
      // shift is the displacement from the position where the data did go
      // to the position where it should have gone. GTIA corrects this, then.
      //
      // Antic generated too much display, we need to erase the display data
      // that got shifted out of the frame. This requires handling of the
      // special case where the display is wide already. Then, more zeros
      // have to enter.
      memset(LineBuffer     ,GTIA::Background,XMin);
      memset(LineBuffer+XMax,GTIA::Background,Antic::DisplayModulo-XMax);
    } else {
      memset(LineBuffer     ,GTIA::Background,Antic::DisplayModulo);
    }
  }
}
///

/// Antic::HBI
// Generate a horizontal blank.
void Antic::HBI(void)
{
}
///

/// Antic::FetchPlayfield
// Load data from the playfield into the scanline buffer
void Antic::FetchPlayfield(struct ModeLine *mode,struct DMAGenerator *dma)
{	
  class AdrSpace *ram = Ram;  // the Antic RAM.
  UBYTE *scan;
  int nbytes;
  ADR pf;
  //
  nbytes = dma->Playfield.NumCycles >> (4 - mode->DMAShift);
  pf     = PFBase;
  scan   = ScanBuffer;
  //
  // Now copy the data from memory into the scan line
  // buffer since the mode line generators expect it there.
  do {
    *scan++  = UBYTE(ram->ReadByte(pf));
    // The PFBase is really a 12 bit counter and a four bit register,
    // hence cannot cross a 4K boundary. Increment PFBase now
    // accordingly.
    pf       = (pf & 0xf000) | ((pf + 1) & 0x0fff);
  } while(--nbytes);
  //
  PFBase     = pf;
}
///

/// Antic::FetchPlayerMissiles
// Fetch the player-missile graphics
void Antic::FetchPlayerMissiles(void)
{
  int i;

  //
  // Read player data.
  if (DMACtrl & 0x08) {
    for(i = 0;i < 4;i++) {
      PlayerData[i] = UBYTE(Ram->ReadByte(PMActive->PlayerBase[i] 
					  + (YPos >> PMActive->YPosShift)));
    }
  } else if (DMACtrl & 0x20) {
    // Simulate bus noise
    for(i = 0;i < 4;i++) {
      PlayerData[i] = UBYTE(rand() >> 8);
    }
  } else {
    // BUNDES.BAS seems to forget to clear GRACTL or picks, by pure chance,
    // 0 as bus noise??
    PlayerData[4] = 0;
    for(i = 0;i < 4;i++) {
      PlayerData[i] = 0;
    }
  }
  //
  // Read missile data.
  // This is enabled whenever player *or* missile data is on.
  if (DMACtrl & 0x0c) {
    PlayerData[4] = UBYTE(Ram->ReadByte(PMActive->MissileBase
					+ (YPos >> PMActive->YPosShift)));
  } else if (DMACtrl & 0x20) {
    // Simulate bus noise
    PlayerData[4] = UBYTE(rand() >> 8);
  } else {
    // BUNDES.BAS seems to forget to clear GRACTL or picks, by pure chance,
    // 0 as bus noise??
    PlayerData[4] = 0;
  }
}
///

/// Antic::Modeline
// Generate one mode line of the Antic display.
// IR is the instruction code and only needed for horizontal/vertical scrolling
// and NMI generation. No jump codes are interpreted here.
// first is the first scan line of the mode line to be generated. It may be
// negative for VScroll artifacts. The first scan line is then generated 
// over and over again until the counter gets zero. 
// nlines is the number of scan lines in this mode line.
// last is the number of the last scan line to be generated. It may be larger
// than nlines. In this case, the last scan line is repeated over and over 
// again to generate more VSCROLL artefacts.
void Antic::Modeline(int ir,int vertscroll,int nlines,struct ModeLine *gen)
{
  struct CPU::DMASlot gfx;  // contains the slots to be allocated for screen graphics and font cell graphics fetches
  struct DMAGenerator *dma;
  int shift    = 0;
  int scanline = 0;
  int dmadelta;             // DMA cycle shift allocation due to HScroll
  bool isfirst    = true;
  bool islast     = false;
  bool wasfiddled = false;
  
  nlines--; // makes a couple of things simpler.
  //
  // Note that the test for "end of mode" is done here again
  // and that this decision depends on a potentially updated version
  // of VScroll. Note that VScroll must be sampled at the start of the line
  // for computing the first line, but not the last line.
  scanline = (vertscroll == 2)?(VScroll):0;
  while(islast == false && YPos <= DisplayHeight) {
    // First reserve P/M DMA slots. They appear first because
    // The CPU needs to see them before the DLI triggers in.
    if (YPos < DisplayHeight) {
      if (DMACtrl & 0x08) {
	// Player DMA is enabled. Steal both the player DMA cycle
	// as well as the missle DMA cycle, no matter whether
	// missile DMA is on.
	Cpu->StealCycles(PlayerFetchSlot);
	Cpu->StealCycles(MissileFetchSlot);
      } else if (DMACtrl & 0x04) {
	// Missle DMA only.
	Cpu->StealCycles(MissileFetchSlot);
      }
      // Fetch now the P/M data for GTIA (or not).
      FetchPlayerMissiles();
    }
    //
    // Advance the CPU for a couple of cycles before the DLI is triggered (Jetboot Jack)
    // and before the DMA settings become active.
    Cpu->Go(6);
    //
    // While the vertical line start is sampled at the start of the 
    // scanline, the end of the modeline is sampled later.
    // Moving this earlier breaks the acid test, moving the vscroll
    // start breaks numen.
    int last = (vertscroll == 1)?(VScroll):(nlines);
    //
    // Check whether this is the last line of the mode and we should
    // possibly generate a DLI. This is more elegant than the
    // previous counter adjustment.
    if (((scanline ^ last) & 0x0f) == 0)
      islast = true;
    // 
    // Advance to the DLI-generation step of ANTIC.
    Cpu->Step();
    //
    // If this is the last scanline and the "generate DLI" bit of the
    // instruction is set, and DLIs are allowed, signal that we need
    // an NMI. 
    if ((islast && (ir & 0x80)) || YPos == VBIStart) {
      bool nmi = false;
      //
      NMIStat = (YPos == VBIStart)?(0x40):(0x80);
      //
      if ((NMIEnable & NMIStat) & 0xc0) {
	// Generate now if NMI is not yet active.
	nmi = true;
      }
      //
      // Give the CPU two cycles to react. The CPU can no longer reset
      // the NMI now.
      Cpu->Step();
      //
      // The CPU cannot reset the flag here, regenerate.
      NMIStat = (YPos == VBIStart)?(0x40):(0x80);
      //
      // But the CPU can enable the NMI.
      if (((NMIEnable & NMIStat) & 0xc0) && (nmi == false)) {
	// Generate now if NMI is not yet active, but again give the CPU some time.
	Cpu->Step();
	nmi = true;
	// Note that the extra step taken here is compensated below.
      }
      Cpu->Go(2);
      //
      if (nmi) {
	// Generate now if NMI is not yet active.
	Cpu->GenerateNMI();
      }
    }
    //
    // Compute the number of clocks to the start of the display - DMA.
    int firstcycle   = GTIAStart; // At worst here because then GTIA starts.
    if (ir & 0x10) {
      dmadelta       = HScroll >> 1;
      dma            = &(ActiveDMATiming->Scrolled);
    } else {
      dmadelta       = 0;
      dma            = &(ActiveDMATiming->Regular);
    } 
    if (DMACtrl & 0x20) {
      if (gen->FontCycles) {
	if (dma->Character.FirstCycle + dmadelta < firstcycle)
	  firstcycle = dma->Character.FirstCycle + dmadelta;
      }
      if (isfirst) {
	if (gen->FontCycles) { 
	  if (dma->Glyph.FirstCycle + dmadelta < firstcycle)
	    firstcycle = dma->Glyph.FirstCycle + dmadelta;
	} else if (gen->DMACycles) {
	  if (dma->Playfield.FirstCycle + dmadelta < firstcycle)
	    firstcycle = dma->Playfield.FirstCycle + dmadelta;
	}
      }
    }
    //
    // Advance the CPU by the given number of cycles: We can run at least
    // these number of cycles uninterrupted, still giving the program
    // a chance to modify the DMA settings.
    {
      UBYTE cycle = Cpu->CurrentXPos();
      if (firstcycle - cycle > 0)
	Cpu->Go(firstcycle - cycle);
    }
    //
    // Now compute the DMA timings, finally.
    if (ir & 0x10) {
      // DMA cycle shifting is now a bit tricky: One DMA cylce is two color clocks.
      dmadelta       = HScroll >> 1; // shift by half the cycles to the right
      // Computations change if horizontal scrolling is enabled
      shift          = HScroll << 1;
      dma            = &(ActiveDMATiming->Scrolled);
    } else {
      dmadelta       = 0;
      shift          = 0;
      dma            = &(ActiveDMATiming->Regular);
    }
    //
    //
    // Steal only cycles if DMA is enabled
    if (DMACtrl & 0x20) {
      if (gen->FontCycles) {
	// Add slots for the character DMA of the specific mode for fetching
	// the characters from the screen. This has to happen every line
	// since the font data depends on the row. This does not happen for
	// graphics mode as its graphics contents is only fetched in the
	// first line. This starts one cycle later to avoid collisions
	gfx.FirstCycle = dma->Character.FirstCycle + dmadelta;
	gfx.NumCycles  = dma->Character.NumCycles;
	gfx.CycleMask  = gen->FontCycles;
	gfx.LastCycle  = 106;
	Cpu->StealCycles(gfx);
      } 
      // If this is the first scan line of this mode line, add the DMA
      // cycles required to fetch the instruction
      if (isfirst) {
	if (gen->FontCycles) {     
	  // Add cycles for fetching the character glyphs.
	  gfx.FirstCycle = dma->Glyph.FirstCycle + dmadelta;
	  gfx.NumCycles  = dma->Glyph.NumCycles;
	  gfx.CycleMask  = gen->DMACycles;
	} else if (gen->DMACycles) {
	  // Include cycles for the screen graphics, but only on the first scan line
	  // of the mode line.
	  gfx.FirstCycle = dma->Playfield.FirstCycle + dmadelta;
	  gfx.NumCycles  = dma->Playfield.NumCycles;
	  gfx.CycleMask  = gen->DMACycles;
	}
	//
	// Now get the data for which the cycles were requested.
	if (gen->DMAShift && dma->Playfield.NumCycles) {
	  gfx.LastCycle  = 106;
	  Cpu->StealCycles(gfx);
	  // Also fetch the playfield data now, only on the first line.
	  FetchPlayfield(gen,dma);
	}
      } 
    }
    // Get the base DMA cycles for character based modes. This
    // part of the DMA fetches characters into the mode line buffer.
    // Include memory refresh. This is additional for all scan lines, not
    // only for the first scan line of a mode line. 
    Cpu->StealMemCycles(MemRefreshSlot);
    //
    // Advance the CPU to the first GTIA cycle.
    if (GTIAStart > firstcycle)
      Cpu->Go(GTIAStart - firstcycle);
    //
    // Keep the fiddling flag from the last line to emulate the lost-sync bug if the last
    // line in the display is a fiddled mode.
    wasfiddled = ScanlineGenerator.isFiddled();
    //
    // Setup the parameters for the scanline thus
    // we can regenerate if CHARCTL or some other mode
    // parameter changes on the screen.
    ScanlineGenerator.ComputeLineParameters(gen,dma,&(ActiveDMATiming->Regular),
					    LineBuffer,shift,scanline);
    //
    // And run it.
    ScanlineGenerator.GenerateScanline();
    // 
    // Run now the display generator.
    // Antic has a bug here that it even drives GTIA (and does not generate a sync pulse)
    // if the last line is a hires line.
    if (YPos < DisplayHeight || wasfiddled) {
      // Create the GTIA output, player/missile, priority logic
      // plus GTIA mode processing. This also runs the CPU for the
      // remaining cycles in the playfield.
      Gtia->TriggerGTIAScanline(LineBuffer + FillInOffset - shift,PlayerData,
				DisplayModulo - FillInOffset,gen->Fiddling);
    } else {
      // Otherwise, dry-run of the CPU. Note that if there are more cycles
      // requested here than fit into the horizonal line, the remaining
      // cycles are no-ops. The CPU is not advanced for these extra cycles
      // and the machine state is not advanced.
      Cpu->Go((DisplayModulo - FillInOffset) >> 2);
    }
    //
    // We are now on the next line.
    YPos++;
    scanline++;
    isfirst = false;
    //
    // Signal start of the next line.
    machine->HBI();
  }
}
///

/// Antic::RunDisplayList
// This method runs a complete display list of Antic and
// hence generates one complete frame.
void Antic::RunDisplayList(void)
{
  bool jvb            = false;      // Not yet vertical blank
  int vertscroll      = 0;          // vertical scrolling flags
  class AdrSpace *ram = Ram;        // the Antic RAM.
  int   currentir     = PreviousIR; // current instruction

  //
  // Antic carries the vertical scrolling flag accross VBIs, so regenerate it here.
  if ((currentir & 0x0f) >= 2 && (currentir & 0x020)) { // a display mode?
    vertscroll = 2;
  }
  if (currentir & 0x0f) {
    // This is not a blank-line mode. Remove the "vertical blank" flag
    // and the LMS flag, both are not repeated.
    currentir &= ~0x40;
    // If this is a jump instruction, convert to blank lines
    if ((currentir & 0x0f) == 1)
      currentir = 0x00;
  }
  // 
  // VCount must equal zero for some games but the first line
  // starts when vcount == 4. Hence, we process here vcount = 0..3
  YPos         = 0;
  AnticPCShadow= AnticPC;
  AnticPCCur   = AnticPC;
  //
  // Also reset the display frontend now: Signal that we restart from top.
  machine->Display()->ResetVertical();
  //
  // We are not displaying anything: If the CPU does character generator modifications
  // now, no immediate consequences will be drawn.
  do {
    // Reserve DMA slots for memory refresh within here.
    Cpu->StealMemCycles(MemRefreshSlot);
    // The nine cycles for DMA are already stolen and
    // need not to be counted below....
    Cpu->Go(114);
    machine->HBI();
  } while(++YPos < DisplayStart);

  do {
    // Update vertical scrolling flags from last line
    vertscroll >>= 1;
    // Check whether we are only waiting
    if (jvb) { 
      // We are not displaying anything: If the CPU does character generator modifications
      // now, no immediate consequences will be drawn.
      ScanlineGenerator.NoMode();
      // Yes, just generate a blank display. Requires 2 DMA cycles for memory,
      // blank modeline, full display width, scanline #0.
      // The wait VSYNC instruction does generate DLIs if bit 7 is set.
      // Use the blank modeline generator for the purpose of generating
      // the right output.
      Modeline(currentir,0,1,ModeLines[0]);
    } else {
      struct ModeLine *gen; // mode line generator
      int ir,nlines;        // Antic instruction register, # scan lines in the mode
      //
      // If display DMA is disabled, re-run the same instruction again.
      if (DMACtrl & 0x20) {
	// Here: Antic is active, we must generate a display.
	AnticPCCur = AnticPC;
	currentir  = ram->ReadByte(AnticPC);
	PreviousIR = currentir;
	INCPC; // Increment the PC.
	// Reserve the slots for DMA access for the DLI counter.
	Cpu->StealCycles(DListFetchSlot);
      } 
      // DLI remains on, even if the same instruction is repeated.
      ir = currentir;
      //
      // Now check for details about the instructions. We first
      // check for blank and jump instructions.
      if ((ir & 0x0f) == 0x00) {
	// This is the blank line generator
	nlines    = ((ir >> 4) & 0x07) + 1; // # of blanks generated
	gen       = ModeLines[0];           // mode line generator: blanks
	ir       &= 0x81;                   // avoid confusion with H/VScroll
      } else if ((ir & 0x0f) == 0x01) {
	// This is the Antic jump instruction: Get the new display list start
	nlines     = 1;                     // jumping requires
	gen        = ModeLines[0];          // one blank line
	if (ir & 0x40)
	  jvb = true;                       // wait for the end of the display now
	ir        &= 0x81;                  // mask scrolling flags out
	if (DMACtrl & 0x20) {
	  UWORD pc   = ram->ReadByte(AnticPC);
	  INCPC;
	  pc        |= ram->ReadByte(AnticPC) << 8;
	  AnticPC    = pc;
	  Cpu->StealCycles(DLScanFetchSlot);
	}
      } else {
	// Regular instructions. First check for all special flags.
	// Reload Playfield base pointer address if we can.
	if (ir & 0x40) {
	  if (DMACtrl & 0x20) {
	    // Antic reload screen pointer
	    PFBase   = ram->ReadByte(AnticPC);
	    INCPC;
	    PFBase  |= ram->ReadByte(AnticPC) << 8;
	    INCPC;
	    // and add two DMA cycles
	    Cpu->StealCycles(DLScanFetchSlot);
	  }
	}
	//
	// Vertical scrolling enable
	if (ir & 0x20) {
	  // Enter the vertical scrolling flag at bit 1, bit #0
	  // is the flag from the last go.
	  vertscroll |= 2;
	}
	//
	// get the mode line generator now
	gen        = ModeLines[ir & 0x0f];
	nlines     = gen->ScanLines;
      } 
      // End of mode type switch.
      //
      // Now go for the line generator.
      if (nlines > 0) {
	// Everything else is displayed the regular way
	Modeline(ir,vertscroll,nlines,gen);
      }
    } 
    // Note that the scanline function also increments the
    // vertical position and runs the pokey activity
  } while(YPos <= DisplayHeight);
  //
  // Display list ended now, or we reached the end of the frame.
  // Generate now some blank lines for the vertical blank
  // We are not displaying anything: If the CPU does character generator modifications
  // now, no immediate consequences will be drawn.
  ScanlineGenerator.NoMode();
  //
  // Note: Releasing NMI here breaks TWERPS because it uses a tight
  // STA WSYNC loop that will extend beyond the current HPos. Leave it
  // asserted as long as the vertical blank is active.
  //
  // Now run the vertical blank. The exact position depends on
  // whether we are a PAL or a NTSC machine
  do {
    // Reserve DMA slots for memory refresh within here.
    Cpu->StealMemCycles(MemRefreshSlot);
    Cpu->Go(114);
    machine->HBI();
  } while(++YPos < TotalLines);
}
///

/// Antic::VCountRead
// Read the vertical count register
UBYTE Antic::VCountRead(void)
{
  int ypos = YPos;
  // We could use the WSync flag here to check whether we are already
  // past the critical HPos.
  // Actually, YPos is incremented in slot #108, so we fix it up here.
  if (machine->CPU()->CurrentXPos() >= YPosIncSlot) {
    ypos++;
    if (ypos == TotalLines && machine->CPU()->CurrentXPos() > YPosIncSlot)
      ypos = 0;
  }
  return UBYTE(ypos >> 1);
}
///

/// Antic::LightPenHRead
// Read the horizontal position of the light pen
UBYTE Antic::LightPenHRead(void)
{
  return machine->Lightpen()->LightPenX();
}
///

/// Antic::LightPenVRead
// Read the vertical position of the light pen
UBYTE Antic::LightPenVRead(void)
{
  return machine->Lightpen()->LightPenY();
}
///

/// Antic::NMIRead
// Read the status of the last Antic generated NMI
UBYTE Antic::NMIRead(void)
{
  return NMIStat | 0x1f;
}
///

/// Antic::ChBaseWrite
// Define the character generator base address 
void Antic::ChBaseWrite(UBYTE val)
{
  int lastcycle;

  ChBase          = ADR(val) << 8;
  // Note that we no longer modify the character generators here. This
  // happens at the end of the line.
  Char20.CharBase = ChBase & 0xfe00;
  Char40.CharBase = ChBase & 0xfc00; // Keep care of alignment restrictions
  // 
  // There should be a two-clock delay here...
  lastcycle = ActiveDMATiming->Regular.Playfield.FirstCycle + 
    ActiveDMATiming->Regular.Playfield.FirstCycle + 4;
  if (Cpu->CurrentXPos() >= GTIAStart && Cpu->CurrentXPos() + 2 < lastcycle)
    ScanlineGenerator.GenerateScanline();
}
///

/// Antic::ChCtrlWrite
// Write into the control register of the character generator
void Antic::ChCtrlWrite(UBYTE val)
{ 
  int lastcycle;

  CharCtrl = val;
  //
  // Set individual properties in the character generator
  Char20.UpsideDown = Char40.UpsideDown = (val & 0x04)?(0x07):(0x00);
  Char20.InvertMask = Char40.InvertMask = (val & 0x02)?(0x80):(0x00);
  Char20.BlankMask  = Char40.BlankMask  = (val & 0x01)?(0x80):(0x00);  
  //
  // There should be a two-clock delay here...
  lastcycle = ActiveDMATiming->Regular.Playfield.FirstCycle + 
    ActiveDMATiming->Regular.Playfield.FirstCycle + 4;
  if (Cpu->CurrentXPos() >= GTIAStart && Cpu->CurrentXPos() + 2 < lastcycle)
    ScanlineGenerator.GenerateScanline();
}
///

/// Antic::DListLoWrite
// Write into the low-address of the display list counter
void Antic::DListLoWrite(UBYTE val)
{
  AnticPC  = (AnticPC & 0xff00) | ADR(val);
  //
  // The shadow copy is just for the debugger.
  AnticPCShadow = AnticPC;
  // AxisAssesin seems to require this, otherwise the DLI
  // in the title screen rolls around forever.
  //
  // Actually, this is just a matter of bad luck
  // as it depends on when precisely the intro notices that
  // the START button is pressed.
  //PreviousIR    = 0x00;
}
///

/// Antic::DListHiWrite
// Write int the high-address of the display list counter
void Antic::DListHiWrite(UBYTE val)
{
  AnticPC       = (AnticPC & 0x00ff) | (ADR(val) << 8);
  AnticPCShadow = AnticPC;
  //
  // AxisAssesin seems to require this, otherwise the DLI
  // in the title screen rolls around forever.
  //
  // Actually, this is just a matter of bad luck
  // as it depends on when precisely the intro notices that
  // the START button is pressed.
  // PreviousIR    = 0x00;
}
///

/// Antic::DMACtrlWrite
// Write into the DMA control register
void Antic::DMACtrlWrite(UBYTE val)
{
  // First step: Store the DMA control register value now
  DMACtrl  = val;

  // Compute the width of the playfield display from bits 0..1
  switch (val & 0x03) {
  case 0x00: // no playfield DMA
    ActiveDMATiming         = &DMA_None;
    // Blank the display.
    if (Cpu->CurrentXPos() >= GTIAStart) {
      ScanlineGenerator.Width = 0;
      ScanlineGenerator.GenerateScanline();
    }
    break;
  case 0x01: // narrow playfield
    ActiveDMATiming       = &DMA_Narrow;
    break;
  case 0x02: // medium playfield
    ActiveDMATiming       = &DMA_Normal;
    break;
  case 0x03: // large playfield
    ActiveDMATiming       = &DMA_Wide;
    break;
  }
  
  //
  // Check whether we have single or double resultion PlayerMissile DMA
  if (val & 0x10) {
    // High resolution PM DMA
    PMActive = &PMGeneratorHigh;
  } else {
    // Low resolution PM DMA
    PMActive = &PMGeneratorLow;
  }
}
///

/// Antic::HScrollWrite
// Write into the HScroll register
void Antic::HScrollWrite(UBYTE val)
{ 
  HScroll  = UBYTE(val & 0x0f);
}
///

/// Antic::VScrollWrite
// Write into the VScroll register
void Antic::VScrollWrite(UBYTE val)
{
  VScroll  = UBYTE(val & 0x0f);
}
///

/// Antic::NMIEnableWrite
// Write into the NMI Enable register
void Antic::NMIEnableWrite(UBYTE val)
{
  NMIEnable = UBYTE(val | 0x1f); // Lower bits are forced to one
  //
  // Note that this does not trigger a pending NMI
}
///

/// Antic::NMIResetWrite
// Write into the reset NMI register
void Antic::NMIResetWrite(void)
{
  NMIStat = 0x1f;  // clear all pending NMI's
}
///

/// Antic::PMBaseWrite
// Write into the player/missile DMA register
void Antic::PMBaseWrite(UBYTE val)
{
  ADR PMBaseLo,PMBaseHi;
  // First define the shadow register
  PMBase   = ADR(val)        << 8;
  PMBaseLo = ADR(val & 0xfc) << 8;
  PMBaseHi = ADR(val & 0xf8) << 8;

  PMGeneratorLow.MissileBase    = PMBaseLo + 0x180;
  PMGeneratorLow.PlayerBase[0]  = PMBaseLo + 0x200;
  PMGeneratorLow.PlayerBase[1]  = PMBaseLo + 0x280;
  PMGeneratorLow.PlayerBase[2]  = PMBaseLo + 0x300;
  PMGeneratorLow.PlayerBase[3]  = PMBaseLo + 0x380;

  PMGeneratorHigh.MissileBase   = PMBaseHi + 0x300;
  PMGeneratorHigh.PlayerBase[0] = PMBaseHi + 0x400;
  PMGeneratorHigh.PlayerBase[1] = PMBaseHi + 0x500;
  PMGeneratorHigh.PlayerBase[2] = PMBaseHi + 0x600;
  PMGeneratorHigh.PlayerBase[3] = PMBaseHi + 0x700;
}
///

/// Antic::WSyncWrite
// Write into the WSync register
void Antic::WSyncWrite(void)
{
  // Stop the CPU until we reach the WSync position
  //
  // This pulls the RDY line. Note that this is different
  // from HALT, which requests an immediate stop, and is
  // used for DMA purposes.
  Cpu->WSyncStop();
}
///

/// Antic::ComplexRead
// Read an Antic register. There aren't many read registers here...
UBYTE Antic::ComplexRead(ADR mem)
{
  switch(mem & 0x0f) {
  case 0x0b:
    return VCountRead();
  case 0x0c:
    return LightPenHRead();
  case 0x0d:
    return LightPenVRead();
  case 0x0f:
    return NMIRead();
  default:
    return 0xff;
  }
}
///

/// Antic::ComplexWrite
// Write into an Antic register
void Antic::ComplexWrite(ADR mem,UBYTE val)
{
  switch(mem & 0x0f) {
  case 0x00:
    DMACtrlWrite(val);
    return;
  case 0x01:
    ChCtrlWrite(val);
    return;
  case 0x02:
    DListLoWrite(val);
    return;
  case 0x03:
    DListHiWrite(val);
    return;
  case 0x04:
    HScrollWrite(val);
    return;
  case 0x05:
    VScrollWrite(val);
    return;
  case 0x07:
    PMBaseWrite(val);
    return;
  case 0x09:
    ChBaseWrite(val);
    return;
  case 0x0a:
    WSyncWrite();
    return;
  case 0x0e:
    NMIEnableWrite(val);
    return;
  case 0x0f:
    NMIResetWrite();
    return;
  default:
    return;
  }
}
///

/// Antic::ParseArgs
// Read Antic command line arguments
void Antic::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector videovector[] = 
    { {"Auto",2    },
      {"PAL" ,0    },
      {"NTSC",1    },
      {NULL ,0}
    };
  LONG val;

  val = NTSC?1:0;
  if (isAuto)
    val = 2;
  args->DefineTitle("ANTIC");
  args->DefineSelection("ANTICVideoMode","sets ANTIC video mode",videovector,val);
  switch(val) {
  case 0:
    NTSC   = false;
    isAuto = false;
    break;
  case 1:
    NTSC   = true;
    isAuto = false;
    break;
  case 2:
    NTSC   = machine->isNTSC();
    isAuto = true;
    break;
  }
  /*args->DefineLong("BeforeDLICycles",
    "set position of DLI in CPU cycles",2,11,BeforeDLICycles);*/
  /*args->DefineLong("YPosIncCycle",
		   "horizontal position where YPos gets incremented",104,114,YPosIncSlot);
  */

  if (NTSC) {
    TotalLines = NTSCTotal;
  } else {
    TotalLines = PALTotal;
  }
}
///

/// Antic::DisplayStatus
// Print the internal status of Antic over the monitor
// for debugging purposes
void Antic::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Antic Status:\n"
		   "\tDListTop    : %04x\tDListCurrent : %04x\tYPos         : %d\n"
		   "\tPlayerMBase : %04x\tCharBase     : %04x\tCharCtrl     : %02x\n"
		   "\tNMIEnable   : %02x\tNMIStat      : %02x\tDMACtrl      : %02x\n"
		   "\tVScroll     : %02x\tHScroll      : %02x\n"
		   "\tVideoMode   : %s\n",
		   AnticPCShadow,AnticPCCur,YPos,
		   PMBase,ChBase,CharCtrl,
		   NMIEnable,NMIStat,DMACtrl,
		   VScroll,HScroll,
		   (NTSC)?("NTSC"):("PAL")
		   );
}
///

/// Antic::State
// Read or set the internal status
void Antic::State(class SnapShot *sn)
{
  UWORD pc     = AnticPC;
  UWORD pmbase = PMBase;
  UWORD chbase = ChBase;
  UWORD pfbase = PFBase;
  
  sn->DefineTitle("Antic");
  sn->DefineLong("PC","Antic program counter",0x0000,0xffff,pc);
  sn->DefineLong("PMBase","Antic Player/Missile base address",0x0000,0xffff,pmbase);
  PMBaseWrite(UBYTE(pmbase >> 8));
  sn->DefineLong("CHBase","Antic character generator base address",0x0000,0xffff,chbase);
  sn->DefineLong("CHCtrl","Antic character control register",0x00,0xff,CharCtrl);
  ChBaseWrite(UBYTE(chbase >> 8));
  ChCtrlWrite(CharCtrl);
  sn->DefineLong("PFBase","Antic current playfield address",0x0000,0xffff,pfbase);
  sn->DefineLong("NMIEnable","Antic NMI enable register",0x00,0xff,NMIEnable);
  sn->DefineLong("NMIStat","Antic NMI status register",0x00,0xff,NMIStat);
  sn->DefineLong("DMACtrl","Antic DMA control register",0x00,0xff,DMACtrl);
  DMACtrlWrite(DMACtrl);
  sn->DefineLong("HScroll","Antic horizontal scroll register",0x00,0xff,HScroll);
  sn->DefineLong("VScroll","Antic vertical scroll register",0x00,0xff,VScroll);

  AnticPC = pc;
  PFBase  = pfbase;
}
///
