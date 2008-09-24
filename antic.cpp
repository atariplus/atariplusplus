/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: antic.cpp,v 1.97 2008/05/22 13:03:54 thor Exp $
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
#define INCPC(d)  AnticPC = ((AnticPC + d) & 0x03ff) | (AnticPC & 0xfc00)

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
const int Antic::DisplayHeight       = 249;
// Lines up to the start of the VBI
const int Antic::VBIStart            = 248;
// Total lines in an NTSC display
const int Antic::NTSCTotal           = 262;
// Total lines in an PAL display
const int Antic::PALTotal            = 312;
// Total number of lines visible in a window.
const int Antic::WindowLines         = Antic::DisplayHeight - Antic::DisplayStart - 1;
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
  107,                 // last cycle to touch
  Playfield24Fetch + 7 // fetch every fourth cycle
};
// Pre-allocated DMA slots for display list fetch
const struct CPU::DMASlot Antic::DListFetchSlot    = {
  0,     // first cycle to allocate
  1,     // number of cycles
  107,   // last cycle to touch
  Ones
};
// Pre-allocated DMA slots for DL-LoadScanPointer fetch
const struct CPU::DMASlot Antic::DLScanFetchSlot   = {
  5,     // first cycle to allocate
  2,     // number of cycles to fetch
  107,   // last cycle to touch
  Ones   // fetch cycles six and seven
};
// Pre-allocated DMA slots for player graphics
const struct CPU::DMASlot Antic::PlayerFetchSlot   = {
  1,     // first cycle to allocate
  4,     // number of cycles to fetch
  107,   // last cycle to touch
  Ones
};
// Pre-allocated DMA slots for missile graphics
const struct CPU::DMASlot Antic::MissileFetchSlot  = {
  112,   // first cycle to fetch
  1,     // number of cycles to fetch
  107,   // last cycle to touch
  Ones
};
/*
/store/doc/Antic_Timings.txt
*/
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
  CurrentMode    = NULL;
  FillIn         = NULL;
  Width          = 0;
  DisplayLine    = 0;
  //
  BeforeDLICycles = 12; // Between 6..12 for atlantis and decathlon, 12 to make robot look correct
  BeforeDisplayClocks = 16;
  YPosIncSlot     = 108;
  YPos            = 0;
  NMIFlag         = false;
  NMILevel        = false;
  //
  NTSC            = false;
  //
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
    BusNoise[i] = UBYTE(rand() >> 8);
  }
  AnticPC       = 0x00;
  AnticPCShadow = 0x00;
  AnticPCCur    = 0x00;
  PFBase        = 0x00;
  YPos          = 0x00;
  NMIEnable     = 0x00;
  NMIStat       = 0x00;
  CharCtrl      = 0x00;
  HScroll       = 0x00;
  VScroll       = 0x00;
  DMACtrlShadow = 0x00;  
  // Reset the current modeline generator
  CurrentMode    = NULL;
  FillIn         = NULL;
  Width          = 0;
  DisplayLine    = 0;
  NMIFlag        = false;
  NMILevel       = false;
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
  ADR chbase;
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;
  
  scanline &= 7;

  if (!CharGen->UpsideDown) {
    chbase = CharGen->CharBase + scanline;
  } else {
    chbase = CharGen->CharBase + 7 - scanline;
  }
  
  do {  
    ADR   chaddr;
    UBYTE chdata,screendata;
    // get screen data from antic buffer
    screendata = *scan++;  
    // get character base address
    chaddr     = chbase + ((screendata & 0x7f) << 3);  
    // get character data
    chdata     = UBYTE(CharGen->Ram->ReadByte(chaddr));
    
    if (screendata & CharGen->InvertMask)
      chdata ^= 0xff;

    if (screendata & CharGen->BlankMask)
      chdata  = 0;
    
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
  static const UBYTE OffsetNormal[10] = {0,1,2,3,4,5,6,7,8,8};
  static const UBYTE OffsetLow[10]    = {8,8,2,3,4,5,6,7,0,1};

  if (scanline > 10)
    scanline &= 7;
 
  do {  
    ADR   chaddr,chline;
    UBYTE chdata,screendata;
    // get screen data from antic buffer
    screendata = *scan++;    
    if ((screendata & 0x60) == 0x60) {
      // This is a lowercase character. This means that the first two
      // scan lines are blank and get mirrored as the last two scan lines
      chline = OffsetLow[scanline];
    } else {
      chline = OffsetNormal[scanline];
    }
    // Check whether we have a completely blank line. If so, supply zero
    if (chline >= 8) {
      chdata = 0;
    } else {
      if (!CharGen->UpsideDown) {
	chaddr = CharGen->CharBase + chline       + ((screendata & 0x7f) << 3);
      } else {
	chaddr = CharGen->CharBase + 7 - chline   + ((screendata & 0x7f) << 3);
      }
      chdata   = UBYTE(CharGen->Ram->ReadByte(chaddr));
    }
    
     if (screendata & CharGen->InvertMask)
      chdata ^= 0xff;

    if (screendata & CharGen->BlankMask)
      chdata  = 0;
    
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
  ADR chbase;
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;

  scanline &= 7;
  
  if (!CharGen->UpsideDown) {
    chbase = CharGen->CharBase + scanline;
  } else {
    chbase = CharGen->CharBase + 7 - scanline;
  }
  
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
    
    lut        = (screendata & 0x80)?(LutHi):(LutLo);
    
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
  ADR chbase;
  int nchars  = width >> 3;
  const UBYTE *scan = ScanBuffer;
  
  if (!CharGen->UpsideDown) {
    chbase = CharGen->CharBase + (scanline >> 1);
  } else {
    chbase = CharGen->CharBase + 7 - (scanline >> 1);
  }
  
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
    
    lut        = (screendata & 0x80)?(LutHi):(LutLo);
    
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
  
  if (!CharGen->UpsideDown) {
    chbase = CharGen->CharBase + scanline;
  } else {
    chbase = CharGen->CharBase + 7 - scanline;
  }
  
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
  
  if (!CharGen->UpsideDown) {
    chbase = CharGen->CharBase +     (scanline >> 1);
  } else {
    chbase = CharGen->CharBase + 7 - (scanline >> 1);
  }
  
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

/// Antic::HBI
// Generate a horizontal blank.
void Antic::HBI(void)
{
  // Update DMACtrl for player DMA
  DMACtrlShadow = DMACtrl;
}
///

/// Antic::ScanLine
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
// a NMI is generated at hpos = 0 since we have 228 half color clocks
// and well to the information in DeReAtari where it is stated that the
// STA WSYNC must reach antic before clock cycle 103.
//
// This is all tricky. Some applications worth testing with:
// Rescue, Nautilus, JetBoot, PitfallII, Zaxxon, Frogger, Decathlon,
// Koronis Rift, Moon Patrol, Fort Apocalypse
void Antic::Scanline(bool nmi,struct ModeLine *mode,
		     UBYTE *fillin,int width,bool xscroll,int displayline,int first)
{
  int dma = DMACtrl;
  // Keep scrolling data because it may change under our feed since the
  // CPU may change it while we're working
  // shift is the displacement from the position where the data did go
  // to the position where it should have gone. GTIA corrects this, then.
  int xmin,xmax,shift,cycles;
  //
  // If we have Hi-Resolution players or are at an even line,
  // add the cycles for Player/Missile DMA
  // FIX: Checked that the number of DMA cycles does not depend
  // on the P/M resolution.
  // Check for PM DMA cycles here.  
  switch (DMACtrl & 0x0c) {      
  case 0x08: // Player DMA
  case 0x0c: // Missile DMA is for free if player DMA is turned on.
    Cpu->StealCycles(PlayerFetchSlot);
    // runs intentionally into the missile fetch
  case 0x04: // Missile DMA
    Cpu->StealCycles(MissileFetchSlot);
    break;
  default:   // No p/m DMA
    break;
  }
  //
  // Advance the CPU for a couple of cycles before the DLI is triggered (Jetboot Jack)
  Cpu->Go(BeforeDLICycles);
  //
  // Check whether the display width changed.
  if (displayline == first && (dma ^ DMACtrl) & 0x03) {
    int delta = (DMACtrl & 0x03) - (dma & 0x03);
    // If the display got smaller, fixup the PFBase pointer for the cycles at the end
    // ANTIC doesn't take.
    delta    *= (1 << mode->DMAShift) >> 1;
    PFBase    = (PFBase & 0xf000) | ((PFBase + delta) & 0x0fff);
  }
  // 
  if (xscroll) {
    shift = FillInOffset - (HScroll << 1); // this should be >= 0
  } else {
    shift = FillInOffset;
  }
  xmin = XMinNoScroll + shift;
  xmax = XMaxNoScroll + shift;
  //
  // As seen from the CPU, antic increments the line counter not before
  // HPos = 8 on the next line. Hence, we update the shadow register here
  // after having run the CPU.
  // YPosShadow = YPos; Conflicts with decathlon
  // 
  if (nmi || YPos == VBIStart) {
    // If we require a DLI, remove the 7 cycles the CPU requires for its reaction
    // on it.
    if (YPos == VBIStart) {
      NMIStat   = 0x40;      
    } else if (nmi) {
      NMIStat   = 0x80;
    }
    if (((NMIEnable & NMIStat) & 0xc0) /* && ((NMIStat & 0x40) || Cpu->IsHalted() == false)*/) {
      // Generate now if NMI is not yet active.
      Cpu->GenerateNMI();
    }
  }
  //
  // Now generate the display if there is someting to set...
  // Keep the data to be able to re-run the generator in case someone sets up a horizontal kernel
  // modifying antic data.
  CurrentMode = mode;
  FillIn      = fillin;
  Width       = width;
  HShift      = shift;
  DisplayLine = displayline;
  //
  if (width) {
      mode->Generator((ULONG *)(fillin),width,displayline);
      //
      // shift is the displacement from the position where the data did go
      // to the position where it should have gone. GTIA corrects this, then.
      //
      // Antic generated too much display, we need to erase the display data
      // that got shifted out of the frame. This requires handling of the
      // special case where the display is wide already. Then, more zeros
      // have to enter.
      if (xmin < FillInOffset) xmin = FillInOffset;
      memset(LineBuffer     ,GTIA::Background,xmin);
      memset(LineBuffer+xmax,GTIA::Background,DisplayModulo-xmax);
  } else {
      memset(LineBuffer     ,GTIA::Background,DisplayModulo);
  }
  // 
  cycles = BeforeDisplayClocks - Cpu->CurrentXPos();
  if (cycles > 0)
    Cpu->Go(cycles);
  //
  // Run now the display generator. This also launches the CPU
  Gtia->TriggerGTIAScanline(LineBuffer+shift,0,DisplayModulo-FillInOffset,mode->Fiddling);
  //
  YPos++;
  //
  machine->HBI();
}
///

/// Antic::ModeLine
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
void Antic::Modeline(int ir,int first,int last,int nlines,struct ModeLine *gen)
{
  struct CPU::DMASlot gfx;  // contains the slots to be allocated for screen graphics and font cell graphics fetches
  struct CPU::DMASlot mem;  // contains the slots for memory refresh that move aLONG with the display DMA
  bool nmi   = false;
  bool shift = false;
  int scanline;
  int dmadelta;             // DMA cycle shift allocation due to HScroll
  int lastref;              // position of memory refresh if all cycles stolen
  
  nlines--;   // for simpler calculations
  for (scanline = first;scanline <= last && YPos < DisplayHeight;scanline++) {
    UBYTE *fillin;
    int width,displayline;
    // get the line that should be currently displayed
    displayline = scanline;
    if (displayline<0)
      displayline = 0;
    if (displayline>nlines)
      displayline = nlines;
    // Get the width of the line. We must read it directly from the
    // dma control register because the CPU may change this within
    // the mode line.
    // Displace data by the fill-in offset for easier horizontal scrolling.
    // We can scroll at most 16 color clocks, hence a fill-in of 32 half
    // color clocks.
    if (ir & 0x10) {
      // DMA cycle shifting is now a bit tricky: One DMA cylce is two color clocks.
      dmadelta       = HScroll >> 1; // shift by half the cycles to the right
      // Computations change if horizontal scrolling is enabled
      width          = DMAWidthScroll << 6; // width in color clocks
      fillin         = LineBuffer   + FillInOffset + XMinScroll;
      // In case we had horizontal scrolling, we need to erase some border
      // since we generated too many rows.
      shift          = true;
      gfx.FirstCycle = FirstDMACycleScroll + dmadelta;
      gfx.NumCycles  = LastDMACycleScroll - FirstDMACycleScroll;
      gfx.LastCycle  = 106;
      mem            = MemRefreshSlot;
      if (LastDMACycleScroll == 106) {
	lastref      = 106;
      } else {
	lastref      = LastDMACycleScroll - 2 + dmadelta;
      }
    } else {
      dmadelta       = 0;
      width          = DMAWidthNoScroll << 6;
      fillin         = LineBuffer   + FillInOffset + XMinNoScroll;
      shift          = false;  // No shifting here.      
      gfx.FirstCycle = FirstDMACycleNoScroll + dmadelta;
      gfx.NumCycles  = LastDMACycleNoScroll - FirstDMACycleNoScroll;
      gfx.LastCycle  = 106;
      mem            = MemRefreshSlot; 
      if (LastDMACycleNoScroll == 106) {
	lastref      = 106;
      } else {
	lastref      = LastDMACycleNoScroll - 2;
      }
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
	gfx.FirstCycle++;
	gfx.CycleMask  = gen->FontCycles;
	Cpu->StealCycles(gfx);
      } 
      // If this is the first scan line of this mode line, add the DMA
      // cycles required to fetch the instruction
      if (scanline == first) {
	if (gen->FontCycles) {     
	  // Add cycles for fetching the characters.
	  // We offset this a little bit to ensure that the accesses for these
	  // graphics fall into empty slots. Character cell DMA starts three
	  // cycles in front of the character shape DMA.
	  gfx.FirstCycle -= 3;
	  gfx.CycleMask   = gen->DMACycles;
	  Cpu->StealCycles(gfx);
	} else if (gen->DMACycles) { 
	  // Include cycles for the screen graphics, but only on the first scan line
	  // of the mode line.
	  gfx.CycleMask   = gen->DMACycles;
	  Cpu->StealCycles(gfx);
	} 
      } 
      // In case we block memory refresh, move it one cycle to the back.
      if (Cpu->isBusy(mem.FirstCycle))
	mem.FirstCycle++;
      if (Cpu->isBusy(mem.FirstCycle))
	mem.FirstCycle++;
      // If still busy, allocate first available slot
      if (Cpu->isBusy(mem.FirstCycle)) {
	mem.NumCycles  = 1;
	mem.FirstCycle = lastref;
      }
    }
    // Get the base DMA cycles for character based modes. This
    // part of the DMA fetches characters into the mode line buffer.
    // Include memory refresh. This is additional for all scan lines, not
    // only for the first scan line of a mode line. Note that we are here
    // running in a loop.
    Cpu->StealCycles(mem);
    //
    // If this is the last scanline and the "generate DLI" bit of the
    // instruction is set, and DLIs are allowed, signal that we need
    // an NMI. This works only if a) the instruction says so, and 
    // b) the DLI is enabled.
    if (scanline == last && (ir & 0x80))
      nmi = true;
    //
    // Now run the scanline of this mode line
    Scanline(nmi,gen,fillin,width,shift,scanline,first);
  }
}
///

/// Antic::RunDisplayList
// This method runs a complete display list of Antic and
// hence generates one complete frame.
void Antic::RunDisplayList(void)
{
  bool jvb;
  int vertscroll;             // vertical scrolling flags
  class CPU      *cpu = Cpu;  // the cpu running this machine
  class AdrSpace *ram = Ram;  // the Antic RAM.
  int   currentir     = 0x10; // current instruction

  vertscroll = 0;     // no vertical scrolling here
  jvb        = false; // not yet a jump/wait VBI
  //
  // 
  // VCount must equal zero for some games but the first line
  // starts when vcount == 4. Hence, we process here vcount = 0..3
  //
  YPos         = 0;
  AnticPCShadow= AnticPC;
  AnticPCCur   = AnticPC;
  // Also reset the display frontend now: Signal that we restart from top.
  machine->Display()->ResetVertical();
  // We are not displaying anything: If the CPU does character generator modifications
  // now, no immediate consequences will be drawn.
  CurrentMode = NULL;
  do {
    // Reserve DMA slots for memory refresh within here.
    Cpu->StealCycles(MemRefreshSlot);
    // The nine cycles for DMA are already stolen and
    // need not to be counted below....
    cpu->Go(114);
    machine->HBI();
  } while(++YPos < DisplayStart);

  NMIStat = 0x00; // clear NMI status
  do {
    int dmawidth = DMAWidthNoScroll; // setup base DMA Width (corrected for scrolling)
    
    // Update vertical scrolling flags from last line
    vertscroll >>= 1;
    // Check whether we are only waiting
    if (jvb) {    
      // We are not displaying anything: If the CPU does character generator modifications
      // now, no immediate consequences will be drawn.
      CurrentMode = NULL;
      // Reserve DMA slots for memory refresh within here.
      Cpu->StealCycles(MemRefreshSlot);   
      // Yes, just generate a blank display. Requires 2 DMA cycles for memory,
      // no NMI, blank modeline, full display width, scanline #0
      Scanline(false,ModeLines[0],LineBuffer,DisplayModulo,FillInOffset,0,1);
    } else {
      struct ModeLine *gen; // mode line generator
      int i,ir,nlines;      // Antic instruction register, # scan lines in the mode
      //
      // Update the Antic bus noise now.
      for(i = 0;i < 5;i++) {
	BusNoise[i] = UBYTE(rand() >> 8);
      }
      //
      // If display DMA is disabled, re-run the same instruction again.
      if (DMACtrl & 0x20) {
	// Here: Antic is active, we must generate a display.
	AnticPCCur = AnticPC;
	currentir = ram->ReadByte(AnticPC);
	INCPC(1); // Increment the PC.
	// Reserve the slots for DMA access for the DLI counter.
	Cpu->StealCycles(DListFetchSlot);
      } else {
	currentir &= 0x7f;
      }
      ir = currentir;
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
	  AnticPC    = ram->ReadWord(AnticPC);
	  Cpu->StealCycles(DLScanFetchSlot);
	}
      } else {
	int nbytes;                         // # of bytes to read for this mode
	UBYTE *scan;
	// Regular instructions. First check for all special flags we
	// could dream of:
	//
	// Reload Playfield base pointer address if we can.
	if (ir & 0x40) {
	  if (DMACtrl & 0x20) {
	    // Antic reload screen pointer
	    PFBase   = ram->ReadWord(AnticPC);
	    INCPC(2);
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
	// horizontal scrolling
	if ((ir & 0x10) && dmawidth < 6)
	  dmawidth++; // requires additional cycles for scrolling
	//
	// get the mode line generator now
	gen        = ModeLines[ir & 0x0f];
	nlines     = gen->ScanLines;
	//
	// get # of bytes per scan line and the # of lines if there are any
	if ((DMACtrl & 0x20) && gen->DMAShift && dmawidth) {
	  ADR    pf  = PFBase;
	  nbytes     = dmawidth << gen->DMAShift;
	  //
	  // Now copy the data from memory into the scan line
	  // buffer since the mode line generators expect it there.
	  scan       = ScanBuffer;
	  do {
	    *scan++  = UBYTE(ram->ReadByte(pf));
	    // The PFBase is really a 12 bit counter and a four bit register,
	    // hence cannot cross a 4K boundary. Increment PFBase now
	    // accordingly.
	    pf       = (pf & 0xf000) | ((pf + 1) & 0x0fff);
	  } while(--nbytes);
	  PFBase     = pf;
	}
      } // End of mode type switch.
      //
      // Now go for the line generator.
      if (nlines > 0) {
	int first = 0;
	int last  = nlines - 1;
	//
	// Check for first and last vertical scroll line.
	switch(vertscroll) {
	case 2:
	  // This is the first vertical scrolling line, last was regular
	  // Hence, delay the first line.
	  first   = VScroll;
	  if (first > last)
	    first-= 16;
	  break;
	case 1:
	  // This is the last vertical scroll line, this one is no longer vscroll
	  last    = VScroll;
	}
	// Everything else is displayed the regular way
	//
	Modeline(ir,first,last,nlines,gen);
	//
      }
    }    
    // Note that the scanline function also increments the
    // vertical position and runs the pokey activity
  } while(YPos < DisplayHeight);
  //
  // Display list ended now, or we reached the end of the frame.
  // Generate now some blank lines for the vertical blank
  // We are not displaying anything: If the CPU does character generator modifications
  // now, no immediate consequences will be drawn.
  CurrentMode = NULL;
  //
  // Now run the vertical blank. The exact position depends on
  // whether we are a PAL or a NTSC machine
  if (NTSC) {
    do {
      // Reserve DMA slots for memory refresh within here.
      Cpu->StealCycles(MemRefreshSlot);
      cpu->Go(114);
      machine->HBI();
    } while(++YPos < NTSCTotal);
  } else {
    do {    
      // Reserve DMA slots for memory refresh within here.
      Cpu->StealCycles(MemRefreshSlot);
      cpu->Go(114);
      machine->HBI();
    } while(++YPos < PALTotal);
  }
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
  if (machine->CPU()->CurrentXPos() >= YPosIncSlot)
    ypos++;
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

/// Antic::RegenerateModeline
// Re-generate a modeline if CHBase/CHAttr changed in the middle.
void Antic::RegenerateModeline(void)
{
  if (CurrentMode) { 
    int xmin = XMinNoScroll + HShift;
    int xmax = XMaxNoScroll + HShift;
    CurrentMode->Generator((ULONG *)FillIn,Width,DisplayLine); 
    if (xmin < FillInOffset) xmin = FillInOffset;
    memset(LineBuffer     ,GTIA::Background,xmin);
    memset(LineBuffer+xmax,GTIA::Background,DisplayModulo-xmax);
  }
}
///

/// Antic::ChBaseWrite
// Define the character generator base address 
void Antic::ChBaseWrite(UBYTE val)
{
  class CPU *cpu  = machine->CPU();
  // First update the shadow, then modify the
  // character generators
  ChBase          = ADR(val) << 8;
  // Note that we no longer modify the character generators here. This
  // happens at the end of the line.
  Char20.CharBase = ChBase & 0xfe00;
  Char40.CharBase = ChBase & 0xfc00; // Keep care of alignment restrictions
  //
  // Check whether the currently active antic mode is a character mode. If so,
  // rebuild the output. Note that we might have been called in the middle of
  // the GTIA output just reading the very same data, and the CPU is modifying
  // antic within the horizontal line.
  //
  // Only do that in case antic hasn't run its DMA yet.
  if (cpu->CurrentXPos() < 24) {
    if (CurrentMode && Width && CurrentMode->FontCycles) {
      RegenerateModeline();
    }
  }
}
///

/// Antic::ChCtrlWrite
// Write into the control register of the character generator
void Antic::ChCtrlWrite(UBYTE val)
{ 
  class CPU *cpu = machine->CPU();
  //
  CharCtrl = val;

  // Set individual properties in the character generator
  Char20.UpsideDown = Char40.UpsideDown = (val & 0x04)?(true):(false);
  Char20.InvertMask = Char40.InvertMask = (val & 0x02)?(0x80):(0x00);
  Char20.BlankMask  = Char40.BlankMask  = (val & 0x01)?(0x80):(0x00);  
  //
  // Check whether the currently active antic mode is a character mode. If so,
  // rebuild the output. Note that we might have been called in the middle of
  // the GTIA output just reading the very same data, and the CPU is modifying
  // antic within the horizontal line. 
  if (cpu->CurrentXPos() < 24) {
    if (CurrentMode && Width && CurrentMode->FontCycles) {
      RegenerateModeline();
    }
  }
}
///

/// Antic::DListLoWrite
// Write into the low-address of the display list counter
void Antic::DListLoWrite(UBYTE val)
{
  AnticPC  = (AnticPC & 0xff00) | ADR(val);
  AnticPCShadow = AnticPC;
}
///

/// Antic::DListHiWrite
// Write int the high-address of the display list counter
void Antic::DListHiWrite(UBYTE val)
{
  AnticPC  = (AnticPC & 0x00ff) | (ADR(val) << 8);
  AnticPCShadow = AnticPC;
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
    XMinNoScroll          = XMaxNoScroll = 0;
    XMinScroll            = XMaxScroll   = 0;
    DMAWidthNoScroll      = 0;
    DMAWidthScroll        = 0;
    FirstDMACycleNoScroll = 0;
    LastDMACycleNoScroll  = 0;
    FirstDMACycleScroll   = 0;
    LastDMACycleScroll    = 0;
    break;
  case 0x01: // narrow playfield
    XMinNoScroll          = 64;
    XMaxNoScroll          = 320;
    XMinScroll            = 32;
    XMaxScroll            = 352;
    DMAWidthNoScroll      = 4;
    DMAWidthScroll        = 5;    
    FirstDMACycleNoScroll = 28;
    LastDMACycleNoScroll  = 92;
    FirstDMACycleScroll   = 20;
    LastDMACycleScroll    = 100;
    break;
  case 0x02: // medium playfield
    XMinNoScroll          = 32;
    XMaxNoScroll          = 352;
    XMinScroll            = 0;
    XMaxScroll            = 384;
    DMAWidthNoScroll      = 5;
    DMAWidthScroll        = 6;
    FirstDMACycleNoScroll = 20;
    LastDMACycleNoScroll  = 100;
    FirstDMACycleScroll   = 12;
    LastDMACycleScroll    = 106;
    break;
  case 0x03: // large playfield
    XMinNoScroll     = XMinScroll = 0;
    XMaxNoScroll     = XMaxScroll = 384;
    DMAWidthNoScroll = 6;
    DMAWidthScroll   = 6;
    FirstDMACycleNoScroll = 12;
    LastDMACycleNoScroll  = 106;
    FirstDMACycleScroll   = 12;
    LastDMACycleScroll    = 106;
    break;
  }

  // Now compute the number of cycles for player/missile DMA
  switch (val & 0x0c) {
  case 0x00: // No p/m DMA 
    PMDMACycles  = 0;
    break;
  case 0x04: // Missile DMA
    PMDMACycles  = 1;
    break;
  case 0x08: // Player DMA
  case 0x0c: // Missile DMA is for free if player DMA is turned on.
    PMDMACycles  = 5;
    break;
  }

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
  //
  /* 
     class CPU *cpu = machine->CPU();
     if (CurrentMode && Width && cpu->CurrentXPos() < 24) {
     RegenerateModeline();
     }
  */
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
bool Antic::ComplexWrite(ADR mem,UBYTE val)
{
  switch(mem & 0x0f) {
  case 0x00:
    DMACtrlWrite(val);
    return false;
  case 0x01:
    ChCtrlWrite(val);
    return false;
  case 0x02:
    DListLoWrite(val);
    return false;
  case 0x03:
    DListHiWrite(val);
    return false;
  case 0x04:
    HScrollWrite(val);
    return false;
  case 0x05:
    VScrollWrite(val);
    return false;
  case 0x07:
    PMBaseWrite(val);
    return false;
  case 0x09:
    ChBaseWrite(val);
    return false;
  case 0x0a:
    WSyncWrite();
    return true; // This is the one and only case where we require a horizontal sync!
  case 0x0e:
    NMIEnableWrite(val);
    return false;
  case 0x0f:
    NMIResetWrite();
    return false;
  default:
    return false;
  }
}
///

/// Antic::ParseArgs
// Read Antic command line arguments
void Antic::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector videovector[] = 
    { {"PAL" ,false},
      {"NTSC",true },
      {NULL ,0}
    };
  LONG val;

  val = NTSC;
  args->DefineTitle("ANTIC");
  args->DefineSelection("VideoMode","sets ANTIC video mode",videovector,val);
  NTSC = (val)?(true):(false);
  args->DefineLong("BeforeDLICycles",
		   "set position of DLI in CPU cycles",0,16,BeforeDLICycles);
  args->DefineLong("BeforeDisplayClocks",
		   "set number of CPU clocks in front of display generation",0,32,BeforeDisplayClocks);
  args->DefineLong("YPosIncCycle",
		   "horizontal position where YPos gets incremented",104,114,YPosIncSlot);

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
