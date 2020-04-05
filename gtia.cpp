/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gtia.cpp,v 1.130 2020/03/21 20:51:44 thor Exp $
 **
 ** In this module: GTIA graphics emulation
 **********************************************************************************/

/// Includes
#include "exceptions.hpp"
#include "keyboard.hpp"
#include "machine.hpp"
#include "antic.hpp"
#include "gamecontroller.hpp"
#include "keyboard.hpp"
#include "gtia.hpp"
#include "cpu.hpp"
#include "mmu.hpp"
#include "monitor.hpp"
#include "display.hpp"
#include "colorentry.hpp"
#include "palcolorblurer.hpp"
#include "flickerfixer.hpp"
#include "palflickerfixer.hpp"
#include "argparser.hpp"
#include "sound.hpp"
#include "snapshot.hpp"
#include "new.hpp"
#include "string.hpp"
#include "stdio.hpp"
///

/*
** PM-Timing issues:
** It takes six color clocks from the write until the first color clock
** of the player can appear on the screen. It takes another six color clocks
** after the last color clock of the player before the logic is free again
** and new data can be pushed in.
*/

/// Statics
#ifndef HAS_MEMBER_INIT
const int GTIA::PMObject::Player_Left_Border  = 4;
const int GTIA::PMObject::Player_Right_Border = 380;
const int GTIA::PMScanlineSize      = 640; // maximum size of a PM scanline
#endif
///

/// GTIA::GTIA
// This is the constructor of the GTIA class
GTIA::GTIA(class Machine *mach)
  : Chip(mach,"GTIA"), Saveable(mach,"GTIA"), HBIAction(mach),
    ExternalColorMap(NULL), ColorMapToLoad(NULL), LoadedColorMap(NULL),
    PostProcessor(NULL),
    PMTarget(new UBYTE[PMScanlineSize]),
    HueMix(new UBYTE[2*16])
{
  int i;
  //
  // Install the default parameters
  //
  ColPF1FiddledArtifacts = false;
  NTSC                   = false;
  isAuto                 = true;
  Speaker                = false;  
  PALColorBlur           = false;
  AntiFlicker            = false;
  ChipGeneration         = GTIA_2;
  ActiveInput            = 0;
  Prior                  = 0;
  InitialPrior           = 0;
  HPos                   = 0;
  Fiddling               = false;
  PMReaction             = 12;
  PMResize               = 6;
  PMShape                = 6;
  //
  // Setup the hi-level-collision registers to "all collisions detectable"
  for(i=0;i<4;i++) {
    PlayerCollisions[i]    = AllC;
    PlayfieldCollisions[i] = AllC;
  }
  //
  // Setup player hitmasks. Missiles do not create hits, but are hit by players.
  // Strange logic, but such is life.
  for(i=0;i<4;i++) {
    Player[i].MeMask       = UBYTE(1<<i);
    // Setup object identification mask.
    Player[i].DisplayMask  = UBYTE(1<<i);
    Missile[i].DisplayMask = UBYTE(1<<(i+4));
  }
  //
  // Initialize to PAL.
  ColorMap = PALColorMap;
  //
  // Setup the mode generators for all display pre-/post processing facilities
  Mode00UF = new class DisplayGenerator00Unfiddled(this);
  Mode00F  = new class DisplayGenerator00Fiddled(this);
  Mode00FA = new class DisplayGenerator00FiddledArtefacted(this);
  Mode40UF = new class DisplayGenerator40Unfiddled(this);
  Mode40F  = new class DisplayGenerator40Fiddled(this);  
  Mode80UF = new class DisplayGenerator80Unfiddled(this);
  Mode80F  = new class DisplayGenerator80Fiddled(this);  
  ModeC0UF = new class DisplayGeneratorC0Unfiddled(this);
  ModeC0F  = new class DisplayGeneratorC0Fiddled(this);
  ModeSUF  = new class DisplayGeneratorStrangeUnfiddled(this);
  ModeSF   = new class DisplayGeneratorStrangeFiddled(this);
}
///

/// GTIA::~GTIA
// The destructor of the GTIA class
GTIA::~GTIA(void)
{
  delete[] PMTarget;
  delete[] HueMix;
  delete[] LoadedColorMap;
  delete[] ColorMapToLoad;
  delete[] ExternalColorMap;
  //
  // Dispose the mode generators now
  delete Mode00UF;
  delete Mode00F;
  delete Mode00FA;
  delete Mode40UF;
  delete Mode40F;
  delete Mode80UF;
  delete Mode80F;
  delete ModeC0UF;
  delete ModeC0F;
  delete ModeSUF;
  delete ModeSF;
  delete PostProcessor;
}
///

/// GTIA::PALColorMap
// The colormap of the GTIA: Since it is created by the GTIA for the
// real hardware, the colormap is also defined here.
const struct ColorEntry GTIA::PALColorMap[256] = {
	{0x00,0x00,0x00,0x00,0x000000},
	{0x00,0x1c,0x1c,0x1c,0x1c1c1c},
	{0x00,0x39,0x39,0x39,0x393939},
	{0x00,0x59,0x59,0x59,0x595959},
	{0x00,0x79,0x79,0x79,0x797979},
	{0x00,0x92,0x92,0x92,0x929292},
	{0x00,0xab,0xab,0xab,0xababab},
	{0x00,0xbc,0xbc,0xbc,0xbcbcbc},
	{0x00,0xbc,0xbc,0xbc,0xbcbcbc},
	{0x00,0xcd,0xcd,0xcd,0xcdcdcd},
	{0x00,0xd9,0xd9,0xd9,0xd9d9d9},
	{0x00,0xe6,0xe6,0xe6,0xe6e6e6},
	{0x00,0xec,0xec,0xec,0xececec},
	{0x00,0xf2,0xf2,0xf2,0xf2f2f2},
	{0x00,0xf8,0xf8,0xf8,0xf8f8f8},
	{0x00,0xff,0xff,0xff,0xffffff},
	{0x00,0x39,0x17,0x01,0x391701},
	{0x00,0x5e,0x23,0x04,0x5e2304},
	{0x00,0x83,0x30,0x08,0x833008},
	{0x00,0xa5,0x47,0x16,0xa54716},
	{0x00,0xc8,0x5f,0x24,0xc85f24},
	{0x00,0xe3,0x78,0x20,0xe37820},
	{0x00,0xff,0x91,0x1d,0xff911d},
	{0x00,0xff,0xab,0x1d,0xffab1d},
	{0x00,0xff,0xab,0x1d,0xffab1d},
	{0x00,0xff,0xc5,0x1d,0xffc51d},
	{0x00,0xff,0xce,0x34,0xffce34},
	{0x00,0xff,0xd8,0x4c,0xffd84c},
	{0x00,0xff,0xe6,0x51,0xffe651},
	{0x00,0xff,0xf4,0x56,0xfff456},
	{0x00,0xff,0xf9,0x77,0xfff977},
	{0x00,0xff,0xff,0x98,0xffff98},
	{0x00,0x45,0x19,0x04,0x451904},
	{0x00,0x72,0x1e,0x11,0x721e11},
	{0x00,0x9f,0x24,0x1e,0x9f241e},
	{0x00,0xb3,0x3a,0x20,0xb33a20},
	{0x00,0xc8,0x51,0x22,0xc85122},
	{0x00,0xe3,0x69,0x20,0xe36920},
	{0x00,0xff,0x81,0x1e,0xff811e},
	{0x00,0xff,0x8c,0x25,0xff8c25},
	{0x00,0xff,0x8c,0x25,0xff8c25},
	{0x00,0xff,0x98,0x2c,0xff982c},
	{0x00,0xff,0xae,0x38,0xffae38},
	{0x00,0xff,0xc5,0x45,0xffc545},
	{0x00,0xff,0xc5,0x59,0xffc559},
	{0x00,0xff,0xc6,0x6d,0xffc66d},
	{0x00,0xff,0xd5,0x87,0xffd587},
	{0x00,0xff,0xe4,0xa1,0xffe4a1},
	{0x00,0x4a,0x17,0x04,0x4a1704},
	{0x00,0x7e,0x1a,0x0d,0x7e1a0d},
	{0x00,0xb2,0x1d,0x17,0xb21d17},
	{0x00,0xc8,0x21,0x19,0xc82119},
	{0x00,0xdf,0x25,0x1c,0xdf251c},
	{0x00,0xec,0x3b,0x38,0xec3b38},
	{0x00,0xfa,0x52,0x55,0xfa5255},
	{0x00,0xfc,0x61,0x61,0xfc6161},
	{0x00,0xfc,0x61,0x61,0xfc6161},
	{0x00,0xff,0x70,0x6e,0xff706e},
	{0x00,0xff,0x7f,0x7e,0xff7f7e},
	{0x00,0xff,0x8f,0x8f,0xff8f8f},
	{0x00,0xff,0x9d,0x9e,0xff9d9e},
	{0x00,0xff,0xab,0xad,0xffabad},
	{0x00,0xff,0xb9,0xbd,0xffb9bd},
	{0x00,0xff,0xc7,0xce,0xffc7ce},
	{0x00,0x05,0x05,0x68,0x050568},
	{0x00,0x3b,0x13,0x6d,0x3b136d},
	{0x00,0x71,0x22,0x72,0x712272},
	{0x00,0x8b,0x2a,0x8c,0x8b2a8c},
	{0x00,0xa5,0x32,0xa6,0xa532a6},
	{0x00,0xb3,0x38,0xb8,0xb338b8},
	{0x00,0xb6,0x3c,0xbd,0xb63cbd},
	{0x00,0xdb,0x47,0xdd,0xdb47dd},
	{0x00,0xdb,0x47,0xdd,0xdb47dd},
	{0x00,0xea,0x51,0xeb,0xea51eb},
	{0x00,0xf4,0x70,0xf5,0xf470f5},
	{0x00,0xf8,0x90,0xf7,0xf890f7},
	{0x00,0xfa,0xa8,0xfa,0xfaa8fa},
	{0x00,0xff,0xac,0xfb,0xffacfb},
	{0x00,0xff,0xae,0xfd,0xffaefd},
	{0x00,0xff,0xb0,0xff,0xffb0ff},
	{0x00,0x28,0x04,0x79,0x280479},
	{0x00,0x40,0x09,0x84,0x400984},
	{0x00,0x59,0x0f,0x90,0x590f90},
	{0x00,0x70,0x24,0x9d,0x70249d},
	{0x00,0x88,0x39,0xaa,0x8839aa},
	{0x00,0xa4,0x41,0xc3,0xa441c3},
	{0x00,0xc0,0x4a,0xdc,0xc04adc},
	{0x00,0xd0,0x54,0xed,0xd054ed},
	{0x00,0xd0,0x54,0xed,0xd054ed},
	{0x00,0xe0,0x5e,0xff,0xe05eff},
	{0x00,0xe9,0x6d,0xff,0xe96dff},
	{0x00,0xf2,0x7c,0xff,0xf27cff},
	{0x00,0xf8,0x8a,0xff,0xf88aff},
	{0x00,0xff,0x98,0xff,0xff98ff},
	{0x00,0xfe,0xa1,0xff,0xfea1ff},
	{0x00,0xfe,0xab,0xff,0xfeabff},
	{0x00,0x35,0x08,0x8a,0x35088a},
	{0x00,0x42,0x0a,0xad,0x420aad},
	{0x00,0x50,0x0c,0xd0,0x500cd0},
	{0x00,0x64,0x28,0xd0,0x6428d0},
	{0x00,0x79,0x45,0xd0,0x7945d0},
	{0x00,0x8d,0x4b,0xd4,0x8d4bd4},
	{0x00,0xa2,0x51,0xd9,0xa251d9},
	{0x00,0xb0,0x58,0xec,0xb058ec},
	{0x00,0xb0,0x58,0xec,0xb058ec},
	{0x00,0xbe,0x60,0xff,0xbe60ff},
	{0x00,0xc5,0x6b,0xff,0xc56bff},
	{0x00,0xcc,0x77,0xff,0xcc77ff},
	{0x00,0xd1,0x83,0xff,0xd183ff},
	{0x00,0xd7,0x90,0xff,0xd790ff},
	{0x00,0xdb,0x9d,0xff,0xdb9dff},
	{0x00,0xdf,0xaa,0xff,0xdfaaff},
	{0x00,0x05,0x1e,0x81,0x051e81},
	{0x00,0x06,0x26,0xa5,0x0626a5},
	{0x00,0x08,0x2f,0xca,0x082fca},
	{0x00,0x26,0x3d,0xd4,0x263dd4},
	{0x00,0x44,0x4c,0xde,0x444cde},
	{0x00,0x4f,0x5a,0xee,0x4f5aee},
	{0x00,0x5a,0x68,0xff,0x5a68ff},
	{0x00,0x65,0x75,0xff,0x6575ff},
	{0x00,0x65,0x75,0xff,0x6575ff},
	{0x00,0x71,0x83,0xff,0x7183ff},
	{0x00,0x80,0x91,0xff,0x8091ff},
	{0x00,0x90,0xa0,0xff,0x90a0ff},
	{0x00,0x97,0xa9,0xff,0x97a9ff},
	{0x00,0x9f,0xb2,0xff,0x9fb2ff},
	{0x00,0xaf,0xbe,0xff,0xafbeff},
	{0x00,0xc0,0xcb,0xff,0xc0cbff},
	{0x00,0x0c,0x04,0x8b,0x0c048b},
	{0x00,0x22,0x18,0xa0,0x2218a0},
	{0x00,0x38,0x2d,0xb5,0x382db5},
	{0x00,0x48,0x3e,0xc7,0x483ec7},
	{0x00,0x58,0x4f,0xda,0x584fda},
	{0x00,0x61,0x59,0xec,0x6159ec},
	{0x00,0x6b,0x64,0xff,0x6b64ff},
	{0x00,0x7a,0x74,0xff,0x7a74ff},
	{0x00,0x7a,0x74,0xff,0x7a74ff},
	{0x00,0x8a,0x84,0xff,0x8a84ff},
	{0x00,0x91,0x8e,0xff,0x918eff},
	{0x00,0x99,0x98,0xff,0x9998ff},
	{0x00,0xa5,0xa3,0xff,0xa5a3ff},
	{0x00,0xb1,0xae,0xff,0xb1aeff},
	{0x00,0xb8,0xb8,0xff,0xb8b8ff},
	{0x00,0xc0,0xc2,0xff,0xc0c2ff},
	{0x00,0x1d,0x29,0x5a,0x1d295a},
	{0x00,0x1d,0x38,0x76,0x1d3876},
	{0x00,0x1d,0x48,0x92,0x1d4892},
	{0x00,0x1c,0x5c,0xac,0x1c5cac},
	{0x00,0x1c,0x71,0xc6,0x1c71c6},
	{0x00,0x32,0x86,0xcf,0x3286cf},
	{0x00,0x48,0x9b,0xd9,0x489bd9},
	{0x00,0x4e,0xa8,0xec,0x4ea8ec},
	{0x00,0x4e,0xa8,0xec,0x4ea8ec},
	{0x00,0x55,0xb6,0xff,0x55b6ff},
	{0x00,0x70,0xc7,0xff,0x70c7ff},
	{0x00,0x8c,0xd8,0xff,0x8cd8ff},
	{0x00,0x93,0xdb,0xff,0x93dbff},
	{0x00,0x9b,0xdf,0xff,0x9bdfff},
	{0x00,0xaf,0xe4,0xff,0xafe4ff},
	{0x00,0xc3,0xe9,0xff,0xc3e9ff},
	{0x00,0x2f,0x43,0x02,0x2f4302},
	{0x00,0x39,0x52,0x02,0x395202},
	{0x00,0x44,0x61,0x03,0x446103},
	{0x00,0x41,0x7a,0x12,0x417a12},
	{0x00,0x3e,0x94,0x21,0x3e9421},
	{0x00,0x4a,0x9f,0x2e,0x4a9f2e},
	{0x00,0x57,0xab,0x3b,0x57ab3b},
	{0x00,0x5c,0xbd,0x55,0x5cbd55},
	{0x00,0x5c,0xbd,0x55,0x5cbd55},
	{0x00,0x61,0xd0,0x70,0x61d070},
	{0x00,0x69,0xe2,0x7a,0x69e27a},
	{0x00,0x72,0xf5,0x84,0x72f584},
	{0x00,0x7c,0xfa,0x8d,0x7cfa8d},
	{0x00,0x87,0xff,0x97,0x87ff97},
	{0x00,0x9a,0xff,0xa6,0x9affa6},
	{0x00,0xad,0xff,0xb6,0xadffb6},
	{0x00,0x0a,0x41,0x08,0x0a4108},
	{0x00,0x0d,0x54,0x0a,0x0d540a},
	{0x00,0x10,0x68,0x0d,0x10680d},
	{0x00,0x13,0x7d,0x0f,0x137d0f},
	{0x00,0x16,0x92,0x12,0x169212},
	{0x00,0x19,0xa5,0x14,0x19a514},
	{0x00,0x1c,0xb9,0x17,0x1cb917},
	{0x00,0x1e,0xc9,0x19,0x1ec919},
	{0x00,0x1e,0xc9,0x19,0x1ec919},
	{0x00,0x21,0xd9,0x1b,0x21d91b},
	{0x00,0x47,0xe4,0x2d,0x47e42d},
	{0x00,0x6e,0xf0,0x40,0x6ef040},
	{0x00,0x78,0xf7,0x4d,0x78f74d},
	{0x00,0x83,0xff,0x5b,0x83ff5b},
	{0x00,0x9a,0xff,0x7a,0x9aff7a},
	{0x00,0xb2,0xff,0x9a,0xb2ff9a},
	{0x00,0x04,0x41,0x0b,0x04410b},
	{0x00,0x05,0x53,0x0e,0x05530e},
	{0x00,0x06,0x66,0x11,0x066611},
	{0x00,0x07,0x77,0x14,0x077714},
	{0x00,0x08,0x88,0x3c,0x08883c},
	{0x00,0x09,0x9b,0x40,0x099b40},
	{0x00,0x0b,0xaf,0x44,0x0baf44},
	{0x00,0x48,0xc4,0x48,0x48c448},
	{0x00,0x48,0xc4,0x4c,0x48c44c},
	{0x00,0x86,0xd9,0x50,0x86d950},
	{0x00,0x8f,0xe9,0x54,0x8fe954},
	{0x00,0x99,0xf9,0x56,0x99f956},
	{0x00,0xa8,0xfc,0x58,0xa8fc58},
	{0x00,0xb7,0xff,0x5b,0xb7ff5b},
	{0x00,0xc9,0xff,0x6e,0xc9ff6e},
	{0x00,0xdc,0xff,0x81,0xdcff81},
	{0x00,0x02,0x35,0x0f,0x02350f},
	{0x00,0x07,0x3f,0x15,0x073f15},
	{0x00,0x0c,0x4a,0x1c,0x0c4a1c},
	{0x00,0x2d,0x5f,0x1e,0x2d5f1e},
	{0x00,0x4f,0x74,0x20,0x4f7420},
	{0x00,0x59,0x83,0x24,0x598324},
	{0x00,0x64,0x92,0x28,0x649228},
	{0x00,0x82,0xa1,0x2e,0x82a12e},
	{0x00,0x82,0xa1,0x2e,0x82a12e},
	{0x00,0xa1,0xb0,0x34,0xa1b034},
	{0x00,0xa9,0xc1,0x3a,0xa9c13a},
	{0x00,0xb2,0xd2,0x41,0xb2d241},
	{0x00,0xc4,0xd9,0x45,0xc4d945},
	{0x00,0xd6,0xe1,0x49,0xd6e149},
	{0x00,0xe4,0xf0,0x4e,0xe4f04e},
	{0x00,0xf2,0xff,0x53,0xf2ff53},
	{0x00,0x26,0x30,0x01,0x263001},
	{0x00,0x24,0x38,0x03,0x243803},
	{0x00,0x23,0x40,0x05,0x234005},
	{0x00,0x51,0x54,0x1b,0x51541b},
	{0x00,0x80,0x69,0x31,0x806931},
	{0x00,0x97,0x81,0x35,0x978135},
	{0x00,0xaf,0x99,0x3a,0xaf993a},
	{0x00,0xc2,0xa7,0x3e,0xc2a73e},
	{0x00,0xc2,0xa7,0x3e,0xc2a73e},
	{0x00,0xd5,0xb5,0x43,0xd5b543},
	{0x00,0xdb,0xc0,0x3d,0xdbc03d},
	{0x00,0xe1,0xcb,0x38,0xe1cb38},
	{0x00,0xe2,0xd8,0x36,0xe2d836},
	{0x00,0xe3,0xe5,0x34,0xe3e534},
	{0x00,0xef,0xf2,0x58,0xeff258},
	{0x00,0xfb,0xff,0x7d,0xfbff7d},
	{0x00,0x39,0x17,0x01,0x391701},
	{0x00,0x5e,0x23,0x04,0x5e2304},
	{0x00,0x83,0x30,0x08,0x833008},
	{0x00,0xa5,0x47,0x16,0xa54716},
	{0x00,0xc8,0x5f,0x24,0xc85f24},
	{0x00,0xe3,0x78,0x20,0xe37820},
	{0x00,0xff,0x91,0x1d,0xff911d},
	{0x00,0xff,0xab,0x1d,0xffab1d},
	{0x00,0xff,0xab,0x1d,0xffab1d},
	{0x00,0xff,0xc5,0x1d,0xffc51d},
	{0x00,0xff,0xce,0x34,0xffce34},
	{0x00,0xff,0xd8,0x4c,0xffd84c},
	{0x00,0xff,0xe6,0x51,0xffe651},
	{0x00,0xff,0xf4,0x56,0xfff456},
	{0x00,0xff,0xf9,0x77,0xfff977},
	{0x00,0xff,0xff,0x98,0xffff98}
};
///

/// GTIA::NTSCColorMap
// The colormap of GTIA in NTSC mode
const struct ColorEntry GTIA::NTSCColorMap[256] = {
	{0x00,0x00,0x00,0x00,0x000000},
	{0x00,0x36,0x36,0x36,0x363636},
	{0x00,0x51,0x51,0x51,0x515151},
	{0x00,0x66,0x66,0x66,0x666666},
	{0x00,0x78,0x78,0x78,0x787878},
	{0x00,0x88,0x88,0x88,0x888888},
	{0x00,0x97,0x97,0x97,0x979797},
	{0x00,0xa5,0xa5,0xa5,0xa5a5a5},
	{0x00,0xb2,0xb2,0xb2,0xb2b2b2},
	{0x00,0xbe,0xbe,0xbe,0xbebebe},
	{0x00,0xca,0xca,0xca,0xcacaca},
	{0x00,0xd5,0xd5,0xd5,0xd5d5d5},
	{0x00,0xe0,0xe0,0xe0,0xe0e0e0},
	{0x00,0xeb,0xeb,0xeb,0xebebeb},
	{0x00,0xf5,0xf5,0xf5,0xf5f5f5},
	{0x00,0xff,0xff,0xff,0xffffff},
	{0x00,0x6a,0x2b,0x00,0x6a2b00},
	{0x00,0x7c,0x49,0x00,0x7c4900},
	{0x00,0x8c,0x5f,0x00,0x8c5f00},
	{0x00,0x9b,0x72,0x00,0x9b7200},
	{0x00,0xa8,0x83,0x00,0xa88300},
	{0x00,0xb5,0x92,0x21,0xb59221},
	{0x00,0xc1,0xa0,0x43,0xc1a043},
	{0x00,0xcd,0xae,0x5a,0xcdae5a},
	{0x00,0xd8,0xba,0x6e,0xd8ba6e},
	{0x00,0xe3,0xc6,0x7f,0xe3c67f},
	{0x00,0xed,0xd2,0x8f,0xedd28f},
	{0x00,0xf7,0xdc,0x9d,0xf7dc9d},
	{0x00,0xff,0xe7,0xab,0xffe7ab},
	{0x00,0xff,0xf1,0xb7,0xfff1b7},
	{0x00,0xff,0xfb,0xc3,0xfffbc3},
	{0x00,0xff,0xff,0xcf,0xffffcf},
	{0x00,0x7f,0x00,0x00,0x7f0000},
	{0x00,0x8e,0x29,0x00,0x8e2900},
	{0x00,0x9d,0x47,0x00,0x9d4700},
	{0x00,0xaa,0x5e,0x00,0xaa5e00},
	{0x00,0xb7,0x71,0x1e,0xb7711e},
	{0x00,0xc3,0x82,0x41,0xc38241},
	{0x00,0xcf,0x91,0x59,0xcf9159},
	{0x00,0xda,0xa0,0x6d,0xdaa06d},
	{0x00,0xe4,0xad,0x7e,0xe4ad7e},
	{0x00,0xef,0xba,0x8e,0xefba8e},
	{0x00,0xf9,0xc6,0x9c,0xf9c69c},
	{0x00,0xff,0xd1,0xaa,0xffd1aa},
	{0x00,0xff,0xdc,0xb6,0xffdcb6},
	{0x00,0xff,0xe7,0xc3,0xffe7c3},
	{0x00,0xff,0xf1,0xce,0xfff1ce},
	{0x00,0xff,0xfb,0xd9,0xfffbd9},
	{0x00,0x83,0x00,0x00,0x830000},
	{0x00,0x92,0x00,0x00,0x920000},
	{0x00,0xa1,0x2a,0x06,0xa12a06},
	{0x00,0xae,0x48,0x37,0xae4837},
	{0x00,0xba,0x5f,0x51,0xba5f51},
	{0x00,0xc6,0x72,0x66,0xc67266},
	{0x00,0xd2,0x83,0x78,0xd28378},
	{0x00,0xdd,0x92,0x88,0xdd9288},
	{0x00,0xe7,0xa0,0x97,0xe7a097},
	{0x00,0xf2,0xad,0xa5,0xf2ada5},
	{0x00,0xfb,0xba,0xb2,0xfbbab2},
	{0x00,0xff,0xc6,0xbf,0xffc6bf},
	{0x00,0xff,0xd1,0xca,0xffd1ca},
	{0x00,0xff,0xdc,0xd6,0xffdcd6},
	{0x00,0xff,0xe7,0xe0,0xffe7e0},
	{0x00,0xff,0xf1,0xeb,0xfff1eb},
	{0x00,0x79,0x00,0x0c,0x79000c},
	{0x00,0x89,0x00,0x38,0x890038},
	{0x00,0x98,0x00,0x52,0x980052},
	{0x00,0xa5,0x35,0x67,0xa53567},
	{0x00,0xb2,0x4f,0x79,0xb24f79},
	{0x00,0xbf,0x65,0x89,0xbf6589},
	{0x00,0xca,0x77,0x98,0xca7798},
	{0x00,0xd6,0x87,0xa6,0xd687a6},
	{0x00,0xe1,0x96,0xb3,0xe196b3},
	{0x00,0xeb,0xa4,0xbf,0xeba4bf},
	{0x00,0xf5,0xb1,0xcb,0xf5b1cb},
	{0x00,0xff,0xbe,0xd6,0xffbed6},
	{0x00,0xff,0xc9,0xe1,0xffc9e1},
	{0x00,0xff,0xd5,0xeb,0xffd5eb},
	{0x00,0xff,0xe0,0xf5,0xffe0f5},
	{0x00,0xff,0xea,0xff,0xffeaff},
	{0x00,0x5e,0x00,0x53,0x5e0053},
	{0x00,0x71,0x00,0x68,0x710068},
	{0x00,0x82,0x00,0x7a,0x82007a},
	{0x00,0x91,0x2a,0x8a,0x912a8a},
	{0x00,0x9f,0x48,0x99,0x9f4899},
	{0x00,0xad,0x5f,0xa6,0xad5fa6},
	{0x00,0xb9,0x72,0xb3,0xb972b3},
	{0x00,0xc5,0x83,0xc0,0xc583c0},
	{0x00,0xd1,0x92,0xcb,0xd192cb},
	{0x00,0xdc,0xa0,0xd6,0xdca0d6},
	{0x00,0xe6,0xad,0xe1,0xe6ade1},
	{0x00,0xf1,0xba,0xec,0xf1baec},
	{0x00,0xfb,0xc6,0xf6,0xfbc6f6},
	{0x00,0xff,0xd1,0xff,0xffd1ff},
	{0x00,0xff,0xdc,0xff,0xffdcff},
	{0x00,0xff,0xe7,0xff,0xffe7ff},
	{0x00,0x2a,0x00,0x73,0x2a0073},
	{0x00,0x48,0x00,0x84,0x480084},
	{0x00,0x5e,0x00,0x93,0x5e0093},
	{0x00,0x71,0x30,0xa1,0x7130a1},
	{0x00,0x82,0x4c,0xae,0x824cae},
	{0x00,0x92,0x62,0xbb,0x9262bb},
	{0x00,0xa0,0x75,0xc7,0xa075c7},
	{0x00,0xad,0x85,0xd2,0xad85d2},
	{0x00,0xba,0x94,0xdd,0xba94dd},
	{0x00,0xc6,0xa2,0xe8,0xc6a2e8},
	{0x00,0xd1,0xb0,0xf2,0xd1b0f2},
	{0x00,0xdc,0xbc,0xfc,0xdcbcfc},
	{0x00,0xe7,0xc8,0xff,0xe7c8ff},
	{0x00,0xf1,0xd3,0xff,0xf1d3ff},
	{0x00,0xfb,0xde,0xff,0xfbdeff},
	{0x00,0xff,0xe9,0xff,0xffe9ff},
	{0x00,0x00,0x00,0x82,0x000082},
	{0x00,0x00,0x00,0x91,0x000091},
	{0x00,0x26,0x21,0x9f,0x26219f},
	{0x00,0x46,0x42,0xad,0x4642ad},
	{0x00,0x5d,0x5a,0xb9,0x5d5ab9},
	{0x00,0x70,0x6e,0xc5,0x706ec5},
	{0x00,0x81,0x7f,0xd1,0x817fd1},
	{0x00,0x90,0x8f,0xdc,0x908fdc},
	{0x00,0x9f,0x9d,0xe6,0x9f9de6},
	{0x00,0xac,0xab,0xf1,0xacabf1},
	{0x00,0xb9,0xb7,0xfb,0xb9b7fb},
	{0x00,0xc5,0xc3,0xff,0xc5c3ff},
	{0x00,0xd0,0xcf,0xff,0xd0cfff},
	{0x00,0xdb,0xda,0xff,0xdbdaff},
	{0x00,0xe6,0xe5,0xff,0xe6e5ff},
	{0x00,0xf0,0xef,0xff,0xf0efff},
	{0x00,0x00,0x00,0x81,0x000081},
	{0x00,0x00,0x1d,0x91,0x001d91},
	{0x00,0x00,0x40,0x9f,0x00409f},
	{0x00,0x00,0x58,0xac,0x0058ac},
	{0x00,0x2e,0x6c,0xb9,0x2e6cb9},
	{0x00,0x4b,0x7e,0xc5,0x4b7ec5},
	{0x00,0x61,0x8d,0xd1,0x618dd1},
	{0x00,0x73,0x9c,0xdc,0x739cdc},
	{0x00,0x84,0xa9,0xe6,0x84a9e6},
	{0x00,0x93,0xb6,0xf0,0x93b6f0},
	{0x00,0xa1,0xc2,0xfa,0xa1c2fa},
	{0x00,0xaf,0xce,0xff,0xafceff},
	{0x00,0xbb,0xd9,0xff,0xbbd9ff},
	{0x00,0xc7,0xe4,0xff,0xc7e4ff},
	{0x00,0xd3,0xee,0xff,0xd3eeff},
	{0x00,0xdd,0xf8,0xff,0xddf8ff},
	{0x00,0x00,0x1f,0x71,0x001f71},
	{0x00,0x00,0x41,0x82,0x004182},
	{0x00,0x00,0x59,0x92,0x005992},
	{0x00,0x00,0x6d,0xa0,0x006da0},
	{0x00,0x00,0x7e,0xad,0x007ead},
	{0x00,0x28,0x8e,0xba,0x288eba},
	{0x00,0x47,0x9c,0xc6,0x479cc6},
	{0x00,0x5e,0xaa,0xd1,0x5eaad1},
	{0x00,0x71,0xb7,0xdc,0x71b7dc},
	{0x00,0x82,0xc3,0xe7,0x82c3e7},
	{0x00,0x91,0xce,0xf1,0x91cef1},
	{0x00,0x9f,0xda,0xfb,0x9fdafb},
	{0x00,0xad,0xe4,0xff,0xade4ff},
	{0x00,0xb9,0xef,0xff,0xb9efff},
	{0x00,0xc5,0xf9,0xff,0xc5f9ff},
	{0x00,0xd1,0xff,0xff,0xd1ffff},
	{0x00,0x00,0x41,0x50,0x004150},
	{0x00,0x00,0x59,0x65,0x005965},
	{0x00,0x00,0x6c,0x77,0x006c77},
	{0x00,0x00,0x7e,0x88,0x007e88},
	{0x00,0x00,0x8e,0x97,0x008e97},
	{0x00,0x1c,0x9c,0xa5,0x1c9ca5},
	{0x00,0x3f,0xaa,0xb2,0x3faab2},
	{0x00,0x58,0xb6,0xbe,0x58b6be},
	{0x00,0x6c,0xc2,0xca,0x6cc2ca},
	{0x00,0x7d,0xce,0xd5,0x7dced5},
	{0x00,0x8d,0xd9,0xe0,0x8dd9e0},
	{0x00,0x9b,0xe4,0xea,0x9be4ea},
	{0x00,0xa9,0xee,0xf4,0xa9eef4},
	{0x00,0xb6,0xf8,0xfe,0xb6f8fe},
	{0x00,0xc2,0xff,0xff,0xc2ffff},
	{0x00,0xce,0xff,0xff,0xceffff},
	{0x00,0x00,0x52,0x00,0x005200},
	{0x00,0x00,0x67,0x34,0x006734},
	{0x00,0x00,0x79,0x4f,0x00794f},
	{0x00,0x00,0x89,0x64,0x008964},
	{0x00,0x00,0x98,0x77,0x009877},
	{0x00,0x35,0xa6,0x87,0x35a687},
	{0x00,0x50,0xb3,0x96,0x50b396},
	{0x00,0x65,0xbf,0xa4,0x65bfa4},
	{0x00,0x77,0xcb,0xb1,0x77cbb1},
	{0x00,0x87,0xd6,0xbd,0x87d6bd},
	{0x00,0x96,0xe1,0xc9,0x96e1c9},
	{0x00,0xa4,0xeb,0xd5,0xa4ebd5},
	{0x00,0xb1,0xf5,0xdf,0xb1f5df},
	{0x00,0xbe,0xff,0xea,0xbeffea},
	{0x00,0xca,0xff,0xf4,0xcafff4},
	{0x00,0xd5,0xff,0xfe,0xd5fffe},
	{0x00,0x00,0x58,0x00,0x005800},
	{0x00,0x00,0x6c,0x00,0x006c00},
	{0x00,0x00,0x7e,0x00,0x007e00},
	{0x00,0x20,0x8d,0x33,0x208d33},
	{0x00,0x42,0x9c,0x4e,0x429c4e},
	{0x00,0x5a,0xaa,0x64,0x5aaa64},
	{0x00,0x6d,0xb6,0x76,0x6db676},
	{0x00,0x7f,0xc2,0x86,0x7fc286},
	{0x00,0x8e,0xce,0x95,0x8ece95},
	{0x00,0x9d,0xd9,0xa3,0x9dd9a3},
	{0x00,0xaa,0xe4,0xb1,0xaae4b1},
	{0x00,0xb7,0xee,0xbd,0xb7eebd},
	{0x00,0xc3,0xf8,0xc9,0xc3f8c9},
	{0x00,0xcf,0xff,0xd4,0xcfffd4},
	{0x00,0xda,0xff,0xdf,0xdaffdf},
	{0x00,0xe4,0xff,0xea,0xe4ffea},
	{0x00,0x00,0x55,0x00,0x005500},
	{0x00,0x1f,0x69,0x00,0x1f6900},
	{0x00,0x41,0x7b,0x00,0x417b00},
	{0x00,0x59,0x8b,0x00,0x598b00},
	{0x00,0x6d,0x9a,0x1a,0x6d9a1a},
	{0x00,0x7e,0xa7,0x3e,0x7ea73e},
	{0x00,0x8e,0xb4,0x57,0x8eb457},
	{0x00,0x9c,0xc0,0x6b,0x9cc06b},
	{0x00,0xaa,0xcc,0x7d,0xaacc7d},
	{0x00,0xb7,0xd7,0x8c,0xb7d78c},
	{0x00,0xc3,0xe2,0x9b,0xc3e29b},
	{0x00,0xce,0xec,0xa9,0xceeca9},
	{0x00,0xd9,0xf6,0xb5,0xd9f6b5},
	{0x00,0xe4,0xff,0xc2,0xe4ffc2},
	{0x00,0xee,0xff,0xcd,0xeeffcd},
	{0x00,0xf8,0xff,0xd8,0xf8ffd8},
	{0x00,0x43,0x46,0x00,0x434600},
	{0x00,0x5b,0x5d,0x00,0x5b5d00},
	{0x00,0x6e,0x70,0x00,0x6e7000},
	{0x00,0x7f,0x81,0x00,0x7f8100},
	{0x00,0x8f,0x91,0x00,0x8f9100},
	{0x00,0x9d,0x9f,0x20,0x9d9f20},
	{0x00,0xab,0xac,0x42,0xabac42},
	{0x00,0xb8,0xb9,0x59,0xb8b959},
	{0x00,0xc4,0xc5,0x6d,0xc4c56d},
	{0x00,0xcf,0xd1,0x7e,0xcfd17e},
	{0x00,0xda,0xdc,0x8e,0xdadc8e},
	{0x00,0xe5,0xe6,0x9d,0xe5e69d},
	{0x00,0xef,0xf0,0xaa,0xeff0aa},
	{0x00,0xf9,0xfa,0xb7,0xf9fab7},
	{0x00,0xff,0xff,0xc3,0xffffc3},
	{0x00,0xff,0xff,0xcf,0xffffcf},
	{0x00,0x6a,0x2b,0x00,0x6a2b00},
	{0x00,0x7c,0x49,0x00,0x7c4900},
	{0x00,0x8c,0x5f,0x00,0x8c5f00},
	{0x00,0x9b,0x72,0x00,0x9b7200},
	{0x00,0xa8,0x83,0x00,0xa88300},
	{0x00,0xb5,0x92,0x21,0xb59221},
	{0x00,0xc1,0xa0,0x43,0xc1a043},
	{0x00,0xcd,0xae,0x5a,0xcdae5a},
	{0x00,0xd8,0xba,0x6e,0xd8ba6e},
	{0x00,0xe3,0xc6,0x7f,0xe3c67f},
	{0x00,0xed,0xd2,0x8f,0xedd28f},
	{0x00,0xf7,0xdc,0x9d,0xf7dc9d},
	{0x00,0xff,0xe7,0xab,0xffe7ab},
	{0x00,0xff,0xf1,0xb7,0xfff1b7},
	{0x00,0xff,0xfb,0xc3,0xfffbc3},
	{0x00,0xff,0xff,0xcf,0xffffcf}
};
///

/// GTIA::UpdateCollisions
// This method updates the collision masks of the player/missile collisions
// for the passed in player/playfield masks
void GTIA::UpdateCollisions(int pf, int pl,const UBYTE *collisionmask)
{
  int i;
  //
  // Extract collision bits already. Depending on the GTIA mode, not all
  // colors can collide (especially for the "processed modes").
  pf = collisionmask[pf];
  // Iterate over all players/missiles
  for(i=0;i<4;i++) {
    // Setup rendering target
    if (pl & Player[i].DisplayMask) {
      // Found that this object is active here, need to consider collisions of this object with others
      Player[i].CollisionPlayer     |= pl; // must mask out self-collisions later
      Player[i].CollisionPlayfield  |= pf;
    }
    if (pl & Missile[i].DisplayMask) {
      // Found that this object is active here, need to consider collisions of this object with others
      Missile[i].CollisionPlayer    |= pl; // must mask out self-collisions later
      Missile[i].CollisionPlayfield |= pf;
    }
  }
}
///

/// GTIA::PixelColor
// The following is the main method of the priority engine. It should
// better be fast as it is called on a per-pixel basis. Arguments are the playfield PreComputedColor,
// the player bitmask and the playfield color in Atari notation. The latter
// is required for the special color fiddling.
UBYTE GTIA::PixelColor(int pf_pixel,int pm_pixel,int pf_color)
{
  int pfidx   = pf_pixel;  // playfield color index into "PreComputedColor"
  int pfcol   = pf_color;  // playfield decoded color 
  
  // If we have any missiles active here and they are combined into
  // a third player, give them the priority and the color of this
  // playfield. Otherwise, share the priority of the player
  if ((pm_pixel & 0xf0) && MissilePF3) {
    // Ignore missiles for the following, the above handled them already.
    pm_pixel = (pm_pixel & 0x0f) | 0x10;
  } else {
    // Ignore missiles for the following, the above handled them already.
    pm_pixel = (pm_pixel & 0x0f) | (pm_pixel >> 4);
  }
  
  // Now check for the color of the playfield
  //  
  // Also combine all colors so far. This step also combines the colors by
  // or'ing as required by the priority engine. Priority is setup such
  // that the "beaten" color gets cleared out otherwise.
  switch(pfidx) {
  case Playfield_0:
  case Playfield_1:   
    // Now disable the background if the players have priority.
    pfcol &= Playfield01Mask[pm_pixel];
    // We have a playfield 0 or 1 underneath.
    pfcol |= ColorLookup[Player0ColorLookupPF01[pm_pixel]];
    pfcol |= ColorLookup[Player2ColorLookupPF01[pm_pixel]];
    pfcol |= ColorLookup[Player4ColorLookupPF01[pm_pixel]];
    break;
  case Playfield_1_Fiddled:
  case Playfield_Artifact1:
  case Playfield_Artifact2:
    // The special fiddled color is first understood as ColPF2
    // and even shares the priority of ColPF2. Same goes for
    // the remaining color fiddling entries for 0,1 and 1,0
    // transitions.
    pfcol  = ColorLookup[Playfield_2];
    // runs intentionally into the following...
  case Playfield_2:
  case Playfield_3:
    // Now disable the playfield if the players have priority.
    pfcol &= Playfield23Mask[pm_pixel];
    // We have playfield 2 or 3 underneath
    pfcol |= ColorLookup[Player0ColorLookupPF23[pm_pixel]];
    pfcol |= ColorLookup[Player2ColorLookupPF23[pm_pixel]];
    pfcol |= ColorLookup[Player4ColorLookupPF23[pm_pixel]];
    break;
  default:
    // Background, players are always visible, just use pre-computed inter-player
    // colors.    
    pfcol  = 0;
    pfcol |= ColorLookup[Player0ColorLookup[pm_pixel]];
    pfcol |= ColorLookup[Player2ColorLookup[pm_pixel]];
    pfcol |= ColorLookup[Player4ColorLookup[pm_pixel]];
    break;
  }
  
  // Now emulate color fiddling for antic 2,3 and F, including the
  // artifacting.
  if (pf_pixel == Playfield_1_Fiddled) {
    pfcol = (pfcol & 0xf0) | (ColorLookup[pf_pixel] & 0x0f);
  }
  
  return UBYTE(pfcol);
}
///

/// GTIA::IntermediateResolverUnfiddled::IntermediateResolverUnfiddled
GTIA::IntermediateResolverUnfiddled::IntermediateResolverUnfiddled(void)
{
  // Table entries for first, second half color clock are identically since both
  // pixels are always identically on unfiddled modes
  static const IntermediateLut lut = {
    {
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x04, 0x08, 0x0c,  // PF 0,1,2,3
      0x00, 0x04, 0x04, 0x04,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x04, 0x08, 0x0c,  // PF 0,1,2,3
      0x00, 0x04, 0x04, 0x04,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {   
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x01, 0x02, 0x03,  // PF 0,1,2,3
      0x00, 0x01, 0x01, 0x01,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {   
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x01, 0x02, 0x03,  // PF 0,1,2,3
      0x00, 0x01, 0x01, 0x01,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    }
  };

  Lut = lut;
}
///

/// GTIA::IntermediateResolverFiddled::IntermediateResolverFiddled
GTIA::IntermediateResolverFiddled::IntermediateResolverFiddled(void)
{
  // Tables for all four half-color clocks of a CPU cycle.
  static const IntermediateLut lut = {
    {  
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x08, 0x00, 0x00,  // PF 0,1,2,3
      0x00, 0x08, 0x08, 0x08,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x04, 0x00, 0x00,  // PF 0,1,2,3
      0x00, 0x04, 0x04, 0x04,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x02, 0x00, 0x00,  // PF 0,1,2,3
      0x00, 0x02, 0x02, 0x02,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {    
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x01, 0x00, 0x00,  // PF 0,1,2,3
      0x00, 0x01, 0x01, 0x01,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    }
  };

  Lut = lut;
}
///

/// GTIA::DisplayGenerator00Unfiddled::DisplayGenerator00Unfiddled
GTIA::DisplayGenerator00Unfiddled::DisplayGenerator00Unfiddled(class GTIA *parent)
  : DisplayGenerator00Preprocessor(parent)
{
  // Collision bits generated for player-playfield collisions indexed
  // by the playfield color index, unfiddled colors.
  static const UBYTE Playfield_Collision_Mask[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,  // These slots are used up by player collisions
    0x01, 0x02, 0x04, 0x08,  // Collisions with Playfields 0,1,2,3
    0x00, 0x02, 0x02, 0x02,  // Collisions with background, and fiddled color
    0x00, 0x00, 0x00, 0x00
  };
  
  CollisionMask = Playfield_Collision_Mask;
}
///

/// GTIA::DisplayGenerator00Unfiddled::PostProcessClock
void GTIA::DisplayGenerator00Unfiddled::PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player)
{
  
  if (player[0] | player[1] | player[2] | player[3]) {
    int i = 4;
    do {
      // Check whether we need the priority engine. Avoid it if it is not
      // necessary.
      if (*player) {
	// Yes, priority engine must run
	gtia->UpdateCollisions(*playfield,*player,CollisionMask);
	*out = gtia->PixelColor(*playfield,*player,ColorLookup[*playfield]);
      } else {
	// Otherwise insert manually and handle color fiddling itself. This is
	// done already in the color lookup table.
	*out = ColorLookup[*playfield];
      }
      player++,playfield++,out++;
      // The compiler knows better whether or if to unroll this loop.
    } while(--i);
  } else {
    // Fast and most frequent choice
    out[0] = ColorLookup[playfield[0]];
    out[1] = ColorLookup[playfield[1]];
    out[2] = ColorLookup[playfield[2]];
    out[3] = ColorLookup[playfield[3]];
  }
}
///

/// GTIA::DisplayGenerator00Fiddled::DisplayGenerator00Fiddled
GTIA::DisplayGenerator00Fiddled::DisplayGenerator00Fiddled(class GTIA *parent)
  : DisplayGenerator00Preprocessor(parent)
{
  // The very same, but with fiddled access. This is strange: Only detections
  // with playfield one are detected, and are reported as collisions with
  // playfield two.
  static const UBYTE Playfield_Collision_Mask_Fiddled[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,  // These slots are used up by player collisions
    0x00, 0x04, 0x00, 0x00,  // Collisions with Playfields 0,1,2,3
    0x00, 0x04, 0x04, 0x04,  // Collisions with background, and fiddled color
    0x00, 0x00, 0x00, 0x00
  };
  
  CollisionMask = Playfield_Collision_Mask_Fiddled;
}
///

/// GTIA::DisplayGenerator00Fiddled::PostProcessClock
void GTIA::DisplayGenerator00Fiddled::PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player)
{  

  if (player[0] | player[1] | player[2] | player[3]) {
    int i = 4;
    do {
      // Check whether we need the priority engine. Avoid it if it is not
      // necessary.
      if (*player) {
	// Yes, priority engine must run
	gtia->UpdateCollisions(*playfield,*player,CollisionMask);
	*out = gtia->PixelColor(*playfield,*player,ColorLookup[*playfield]);
      } else {
	// Otherwise insert manually and handle color fiddling itself. This is
	// done already in the color lookup table.
      *out = ColorLookup[*playfield];
      }
      player++,playfield++,out++;
      // The compiler knows better whether or if to unroll this loop.
    } while(--i);
  } else { 
    // Fast and most frequent choice
    out[0] = ColorLookup[playfield[0]];
    out[1] = ColorLookup[playfield[1]];
    out[2] = ColorLookup[playfield[2]];
    out[3] = ColorLookup[playfield[3]];
  }
}
///

/// GTIA::DisplayGenerator00FiddledArtefacted::DisplayGenerator00FiddledArtefacted
GTIA::DisplayGenerator00FiddledArtefacted::DisplayGenerator00FiddledArtefacted(class GTIA *parent)
  : DisplayGenerator00Preprocessor(parent)
{
  // The very same.
  static const UBYTE Playfield_Collision_Mask_Fiddled[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,  // These slots are used up by player collisions
    0x00, 0x04, 0x00, 0x00,  // Collisions with Playfields 0,1,2,3
    0x00, 0x04, 0x04, 0x04,  // Collisions with background, and fiddled color
    0x00, 0x00, 0x00, 0x00
  };
  
  CollisionMask = Playfield_Collision_Mask_Fiddled;
}
///

/// GTIA::DisplayGenerator00FiddledArtefacted::PostProcessClock
void GTIA::DisplayGenerator00FiddledArtefacted::PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player)
{
  int i      = 4;
  int pf,diff;
  UBYTE back;  // background color for fiddled artefacts.
  
  do {    
    // Check which kind of color we handle. We require special
    // treathment of the fiddled color #1.
    pf   = *playfield;
    last = (last << 4) | pf;
    //
    // Update player collisions if we have players and
    // the background color.
    if (*player) {
      gtia->UpdateCollisions(*playfield,*player,CollisionMask);
      back = gtia->PixelColor(pf,*player,ColorLookup[pf]);
    } else {
      back = ColorLookup[pf];
    }
    //
    // Now to the color fiddling computations. This is a bit touchy.
    if (last == ((Playfield_1_Fiddled<<4) | Playfield_2) ||
	last == ((Playfield_2<<4) | Playfield_1_Fiddled)) {
      // Could be artifacting.
      // Compute the difference in value between background color and the
      // artifacting color (COLPF1). This defines the artefact hue.
      // Also get the other color contributing to the artifact.
      // Get the hue difference. This generates the signal in the color carrier.
      diff  = (back & 0x0f) - (other & 0x0f);
      if (diff) {
	// Now combine colors to the first artefacted color for a dark->light
	// edge, and to the second for a light->dark edge.
	// Now combine the hue from the hue of the background with the weighted
	// average of the values.
	*out  = HueMix[((back & 0xf0) >> 3) | (((diff >> 4) ^ i) & 1)] + (((other & 0x0f) + (back  & 0x0f)) >> 1);
      } else {
	// No artifacting since no difference in value.
	*out  = back;
      }
    } else {
      // Check whether we need the priority engine. Avoid it if it is not
      // necessary.
      *out = back;
    }
    //
    //
    other = back;
    player++,playfield++,out++;
    // The compiler knows better whether or if to unroll this loop.
  } while(--i);
}
///

/// GTIA::DisplayGenerator40Base::DisplayGenerator40Base
GTIA::DisplayGenerator40Base::DisplayGenerator40Base(class GTIA *parent)
  : DisplayGeneratorBase(parent)
{
  // No collisions are detected in this mode, hence clear the collision mask
  // completely
  static const UBYTE collisionmask[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  
  CollisionMask = collisionmask;
}
///

/// GTIA::DisplayGenerator40Base::PostProcessClock
void GTIA::DisplayGenerator40Base::PostProcessClock(UBYTE *out,UBYTE *pf,UBYTE *player)
{
  int i = 4; // four half color clocks at once
  UBYTE playfield;  // playfield color is constant along the clock

  // Get all the four pixel values by looking them up in the arrays.
  playfield = UBYTE(Lut[0][pf[0]] | Lut[1][pf[1]] | Lut[2][pf[2]] | Lut[3][pf[3]]);
  do {
    // Check whether we need the priority engine. Avoid it if it is not
    // necessary. Players always have priority in this mode regardless
    // of the color in the playfield. Colors are already pre-processed and
    // non-indexed here, we only have to or-into the background color
    if (*player) {
      UBYTE bgcolor;
      UBYTE playdat = *player;
      // Yes, priority engine must run: Players have always priority here.
      gtia->UpdateCollisions(playfield,playdat,CollisionMask);
      bgcolor = ColorLookup[Background];
      // A special quirk: Player 4 (missiles) do not overlay, but mix
      if ((playdat & 0xf0) && gtia->MissilePF3) {
	bgcolor  = ColorLookup[Playfield_3];
	playdat &= 0x0f;
      }
      if (playdat) {
	// Not a typo, the original missile priority is still relevant.
	*out = gtia->PixelColor(Background,*player,playfield | bgcolor);
      } else {
	*out = UBYTE(playfield | bgcolor);
      }
    } else {
      // Otherwise insert manually and handle color fiddling itself. This is
      // done already in the color lookup table.
      *out = UBYTE(playfield | ColorLookup[Background]);
    }    
    player++,out++;
    //
    // Leave it to the compiler to decide about loop-unrolling
  } while(--i);
}
///

/// GTIA::DisplayGenerator80Base::DisplayGenerator80Base
GTIA::DisplayGenerator80Base::DisplayGenerator80Base(class GTIA *parent)
  : DisplayGeneratorBase(parent)
{
  static const UBYTE collisionmask[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,  // These slots are used up by player collisions
    0x01, 0x02, 0x04, 0x08,  // Collisions with Playfields 0,1,2,3
    0x00, 0x02, 0x02, 0x02,  // Collisions with background, and fiddled color
    0x00, 0x00, 0x00, 0x00
  };
  CollisionMask = collisionmask;
}
///

/// GTIA::DisplayGenerator80Unfiddled::DisplayGenerator80Unfiddled
GTIA::DisplayGenerator80Unfiddled::DisplayGenerator80Unfiddled(class GTIA *parent)
  : DisplayGenerator80Base(parent)
{ 
  // Table entries for first, second half color clock are identically since both
  // pixels are always identically on unfiddled modes
  // This is a bit special since the combination PF1,BG is resolved in a different
  // way here.
  static const IntermediateLut lut = {
    {
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x04, 0x08, 0x0c,  // PF 0,1,2,3
      0x00, 0x04, 0x04, 0x04,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x04, 0x08, 0x0c,  // PF 0,1,2,3
      0x00, 0x04, 0x04, 0x04,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x00   // Player combined colors and background
    },
    {   
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x01, 0x02, 0x03,  // PF 0,1,2,3
      0x10, 0x01, 0x01, 0x01,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x10   // Player combined colors and background
    },
    {   
      0x00, 0x00, 0x00, 0x00,  // These slots are used up by players
      0x00, 0x01, 0x02, 0x03,  // PF 0,1,2,3
      0x10, 0x01, 0x01, 0x01,  // BK,Fiddled1,Fiddled2,Fiddled3
      0x00, 0x00, 0x00, 0x10   // Player combined colors and background
    }
  };

  Lut = lut;
}
///

/// GTIA::DisplayGenerator80Base::PostProcessClock
void GTIA::DisplayGenerator80Base::PostProcessClock(UBYTE *out,UBYTE *playfield,UBYTE *player)
{
  int i = 4; // four half color clocks at once
  UBYTE pf0,pf1,*pf = playfield + 2;
  // The following array translates nibble indices into PreComputedColor indices
  // The additional 16 entries are used in case the first color is PF1. Strangely
  // enough, the combination PF1,BG does not resolve to PF0, but to BG.
  static const UBYTE GTIAXLate[32] = {
    Player_0   ,Player_1   ,Player_2   ,Player_3,
    Playfield_0,Playfield_1,Playfield_2,Playfield_3,
    Background ,Background ,Background ,Background,
    Playfield_0,Playfield_1,Playfield_2,Playfield_3,  
    // The following entries are used if the second pair of half color clocks are BG.
    // Thus, only the first column is in use.
    Player_0   ,Player_1   ,Player_2   ,Player_3, // Is this incorrect?
    //Background ,Background ,Background ,Background,
    Background ,Background ,Background ,Background,
    Background ,Background ,Background ,Background,
    Background ,Background ,Background ,Background
  }; 
  // Additional player masks generated by the playfield, indexed by the PreComputedColor
  static const UBYTE GTIAPlayerMask[PreComputedEntries] = {
    0x01,0x02,0x04,0x08,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00
  };
  
  // Get all the four pixel values by looking them up in the arrays.
  // unfortunately, the pixels are not constant aLONG the clock as we
  // have a shift here.
  pf0    = pf[-2];
  pf1    = pf[-1];
  pf[-1] = pf[-2] = oc; // insert last pixel here, delayed by a color clock
  oc     = GTIAXLate[Lut[0][pf0] | Lut[1][pf1] | Lut[2][pf[0]] | Lut[3][pf[1]]];
  pf[0]  = pf[1]  = oc;
  
  do {    
    // Check whether we need the priority engine. Avoid it if it is not
    // necessary.
    if (*player) {
      UBYTE playermask = *player;
      //
      // No, players do not detect collision with the additional "player" colors as player-player
      // collisions.
      gtia->UpdateCollisions(*playfield,playermask,CollisionMask);
      // Yes, priority engine must run
      // Color mixing fixup: Note that here the playfield may use player colors,
      // and hence player priorities.
      *out = gtia->PixelColor(*playfield,playermask | GTIAPlayerMask[*playfield],ColorLookup[*playfield]);
    } else {
      // Otherwise insert manually and handle color fiddling itself. This is
      // done already in the color lookup table.
      *out = ColorLookup[*playfield];
    }    
    //
    player++,playfield++,out++;
    //
    // Leave it to the compiler to decide about loop-unrolling
  } while(--i);
}
///

/// GTIA::DisplayGeneratorC0Base::DisplayGeneratorC0Base
GTIA::DisplayGeneratorC0Base::DisplayGeneratorC0Base(class GTIA *parent)
  : DisplayGeneratorBase(parent)
{
  // No collisions are detected in this mode, hence clear the collision mask
  // completely
  static const UBYTE collisionmask[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  
  CollisionMask = collisionmask;
}
///

/// GTIA::DisplayGeneratorC0Base::PostProcessClock
void GTIA::DisplayGeneratorC0Base::PostProcessClock(UBYTE *out,UBYTE *pf,UBYTE *player)
{
  int i = 4; // four half color clocks at once
  UBYTE playfield;
  //
  // Get all the four pixel values by looking them up in the arrays, they
  // remain constant aLONG the clock cycle
  playfield = UBYTE(Lut[0][pf[0]] | Lut[1][pf[1]] | Lut[2][pf[2]] | Lut[3][pf[3]]);  
  //
  do {
    UBYTE hue;
    // Check whether we need the priority engine. Avoid it if it is not
    // necessary. Players always have priority in this mode regardless
    // of the color in the playfield. Colors are already pre-processed and
    // non-indexed here, we just have to insert the value from the background
    // color here.    
    // Special rule for hue zero here: This is always set to dark
    hue = UBYTE(playfield << 4);
    if (hue) {
      hue |= ColorLookup[Background]; // or in the background color with its value
    } else {
      hue |= ColorLookup[Background] & 0xf0; // only the hue, not the value. Leave it dark.
    }
    if (*player) {
      UBYTE playdat = *player;
      // Yes, priority engine must run: Players have always priority here.
      gtia->UpdateCollisions(playfield,playdat,CollisionMask); 
      // A special quirk: Player 4 (missiles) do not overlay, but mix
      if ((*player & 0xf0) && gtia->MissilePF3) {
	hue = UBYTE(playfield << 4);
	if (hue) {
	  hue |= ColorLookup[Playfield_3]; // or in the background color with its value
	} else {
	  hue |= ColorLookup[Playfield_3] & 0xf0; // only the hue, not the value. Leave it dark.
	}
	playdat &= 0x0f;
      }
      if (playdat) {
	// Not a typo, the original missile priority is still relevant.
	*out = gtia->PixelColor(Background,*player,hue);
      } else {
	*out = hue;
      }
    } else {
      // Otherwise insert manually and handle color fiddling itself. This is
      // done already in the color lookup table.
      *out = hue;
    }
    player++,out++;
    //
    // Leave it to the compiler to decide about loop-unrolling
  } while(--i);
}
///

/// GTIA::DisplayGeneratorStrangeBase::DisplayGeneratorStrangeBase
GTIA::DisplayGeneratorStrangeBase::DisplayGeneratorStrangeBase(class GTIA *parent)
  : DisplayGeneratorBase(parent)
{
  // Collision bits generated for player-playfield collisions indexed
  // by the playfield color index, unfiddled colors.
  // Strange enough, the player really "sees" the playfield colors here
  // for collision purposes.
  static const UBYTE collisionmask[GTIA::PreComputedEntries] = {
    0x00, 0x00, 0x00, 0x00,  // These slots are used up by player collisions
    0x01, 0x02, 0x04, 0x08,  // Collisions with Playfields 0,1,2,3
    0x00, 0x02, 0x02, 0x02,  // Collisions with background, and fiddled color
    0x00, 0x00, 0x00, 0x00
  };
  
  CollisionMask = collisionmask;
}
///

/// GTIA::DisplayGeneratorStrangeBase::PostProcessClock
void GTIA::DisplayGeneratorStrangeBase::PostProcessClock(UBYTE *out,UBYTE *pf,UBYTE *player)
{
  int i = 4; // four half color clocks at once
  UBYTE combinedcolor;
  static const UBYTE NibbleMapping[4] = {
    Playfield_0,Playfield_1,Playfield_2,Playfield_3 // strange enough, the background maps to PF0 etc...
  };
  //
  // Since the Antic frame is aligned to two color-clock boundaries
  // (and even more), it is sufficient to check the first pixel.
  if (pf[0] == Background) {
    pf[0] = pf[1] = pf[2] = pf[3] = Background;
  } else {  
    // First compute the color of the full nibble
    combinedcolor = UBYTE(Lut[0][pf[0]] | Lut[1][pf[1]] | Lut[2][pf[2]] | Lut[3][pf[3]]); 
    // Extract again two-byte pairs forming the two color clocks (strange indeed).
    // Map to background in case one of the pixels is the frame background, not the
    // playfield background.
    pf[0] = pf[1] = NibbleMapping[combinedcolor   >> 2];
    pf[2] = pf[3] = NibbleMapping[combinedcolor & 0x03];
  }
  
  do {
    // Check whether we need the priority engine. Avoid it if it is not
    // necessary. Players always have priority in this mode regardless
    // of the color in the playfield. Colors are already pre-processed and
    // non-indexed here, we only have to or-into the background color
    if (*player) {
      // Yes, priority engine must run: Players have always priority here.
      gtia->UpdateCollisions(*pf,*player,CollisionMask);
      *out = gtia->PixelColor(Background,*player,ColorLookup[*pf]);
    } else {
      // Otherwise insert manually and handle color fiddling itself. This is
      // done already in the color lookup table.
      *out = UBYTE(ColorLookup[*pf]);
    }    
    player++,pf++,out++;
    //
    // Leave it to the compiler to decide about loop-unrolling
  } while(--i);
}
///

/// GTIA::PickModeGenerator
// Pick the proper mode generator, depending on the PRIOR and the fiddling
// values.
void GTIA::PickModeGenerator(UBYTE prior)
{
  UBYTE mode;
  // This only depends on PRIOR, and on
  // whether fiddling is on or off. CTIA
  // does not provide the modes.
  mode = (ChipGeneration == CTIA)?0:prior & 0xc0;
  //
  switch(mode) {
  case 0x00:
    // Unprocessed mode. However, if we
    // had a processed mode 0x40 or 0xc0
    // at the start of the line and turn it
    // off now, we enter the "strange mode".
    if (InitialPrior & 0xc0) {
      if (Fiddling) {
	CurrentGenerator   = ModeSF;
      } else {
	CurrentGenerator   = ModeSUF;
      } 
    } else {
      if (Fiddling) {
	if (ColPF1FiddledArtifacts) {
	  // Generate with artefacts
	  CurrentGenerator = Mode00FA;
	} else {
	  // No artefacts, but color fiddling.
	  CurrentGenerator = Mode00F;
	}
      } else {
	// Regular mode.
	CurrentGenerator   = Mode00UF;
      }
    }
    break;
  case 0x40:
    if (Fiddling) {
      CurrentGenerator     = Mode40F;
    } else {
      CurrentGenerator     = Mode40UF;
    }
    break;
  case 0x80:
    if (Fiddling) {
      CurrentGenerator     = Mode80F;
    } else {
      CurrentGenerator     = Mode80UF;
    }
    break;
  case 0xc0:
    if (Fiddling) {
      CurrentGenerator     = ModeC0F;
    } else {
      CurrentGenerator     = ModeC0UF;
    }
    break;
  }
  //
  // Also if switched off in the middle of the line.
  InitialPrior |= Prior;
}
///

/// GTIA::WarmStart
// Reset the GTIA to its warm start values
void GTIA::WarmStart(void)
{
  int i;
  
  for(i=0;i<PreComputedEntries;i++)
    ColorLookup[i] = 0x00;  // reset all colors
  //
  // Setup artifacting color bases.
  SetupArtifacting();
  
  for(i=0;i<4;i++) {
    Player[i].Reset();
    Missile[i].Reset();
  }
  Fiddling                = false;
  InitialPrior            = 0x00;
  Prior                   = 0x00;

  UpdatePriorityEngine(0x00);
  PickModeGenerator(0x00);
  
  Gractl                  = 0x00;
  GractlShadow            = 0x00;
  VertDelay               = 0x00;
  HPos                    = 0;

  if (PostProcessor)
    PostProcessor->Reset();
}
///

/// GTIA::ColdStart
// Run the GTIA coldstart now
void GTIA::ColdStart(void)
{
  WarmStart();
}
///

/// GTIA::UpdatePriorityEngine
// This pre-computes all the data for the priority engine from the
// hardware registers.
void GTIA::UpdatePriorityEngine(UBYTE pri)
{
  int pm_pixel;  
  bool pl01beatspf;            // true if player 0,1 is on top of playfield 0,1
  bool pf01beatspl;            // true if playfield 0,1 is on top of player 0,1
  bool pl23beatspf;            // true if player 2,3 is on top of playfield 2,3
  bool pf23beatspl;            // true if playfield 2,3 is on top of player 2,3
  bool pl02beatspl;            // true if player 0,2 is on top of player 1,3
  bool pfbeatspl;              // true if player 0,1 behind playfield 2,3
  bool plbeatspf;              // true if player 2,3 in front of playfield 0,1
  //
  // Enter default priority settings
  MissilePF3    = (pri & 0x10)?(true):(false);  // missles as third player?
  pfbeatspl     = false;
  plbeatspf     = false;
  pl02beatspl   = true;  // player 0,2 always in front of player 1,3
  pf23beatspl   = false;
  pl23beatspf   = false;
  pf01beatspl   = false;
  pl01beatspf   = false;

  if (pri & 0x20)
    pl02beatspl = false;  // player 0,2 combined with player 1,3

  if (pri & 0x01) {       // player in front of playfield
    pl01beatspf = true;   // player pair 0,1 in front of playfield
    pl23beatspf = true;   // player pair 2,3 in front of playfield
    plbeatspf   = true;   // player in front of playfield
  }

  if (pri & 0x02) {       // player 0,1 in front of playfield, in front of player 2,3
    pl01beatspf = true;   // player 0,1 in front of playfield
    pf23beatspl = true;   // plafield 2,3 in front of players
  }

  if (pri & 0x04) {       // playfield in front of players
    pf01beatspl = true;   // playfield 0,1 in front of players
    pf23beatspl = true;   // playfield 2,3 in front of players
    pfbeatspl   = true;   // playfield in front of players
  }

  if (pri & 0x08) {       // playfield 0,1 in front of players in front of playfield 2,3
    pf01beatspl = true;   // playfield 0,1 in front of players
    pl23beatspf = true;   // player 2,3 in front of playfield
  }

  //
  // Now fill in the color lookup registers for the PM graphics
  // This is indexed by a bitmask formed by the player/missile "present"
  // bits. Since we have four player color registers, the number of
  // combinations is PlayerColorLookupSize = 16 = 2^4
  for(pm_pixel = 0 ; pm_pixel < PlayerColorLookupSize; pm_pixel++) {
    PreComputedColor pl0,pl2,pl4;
    UBYTE mask;            // pre-computed playfield mask

    pl0 = Black;           // Set player color to black: Priority conflict.
    pl2 = Black;
    pl4 = Black;

    if (pm_pixel & 0x08)   // Player 3 visible?
      pl2 = Player_3;      // if so, set the color of pair 2,3 to 3.
    
    if (pm_pixel & 0x04) { // Player 2 visible?
      if ((pm_pixel & 0x08) && pl02beatspl==false) {
	pl2 = Player_2Or3; // combined color unless the priority is fixed.
      } else {
	pl2 = Player_2;    // player 2 alone as it has higher priority otherwise
      }
    }

    if (pm_pixel & 0x02) { // Player 1 visible?
      pl2 = Black;         // make player pair 2,3 invisible as player 1 has priority
      pl0 = Player_1;      // make player pair 0,1 visible
    }

    if (pm_pixel & 0x01) { // Player 0 visible?
      pl2 = Black;

      if ((pm_pixel & 0x02) && pl02beatspl==false) {
	pl0 = Player_0Or1; // combined color unless priority is fixed
      } else {
	pl0 = Player_0;    // player 0 alone as it has higher priority otherwise
      } 
    }
    
    if (pm_pixel & 0x10) { // Missiles as player 4 visible?
      pl4 = Playfield_3;
      if ((pm_pixel & 0x03) && pfbeatspl==false)
	pl4 = Black;
      if ((pm_pixel & 0x0c) && pl23beatspf)
	pl4 = Black;
      if (pf23beatspl)
	pl2 = Black;
      if (pfbeatspl)
	pl0 = Black;
    }
    
    // These are the colors in front of the background.
    Player0ColorLookup[pm_pixel]     = pl0;
    Player2ColorLookup[pm_pixel]     = pl2; 
    Player4ColorLookup[pm_pixel]     = pl4; 
    //
    // And now for the colors in front of playfields.
    Player0ColorLookupPF01[pm_pixel] = pl0;
    Player0ColorLookupPF23[pm_pixel] = pl0;
    Player2ColorLookupPF01[pm_pixel] = pl2;
    Player2ColorLookupPF23[pm_pixel] = pl2;
    Player4ColorLookupPF01[pm_pixel] = pl4; 
    Player4ColorLookupPF23[pm_pixel] = pl4; 

    // If a player gets "beaten", set its color lookup to Black. We can then
    // just or its value in.
    if (pf01beatspl)
      Player0ColorLookupPF01[pm_pixel] = Black;
    
    if (!plbeatspf)
      Player2ColorLookupPF01[pm_pixel] = Black;

    // Ditto for players in front of playfields 2,3
    if (pf23beatspl)
      Player2ColorLookupPF23[pm_pixel] = Black;
    
    if (pfbeatspl)
      Player0ColorLookupPF23[pm_pixel] = Black;
    
    
    // Now setup the mask for the playfield colors 0,1
    // depending on the player.
    mask = 0xff;
    if (pm_pixel & 0x03) { // For player 0,1
      if (pl01beatspf)
	mask  = 0;
    } 
    if (pm_pixel & 0x0c) { // For player 2,3
      if (plbeatspf)
	mask  = 0;
    }
    if (pm_pixel & 0x10) { // For player 4
      if (pfbeatspl || (pf01beatspl == false && (pm_pixel & 0x03) == 0))
	mask = 0;
    }
    Playfield01Mask[pm_pixel] = mask;

    // Ditto for playfield 2,3
    mask = 0xff;    
    if (pm_pixel & 0x03) { // For players 0,1
      if (!pfbeatspl)
	mask  = 0;
    }   
    if (pm_pixel & 0x0c) { // For players 2,3
      if (pl23beatspf)
	mask  = 0;
    } 
    if (pm_pixel & 0x10) { // For missiles aka player 4
      mask = 0;
    }
    Playfield23Mask[pm_pixel] = mask;
  }
}
///

/// GTIA::PMObject::RemoveRightOf
// Remove all objects right to the indicated half color clock.
// Bitsize is the size of the object in bits in the graphics shift register.
void GTIA::PMObject::RemoveRightOf(UBYTE *target,int bitsize,int retrigger)
{ 
  UBYTE *first,*last,mask;
  //
  if (target) {
    // Compute the last half color clock occupied by the object right now.
    int firsthcc = DecodedPosition;
    int lasthcc  = DecodedPosition + (bitsize << (DecodedSize + 1));
    //
    // Do not remove before the indicated trigger position.
    if (firsthcc < retrigger)
      firsthcc = retrigger;
    //
    // Clip into the limits of the PM scanline.
    if (lasthcc > Player_Right_Border)
      lasthcc = Player_Right_Border;
    //
    if (firsthcc < Player_Left_Border)
      firsthcc = Player_Left_Border;
    //
    // Mask out the object in the rendered target.
    first = target + firsthcc;
    last  = target + lasthcc;
    mask  = UBYTE(~DisplayMask);
    while(first < last) {
      *first++ &= mask;
    }
  }
}
///

/// GITA::PMObject::RetriggerObject
// Reposition an object. Bitsize is the size of
// the object in bits (8 for players, 2 for missiles), val
// the new horizontal position, retrigger the earliest
// possible retrigger position.
void GTIA::PMObject::RetriggerObject(UBYTE *target,int bitsize,UBYTE val,int retrigger)
{ 
  int deltabits;
  //
  // Compute how many bits have been shifted out of the
  // P/M shift register when reaching the new scan position.
  deltabits   = val - HPos;
  //
  // Only do the retriggering if we did not move to the
  // left, and the object is already triggered.
  if (deltabits > 0 && DecodedPosition <= retrigger) {
    UBYTE grafold;
    //
    // Note the divide by 2 or 4 register for player resizing.
    deltabits = (deltabits + (1L << DecodedSize) - 1) >> DecodedSize;
    //
    // Get the contents of the P/M output shift register at the new position.
    if (deltabits >= bitsize) {
      grafold = 0; // No old bits.
    } else {
      grafold = UBYTE(Graphics << deltabits);
    }
    //
    //
    // Done removing the object. Render anew.
    RepositionObject(val); 
    // Now remove the old object to the right of
    // the new position.
    RemoveRightOf(target,bitsize,retrigger); 
    // And render new.
    Render(target,bitsize,grafold | Graphics);
  } else {
    // Object is just moved to the left at the right of the trigger position.
    // Thus, specifically, the object is not even yet triggered. Just remove it
    // at the old position and re-render it.
    RemoveRightOf(target,bitsize,retrigger);
    RepositionObject(val);
    Render(target,bitsize);
  }
}
///

/// GTIA::PMObject::RetriggerSize
// Update the size register while the object is on the screen.
// Arguments are the render target buffer, the size of the object
// in bits occupied in its shift register, the new value of the
// shift scaler as raw value, and the screen position in half
// color clocks where the change should take place.
void GTIA::PMObject::RetriggerSize(UBYTE *target,int bitsize,UBYTE val,int retrigger)
{ 
  int deltabits;
  //
  // Compute how many bits have been shifted out of the
  // P/M shift register when reaching the new scan position.
  // First compute the number of half-color clocks skipped.
  deltabits   = retrigger - DecodedPosition;
  //
  // Only if there is something to do, and there are bits
  // left in the shifter.
  if (deltabits >= 0 && (val & 0x03) != Size) { 
    int deltapos = deltabits; 
    // Distance at which to restart rendering from the current position: 
    // At the trigger position.
    //
    // Compute the number of bits already shifted out, and
    // the bit number that is currently actively on the screen.
    deltabits >>= DecodedSize + 1;
    //
    if (deltabits < bitsize) {
      int phase;
      int missingbits;
      int oldsize = DecodedSize;
      //
      // Remove the old graphics.
      RemoveRightOf(target,bitsize,retrigger);
      //
      // Adjust the size of the object now in the register set.
      ResizeObject(val);
      //
      // Number of bits missing from the output.
      missingbits = deltabits << DecodedSize;
      //
      // Due to phase misalignment, more bits might be missing.
      // Compute the phase relative to the new clock.
      // The clock ticks here always at the end of the player bit,
      // shifting a new bit into the display, indicated by a zero.
      // Size = 0: Phase = 00000000000000000 xx->1
      // Size = 1: Phase = 01010101010101010 4 ->2
      if (oldsize == 0 || DecodedSize == 0) {
	phase = 0; // regular stretching
      } else {
	phase = (deltapos >> 1) & 1; // Generates the second phase pattern.
      } 
      // If we switch from 4x or 2x to 1x with the special value 2, and this
      // happens at phase 1 of the overall shifter, then the shift register gets stuck!
      if ((val & 0x03) == 2) {
	int t = (deltapos >> 1) & 3;
	if ((oldsize == 1 && (t & 1) == 1) || 
	    (oldsize == 2 && (t == 1 || t == 2))) { 
	  DecodedSize = 8;
	}
      }
      //
      // And redraw the object right of the trigger position with
      // the indicated number of bits already removed from the shift register.
      Render(target,bitsize,Graphics,deltapos,missingbits + phase);
    }
  }
}
///

/// GTIA::PMObject::Render
// Render the object into the target, possibly remove the leftmost n
// bits because they are already on the screen.
void GTIA::PMObject::Render(UBYTE *target,int bitsize,UBYTE graphics,int deltapos,int deltabits)
{  
  static const ULONG NibbleDoubleBits[16]    = {0x00,0x03,0x0c,0x0f,0x30,0x33,0x3c,0x3f,
					        0xc0,0xc3,0xcc,0xcf,0xf0,0xf3,0xfc,0xff};
  
  static const ULONG NibbleQuadrupleBits[16] = {0x0000,0x000f,0x00f0,0x00ff,0x0f00,0x0f0f,0x0ff0,0x0fff,
						0xf000,0xf00f,0xf0f0,0xf0ff,0xff00,0xff0f,0xfff0,0xffff};

  if (graphics && target) {
    ULONG graf;
    UBYTE *pmpos;
    int   hpos;
    UBYTE mask;
    //
    // fetch horizontal position, and clear collision masks.
    hpos  = DecodedPosition + deltapos;
    mask  = DisplayMask;
    // all this makes only sense if there is an object to render.
    pmpos = target          + hpos; // get target position in the PM temporary array.
    graf  = graphics;
    //
    // Enlarge the player to its final size.
    switch(DecodedSize) {
    case 0:
      // Single size. Just left-shift the bits to their target position such that all of the
      // player/missile is to the immediate right of the hpos.
      graf = graf << 24;
      break;
    case 1:
      // Double size. Double the size of the object thru the nibble scaler lookup table.
      graf = (NibbleDoubleBits[graf >> 4] << 24)    | NibbleDoubleBits[graf & 0x0f] << 16;
      bitsize <<= 1;
      break;
    case 2:
      // Quadruple the size thru the nibble scaler lookup table as well.
      graf = (NibbleQuadrupleBits[graf >> 4] << 16) | NibbleQuadrupleBits[graf & 0x0f];
      bitsize <<= 2;
      break;
    case 8: 
      // Shift register stuck. Otherwise, it is a x1 mode.
      // Is there data in the shifter that is repeated over the screen?
      if ((graf << deltabits) & 0x80) {
	UBYTE *pmend = target + Player_Right_Border;
	// Yup. Render up to the end of the line.
	pmpos = target + Player_Left_Border; 
	//
	while(pmpos < pmend) {
	  *pmpos++ |= mask;
	}
      }
      return;
    default: // shut up the compiler
      graf = 0;
      break;
    }
    //
    // Additional shift after the size adjustment.
    if (bitsize < deltabits)
      return;
    //
    graf   <<= deltabits;
    bitsize -= deltabits;
    //
    // To avoid unnecessary computations within the loop, check whether we have to mask out any
    // bits because they lie outside of the detectable/renderable region.
    if (hpos < Player_Left_Border) {
      int missingbits = (Player_Left_Border - hpos) >> 1; // The number of bits missing to the left.
      if (missingbits >= 32) {
	// all bits are gone. Nothing to render.
	return;
      }
      // mask out the bits that are gone.
      graf &= 0xffffffff >> missingbits;
    } else if (hpos + 64 > Player_Right_Border) {
      int missingbits = (hpos + 64 - Player_Right_Border) >> 1; // The number of bits missing to the right.
      if (missingbits >= 32) {
	// all bits gone
	return;
      }
      // mask out the bits that are gone.
      graf &= 0xffffffff << missingbits;
    }
    //
    // Now iterate over all bits.
    do {
      if (graf & 0x80000000) {
	// The bit is visible. Display it.
	// Pixel is visible. Hence, render it. 
	// Note that PM graphics is two half color clocks wide
	pmpos[0] |= mask;              // insert the player 
	pmpos[1] |= mask;
      }
      pmpos+=2;
      // Now advance to the next bit
      graf <<= 1;
    } while(--bitsize && graf);
  }
}
///

/// GTIA::TriggerGTIAScanline
// Run a horizontal scanline thru the GTIA, insert
// player/missile graphics and translate abstract
// color indices into Atari colors.
void GTIA::TriggerGTIAScanline(UBYTE *playfield,UBYTE *player,int size,bool fiddling)
{  
  class CPU *cpu              = machine->CPU();          // get CPU for cycle run
  class Antic          *antic = machine->Antic();        // get Antic for DMA
  class AtariDisplay *display = machine->Display();      // get display for generating output data
  UBYTE *out                  = display->NextScanLine(); // get the next scanline for output
  UBYTE *pm,*om;                                         // p/m graphics display, output pointer
  int i;
  int ypos;

#if CHECK_LEVEL > 0
  if (size & 0x03) {
    Throw(InvalidParameter,"GTIA::TriggerGTIAScanline","scanline size must be divisible by four");
  }
#endif
  //
  // Keep the fiddling value for this row
  Fiddling = fiddling;
  //
  PickModeGenerator(Prior);
  //
  // Reset the generator at the beginning of the scanline.
  // FIX: We reset all relevant generators since the generator itself may
  // change within the scanline
  Mode00FA->SignalHBlank();
  Mode80UF->SignalHBlank();
  Mode80F->SignalHBlank();
  //
  // Reset the rendering target for the scanline generator.
  ypos     = antic->CurrentYPos();
  //
  // Now check against fetching the DMA data for players
  if (GractlShadow & 0x02) {
    static const UBYTE PlayerMask[4] = {0x10,0x20,0x40,0x80}; // Player bits in VertDelay
    // Get the player DMA channels from ANTIC with
    // possible VDelay. If the bit is on, fetch only on odd lines.
    for (i = 0;i < 4;i++) {
      if ((ypos & 0x01) || (VertDelay & PlayerMask[i]) == 0)
	Player[i].ReshapeObject(player[i]);
    }
  }
  //
  // Now check against fecthing the DMA data for missiles. Missile 0 occupies
  // bits 0 and 1 (LSBs), but we render missiles from the MSB at the leftmost
  // position, hence missiles have to be upshifted.
  if (GractlShadow & 0x01) {
    static const UBYTE MissileMask[4] = {0x01,0x02,0x04,0x08}; // Missile bits in VertDelay
    UBYTE shift;
    // Extract the missile data from the ANTIC DMA channel. Missiles can be delayed
    // independently, so this is a bit tricky.
    for(i = 0,shift = 6;i < 4;i++,shift -= 2) {
      // Fetch new data on odd lines always, or if no delay is enabled.
      if ((ypos & 0x01) || (VertDelay & MissileMask[i]) == 0) {
	// only the two upper bits are used
	Missile[i].ReshapeObject((player[4] << shift) & 0xc0); 
      }
    }
  }
  // 
  // Clear the PM scanline. This also loads the scanline into the
  // cache, which is good since it is required soon anyhow.
  memset(PMTarget,0,PMScanlineSize);
  //
  // Now render the objects. Due to the new collision logic, the order does not
  // matter any more.
  for(i=0;i<4;i++) {
    Player[i].Render(PMTarget,8);
    Missile[i].Render(PMTarget,2);
  }
  //
  // Color lookup post-processing and scanline generation. 
  // This merges the playfield with the player output and runs the priority engine.
  HPos = 0;
  om   = out,pm = PMTarget,i = size >> 2;
  do {
    //
    // Run the mode generator.
    CurrentGenerator->PostProcessClock(om,playfield,pm);
    //
    // Now advance the CPU clock by one tick.
    cpu->Step();
    // Advance the display by four half color clocks
    pm         += 4;
    playfield  += 4;
    om         += 4;
    HPos       += 4;
  } while(--i);
  //
  // Check whether we need any kind of post-processing here.  
  if (antic->CurrentYPos() < Antic::VBIStart) {
      if (PostProcessor) {
	  PostProcessor->PushLine(out,size);
      } else {
	  // Now signal the display that this line is ready.
	  display->PushLine(out,size);
      }
  }
}
///

/// GTIA::HBI
void GTIA::HBI(void)
{  
  // Now update the Gractl Shadow register
  GractlShadow = Gractl; 
  //  
  // Finally carry over the priority mask if this did not yet happen.
  if (ChipGeneration != CTIA) {
    // Get the modeline generator that is responsible for the color mapping
    // We must pick it each line since the fiddled flag changes each line.
    // This must happen up to cycle 15 or GTIA switches into the strange mode.
    InitialPrior = Prior & 0xc0; // Keep the flag that we are now at the start of the line
  }
}
///

/// GTIA::ConsoleRead
// Read the status of the console switches from the
// GTIA class. This also emulates pressing the OPTION
// key on coldstart
UBYTE GTIA::ConsoleRead(void)
{
  UBYTE val = 0xff;
  //
  // The 5200 doesn't have any console keys but uses this port as an
  // output port to select the active keypad
  if (machine->MachType() != Mach_5200) {
    val = machine->Keyboard()->ConsoleKeys();
  }
  //
  // A one-bit pulls the data down.
  return val & ~ActiveInput;
}
///

/// GTIA::MissilePFCollisionRead
// Read the collision register of the missiles with playfields
UBYTE GTIA::MissilePFCollisionRead(int n)
{
  struct PMObject *missile = Missile + n;
  
  return UBYTE(missile->CollisionPlayfield & missile->PlayfieldColMask);
}
///

/// GTIA::MissilePLCollisionRead
// Read the collision register of missiles with the players.
UBYTE GTIA::MissilePLCollisionRead(int n)
{
  struct PMObject *missile = Missile + n;
  
  // Mask out self-collisions that are otherwise detected
  return UBYTE(missile->CollisionPlayer & missile->PlayerColMask & (~missile->DisplayMask));
}
///

/// GTIA::PlayerPFCollisionRead
// Read the collision registers between player and the
// playfields. This is also straight.
UBYTE GTIA::PlayerPFCollisionRead(int n)
{
  struct PMObject *player = Player + n;
  
  return UBYTE(player->CollisionPlayfield & player->PlayfieldColMask);
}
///

/// GTIA::PlayerPLCollisionRead
// Read the collisions between players. 
UBYTE GTIA::PlayerPLCollisionRead(int n)
{
  struct PMObject *player = Player + n;

  // Mask out self-collisions that are otherwise detected
  return UBYTE(player->CollisionPlayer & player->PlayerColMask & (~player->DisplayMask));
}
///

/// GTIA::PALFlagRead
// Read the PAL/NTSC flag of GTIA
UBYTE GTIA::PALFlagRead(void)
{
  if (NTSC) {
    return 0x0f;
  } else {
    return 0x01;
  }
}
///

/// GTIA::TrigRead
// Read the trigger of the attached joysticks
UBYTE GTIA::TrigRead(int n)
{
  switch(machine->MachType()) {
  case Mach_5200:
    return UBYTE(machine->Paddle(n)->Strig()?(0):(1));   // GTIA has negative logic
  case Mach_Atari800:
    return UBYTE(machine->Joystick(n)->Strig()?(0):(1)); // GTIA has negative logic
  case Mach_AtariXL:
  case Mach_AtariXE:
  case Mach_Atari1200:
    switch(n) {
    case 0:
    case 1:
      return UBYTE(machine->Joystick(n)->Strig()?(0):(1)); // GTIA has negative logic
    case 2:
      // TRIG2 is not attached to anything useful, keep it up
      return 1;
    case 3:
      // TRIG3 is attached to the cartridge logic
      if (machine->MMU()->Trig3CartLoaded()) {
	return 1; // cart is inserted
      } else {
	return 0; // no cart inserted
      }
    }
  default:
    Throw(NotImplemented,"GTIA::TrigRead","Unknown machine type");
    return 0;
  }
}
///

/// GTIA::ColorBKWrite
// Write the background color register
void GTIA::ColorBKWrite(UBYTE val)
{
  ColorLookup[Background_Mask] = ColorLookup[Background] = UBYTE(val & 0xfe);
}
///

/// GTIA::ColorPlayfieldWrite
// Install a playfield color register
void GTIA::ColorPlayfieldWrite(int n,UBYTE val)
{
  // Set the color lookup entry
  ColorLookup[n + Playfield_0] = UBYTE(val & 0xfe);

  // We need to take special care for registers 1 and 2 due to
  // the emulation of color fiddling and artifacting
  switch(n + Playfield_0) {
  case Playfield_1:
  case Playfield_2:
    // The value of the fiddled color comes from register 1, the hue from register 2
    ColorLookup[Playfield_1_Fiddled] = UBYTE((ColorLookup[Playfield_1] & 0x0f) | 
					     (ColorLookup[Playfield_2] & 0xf0));
    break;
  }
}
///

/// GTIA::ColorPlayerWrite
// Install a player color register
void GTIA::ColorPlayerWrite(int n,UBYTE val)
{
  ColorLookup[n + Player_0] = val;
  
  // Setup the or'd registers for player combinations
  ColorLookup[Player_0Or1]      = UBYTE(ColorLookup[Player_0]    | ColorLookup[Player_1]);
  ColorLookup[Player_2Or3]      = UBYTE(ColorLookup[Player_2]    | ColorLookup[Player_3]);
}
///

/// GTIA::GraphicsMissilesWrite
// Write the graphics register for all
// missiles
void GTIA::GraphicsMissilesWrite(UBYTE val)
{
  int retrigger = HPos + PMShape;
  int i,shift;
  struct PMObject *missile;
  //
  // split the graphics amongst all four missiles
  // missile 0 takes the LSB.
  for(i = 0,shift = 6,missile = Missile;i < 4;i++,missile++) {
    if (retrigger < missile->DecodedPosition) {
      missile->RemoveRightOf(PMTarget,2,retrigger);
      missile->ReshapeObject(UBYTE((val << shift) & 0xc0));
      missile->Render(PMTarget,2);
    } else if (retrigger >= missile->DecodedPosition + (4 << missile->DecodedSize)) {
      // Object is already drawn completely.
      missile->ReshapeObject(UBYTE((val << shift) & 0xc0));
    } else {
      // Object is partially on the screen. Unclear what GTIA does in this case. Let's
      // suppose it doesn't reload its shift register just because we're rewriting the
      // shadow copy. Thus, do not modify the screen graphics.
      missile->ReshapeObject(UBYTE((val << shift) & 0xc0));
    }
    shift -= 2;
  }
}
///

/// GTIA::GraphicsPlayerWrite
// Write into the graphics register of player #n
void GTIA::GraphicsPlayerWrite(int n,UBYTE val)
{
  int retrigger = HPos + PMShape;

  if (retrigger < Player[n].DecodedPosition) {
    Player[n].RemoveRightOf(PMTarget,8,retrigger);
    Player[n].ReshapeObject(val);
    Player[n].Render(PMTarget,8);
  } else if (retrigger >= Player[n].DecodedPosition + (16 << Player[n].DecodedSize)) {
    // Object is already drawn completely.
    Player[n].ReshapeObject(val);
  } else {
    // Object is partially on the screen. Unclear what GTIA does in this case. Let's
    // suppose it doesn't reload its shift register just because we're rewriting the
    // shadow copy. Thus, do not modify the screen graphics.
    Player[n].ReshapeObject(val);
  }
}
///

/// GTIA::HitClearWrite
// Clear all collisions found so far
void GTIA::HitClearWrite(void)
{
  int ch;
  struct PMObject *player,*missile;

  for (ch = 0,player = Player,missile = Missile;ch<4;ch++) {
    player->CollisionPlayer     = 0;
    player->CollisionPlayfield  = 0;
    missile->CollisionPlayer    = 0;
    missile->CollisionPlayfield = 0;
    player++,missile++;
  }
}
///

/// GTIA::MissileHPosWrite
// Write the horizontal position of missiles
void GTIA::MissileHPosWrite(int n,UBYTE val)
{ 
  int newpos = int(val - 0x20) << 1; // where the new player would be.
  int reload = HPos + PMReaction;
  //
  // Is there even a chance that the new player gets triggered?
  // The nearest possible trigger position would be at HPos + PMReaction.
  if (newpos >= reload) {
    Missile[n].RetriggerObject(PMTarget,2,val,reload);
  } else {
    Missile[n].RepositionObject(val);
  }
}
///

/// GTIA::PlayerHPosWrite
// Write the horizontal position of players
void GTIA::PlayerHPosWrite(int n,UBYTE val)
{ 
  int newpos = int(val - 0x20) << 1; // where the new player would be.
  int reload = HPos + PMReaction;
  //
  // Is there even a chance that the new player gets triggered?
  // The nearest possible trigger position would be at HPos + PMReaction.
  if (newpos >= reload) {
    Player[n].RetriggerObject(PMTarget,8,val,reload);
  } else {
    Player[n].RepositionObject(val);
  }
}
///

/// GTIA::MissileSizeWrite
// Write the size register of the missiles
void GTIA::MissileSizeWrite(UBYTE val)
{
  int n,retrigger = HPos + PMResize; // Position on the screen where resizing takes place.
  struct PMObject *missile;
  //
  // Bits 0..1 is the size of missile 0 etc.
  for(n = 0,missile = Missile;n < 4;n++,missile++) { 
    if (retrigger < missile->DecodedPosition) {
      // Need to redraw into the buffer, will fit completely.
      missile->RemoveRightOf(PMTarget,2,retrigger);
      missile->ResizeObject(val);
      missile->Render(PMTarget,2);
    } else if (retrigger >= missile->DecodedPosition + (4 << Player[n].DecodedSize)) {
      // Object is already drawn completely, just set the size registers
      // as nothing will happen at this line.
      missile->ResizeObject(val);
    } else {
      // Object is partially on the screen.
      missile->RetriggerSize(PMTarget,2,val,retrigger);
    }
    val >>= 2;
  }
}
///

/// GTIA::PlayerSizeWrite
// Write into the size register of one player
void GTIA::PlayerSizeWrite(int n,UBYTE val)
{
  int retrigger    = HPos + PMResize; // Position on the screen where resizing takes place.
  //
  // If the player is not yet drawn, just write to the size registers
  if (retrigger < Player[n].DecodedPosition) {
    // Need to redraw into the buffer, will fit completely.
    Player[n].RemoveRightOf(PMTarget,8,retrigger);
    Player[n].ResizeObject(val);
    Player[n].Render(PMTarget,8);
  } else if (retrigger >= Player[n].DecodedPosition + (16 << Player[n].DecodedSize)) {
    // Object is already drawn completely, just set the size registers
    // as nothing will happen at this line.
    Player[n].ResizeObject(val);
  } else {
    // Object is partially on the screen.
    Player[n].RetriggerSize(PMTarget,8,val,retrigger);
  }
}
///

/// GTIA::VDelayWrite
// Write into the VDelay register
void GTIA::VDelayWrite(UBYTE val)
{    
  VertDelay = val;
}
///

/// GTIA::ConsoleWrite
// Write into the console register, run the
// console Speaker
void GTIA::ConsoleWrite(UBYTE val)
{
  Speaker = (val & 0x08)?(false):(true);
  machine->Sound()->ConsoleSpeaker(Speaker);
  // The 5200 uses the lower bits of the value to select the active input controller.
  // It also defines the output - a one bit pulls the output down.
  ActiveInput = UBYTE(val & 0x07);
}
///

/// GTIA::PriorWrite
// Write into the priority register
void GTIA::PriorWrite(UBYTE val)
{
  if (val != Prior) {
    UpdatePriorityEngine(val); 
    //
    if (machine->CPU()->CurrentXPos() < 16 && ChipGeneration != CTIA) {
      // Get the modeline generator that is responsible for the color mapping
      // We must pick it each line since the fiddled flag changes each line.
      // This must happen up to cycle 15 or GTIA switches into the strange mode.
      InitialPrior = val & 0xc0; // Keep the flag that we are now at the start of the line
      Prior        = val;
    }
    //
    Prior      = val;
    PickModeGenerator(val);
  }
}
///

/// GTIA::GractlWrite
// Write into the graphics control register
void GTIA::GractlWrite(UBYTE val)
{
  int ch;

  Gractl        = val;
  // If we disable something, disable it in the shadow register as well to
  // avoid that we write into a PM register that is defined by the user
  // shortly after. This fixes Gateway to Apshai
  GractlShadow &= val;

  for (ch=0;ch < 4;ch++) {
    machine->Joystick(ch)->StoreButtonPress((val & 0x04)?(true):(false));
  }
}
///

/// GTIA::ComplexRead
// Read from a GTIA registger, return the
// byte read.
UBYTE GTIA::ComplexRead(ADR mem)
{

  // GTIA is incompletely addressed, only bits 0..4 are routed.
  switch (mem & 0x1f) {
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
    return MissilePFCollisionRead(mem & 0x03); // collisions missiles/playfield
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x07:
    return PlayerPFCollisionRead(mem & 0x03); // collisions player/playfield
  case 0x08:
  case 0x09:
  case 0x0a:
  case 0x0b:
    return MissilePLCollisionRead(mem & 0x03); // collision missiles/players
  case 0x0c:
  case 0x0d:
  case 0x0e:
  case 0x0f:
    return PlayerPLCollisionRead(mem & 0x03); // collision player/player
  case 0x10:
  case 0x11:
  case 0x12:
  case 0x13:
    return TrigRead(mem & 0x03); // joystick trigger read
  case 0x14:
    return PALFlagRead();
  case 0x1f:
    return ConsoleRead();
  default:
    // Interestingly, GTIA seems to pull the topmost nibble low here...
    if (ChipGeneration == CTIA) {
      return 0xff;
    } else {
      return 0x0f;
    }
  }
}
///

/// GTIA::ComplexWrite
// Write into a GTIA register
void GTIA::ComplexWrite(ADR mem,UBYTE val)
{
  switch (mem & 0x1f) {
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
    PlayerHPosWrite(mem & 0x03,val);   // player horizontal position
    return;
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x07:
    MissileHPosWrite(mem & 0x03,val);  // missile horizontal position
    return;
  case 0x08:
  case 0x09:
  case 0x0a:
  case 0x0b:
    PlayerSizeWrite(mem & 0x03,val);   // player size
    return;
  case 0x0c:
    MissileSizeWrite(val);
    return;
  case 0x0d:
  case 0x0e:
  case 0x0f:
  case 0x10:
    GraphicsPlayerWrite((mem - 0x0d) & 0x03,val); // player graphics register
    return;
  case 0x11:
    GraphicsMissilesWrite(val);  // missiles player register
    return;
  case 0x12:
  case 0x13:
  case 0x14:
  case 0x15:
    ColorPlayerWrite((mem - 0x12) & 0x03,val);
    return;
  case 0x16:
  case 0x17:
  case 0x18:
  case 0x19:
    ColorPlayfieldWrite((mem - 0x16) & 0x03,val);
    return;
  case 0x1a:
    ColorBKWrite(val);
    return;
  case 0x1b:
    PriorWrite(val);
    return;
  case 0x1c:
    VDelayWrite(val);
    return;
  case 0x1d:
    GractlWrite(val);
    return;
  case 0x1e:
    HitClearWrite();
    return;
  case 0x1f:
    ConsoleWrite(val);
    return;
  }
}
///

/// GTIA::SetupArtifacting
// Setup the artifacting colors of GTIA.
void GTIA::SetupArtifacting(void)
{  
  int in;
  //
  switch(ChipGeneration) {
  case CTIA:
    ColorLookup[Playfield_Artifact1] = 0x80; // blue
    ColorLookup[Playfield_Artifact2] = 0xc0; // green
    break;
  case GTIA_1:
    ColorLookup[Playfield_Artifact1] = 0xa0; // green
    ColorLookup[Playfield_Artifact2] = 0x40; // blue
    break;
  case GTIA_2:
    ColorLookup[Playfield_Artifact1] = 0x90; // red
    ColorLookup[Playfield_Artifact2] = 0x20; // blue
    break;
  }
  //
  // Now built-up the hue mixing engine for the
  // color artifacting.
  //
  // Simple for black&white. Just the above colors.
  HueMix[0] = ColorLookup[Playfield_Artifact1];
  HueMix[1] = ColorLookup[Playfield_Artifact2];
  //
  // Now the mixing colors. This is the average of the
  // two colors.
  for(in = 1; in < 16; in++) {
    int idx;
    //
    for(idx = 0;idx < 2;idx ++) {
      int color1,color2,delta,color;
      int center = (ColorLookup[Playfield_Artifact1 + idx] - 0x50  ) & 0xf0;
      color1     = (ColorLookup[Playfield_Artifact1 + idx] - center) & 0xf0;
      color2     = ((in << 4)                              - center) & 0xf0;
      //
      delta      = (color2 - color1) >> 1;
      color      = delta + color1 + center;
      if (color >= 0x100)
	color -= 0xf0;
      HueMix[idx + (in << 1)] = color & 0xf0;
    }
  }
}
///

/// GITA::LoadColorMapFrom
// Load an external color map from the indicated file and install it.
void GTIA::LoadColorMapFrom(const char *src)
{
  FILE *file;
  UBYTE data[256*3]; // will contain the new color map once loaded.

  file = fopen(src,"rb");
  if (file) {
    try {
      if (fread(data,sizeof(UBYTE),256*3,file) != 256*3) {
	if (feof(file)) {
	  Throw(InvalidParameter,"GTIA::LoadColorMapFrom","invalid file format, file is not a palette file");
	} else {
	  ThrowIo("GTIA::LoadColorMapFrom","error reading palette file");
	}
      } else {
	struct ColorEntry *cols = NULL;
	char *newname            = NULL;
	const UBYTE *entry      = data;
	int i;
	//
	if (fgetc(file) != -1)
	  Throw(InvalidParameter,"GTIA::LoadColorMapFrom","invalid file format, file is not a palette file");
	//
	// Otherwise, create a new palette if we can get one.
	try {
	  cols = new struct ColorEntry[256];
	  newname = new char[strlen(src) + 1];
	} catch(...) {
	  delete[] cols;
	  delete[] newname;
	  throw;
	}
	delete[] LoadedColorMap;
	strcpy(newname,src);
	LoadedColorMap = newname;
	delete[] ExternalColorMap;
	ExternalColorMap = cols;
	//
	for(i = 0;i < 256;i++) {
	  UBYTE r,g,b;
	  //
	  r = *entry++;
	  g = *entry++;
	  b = *entry++;
	  //
	  cols[i].alpha = 0x00;
	  cols[i].red   = r;
	  cols[i].green = g;
	  cols[i].blue  = b;
	  cols[i].packed= (ULONG(r) << 16) | (ULONG(g) << 8) | ULONG(b);
	}
      }
      fclose(file);
    } catch(...) {
      fclose(file);
      throw;
    }
  } else {
    ThrowIo("GTIA::LoadColorMapFrom","error opening palette file");
  }
}
///

/// GTIA::ParseArgs
// Parse arguments for the GTIA class
void GTIA::ParseArgs(class ArgParser *args)
{
  LONG val,gen;
  char cmaskname[64];
  int i;
  bool ntsc;
  static const struct ArgParser::SelectionVector videovector[] = 
    { {"Auto"         ,2    },
      {"PAL"          ,0    },
      {"NTSC"         ,1    },
      {NULL           ,0}
    };
  static const struct ArgParser::SelectionVector playervector[] =
    { {"All"          ,PlayerC|MissileC},
      {"Players"      ,PlayerC},
      {"Missiles"     ,MissileC},
      {"None"         ,0},
      {NULL           ,0}
    };
  static const struct ArgParser::SelectionVector playfieldvector[] =
    { {"All"          ,PlayerC|MissileC},
      {"Players"      ,PlayerC},
      {"Missiles"     ,MissileC},
      {"None"         ,0},
      {NULL           ,0}
    };
  static const struct ArgParser::SelectionVector generationvector[] = 
    { {"CTIA"            ,CTIA},
      {"GTIA"            ,GTIA_1},
      {"XLGTIA"          ,GTIA_2},
      {NULL              ,0}
    };

  val = NTSC?1:0;
  if (isAuto)
    val = 2;
  ntsc = NTSC;
  
  gen = ChipGeneration;
  args->DefineTitle("GTIA");
  args->DefineSelection("GTIAVideoMode","set GTIA video mode",videovector,val);
  args->DefineSelection("ChipGeneration","set GTIA chip revision",generationvector,gen);
  args->DefineBool("Artifacts","enable COLPF1 artifacts",ColPF1FiddledArtifacts);
  args->DefineBool("PALColorBlur","enable color blur between adjacent lines",PALColorBlur);
  args->DefineBool("AntiFlicker","enable color blur between adjacent frames",AntiFlicker);
  args->DefineLong("PlayerPositionDelay","half color clocks required to retrigger a player",0,32,PMReaction);
  args->DefineLong("PlayerResizeDelay","half color clocks required to resize a player",0,32,PMResize);
  args->DefineLong("PlayerReshapeDelay","half color clocks required to change the graphics of a player",0,32,PMShape);
  args->DefineFile("ColorMapName","name of an external color map to be used",ColorMapToLoad,false,true,false);
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
  if (NTSC != ntsc) {
    // Changed video mode => requires a rebuild
    // of the palette
    args->SignalBigChange(ArgParser::Reparse);
  }
  switch(gen) {
  case CTIA:
    ChipGeneration = CTIA;
    break;
  case GTIA_1:
    ChipGeneration = GTIA_1;
    break;
  case GTIA_2:
    ChipGeneration = GTIA_2;
    break;
  }
  //
  // Load a replacement color map?
  if (ColorMapToLoad &&  
      (LoadedColorMap == NULL || strcmp(LoadedColorMap,ColorMapToLoad))) {
    if (*ColorMapToLoad) {
      LoadColorMapFrom(ColorMapToLoad);
    } else {
      delete[] ExternalColorMap;
      ExternalColorMap = NULL;
    }
  }
  //
  // Setup or modify the color map now
  if (ExternalColorMap) {
    ColorMap = ExternalColorMap;
  } else {
    ColorMap = (NTSC)?(NTSCColorMap):(PALColorMap);
  }
  //
  // Setup artifacting color bases.
  SetupArtifacting();
  //
  // Now get the collision masks for players, playfields and missiles
  for(i=0;i<4;i++) {
    sprintf(cmaskname,"PlayerTrigger.%d",i);
    args->DefineSelection(cmaskname,"set collisions the player may cause",playervector,PlayerCollisions[i]);
    sprintf(cmaskname,"PlayfieldTrigger.%d",i);
    args->DefineSelection(cmaskname,"set collisions the playfield may cause",playfieldvector,PlayfieldCollisions[i]);
  }
  //
  // Now compute the masks based on the above collision set.
  {
    UBYTE pfplmask = 0x00;
    UBYTE pfmlmask = 0x00;
    UBYTE plplmask = 0x00;
    UBYTE plmlmask = 0x00;
    //
    for(i=0;i<4;i++) {
      if (PlayerCollisions[i]    & PlayerC )
	plplmask |= UBYTE(1<<i);
      if (PlayerCollisions[i]    & MissileC)
	plmlmask |= UBYTE(1<<i);
      if (PlayfieldCollisions[i] & PlayerC )
	pfplmask |= UBYTE(1<<i);
      if (PlayfieldCollisions[i] & MissileC)
	pfmlmask |= UBYTE(1<<i);
    }
    //
    //
    for(i=0;i<4;i++) {
      Player[i].PlayerColMask     = plplmask;
      Player[i].PlayfieldColMask  = pfplmask;
      Missile[i].PlayerColMask    = plmlmask;
      Missile[i].PlayfieldColMask = pfmlmask;
    }
  }
  //
  // Setup the post-processor.
  delete PostProcessor;
  PostProcessor = NULL;
  if (PALColorBlur) {
    if (AntiFlicker) {
      PostProcessor = new PALFlickerFixer(machine,ColorMap);
    } else {
      PostProcessor = new PALColorBlurer(machine,ColorMap);
    }
  } else {
    if (AntiFlicker) {
      PostProcessor = new FlickerFixer(machine,ColorMap);
    } else {
      PostProcessor = NULL;
    }
  }
  //
  // Reset the post-processor now in case we have it.
  if (PostProcessor)
    PostProcessor->Reset();
  //
  // Re-pick the mode generator since the GTIA/CTIA flag might have
  // been toggled.
  PickModeGenerator(Prior);
}
///

/// GTIA::DisplayStatus
void GTIA::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("GTIA status: (Generation %s)\n"
		   "\tPlayer0Pos : %02x\tPlayer1Pos : %02x\tPlayer2Pos : %02x\tPlayer3Pos : %02x\n"
		   "\tMissile0Pos: %02x\tMissile1Pos: %02x\tMissile2Pos: %02x\tMissile3Pos: %02x\n"
		   "\tPlayer0Size: %02x\tPlayer2Size: %02x\tPlayer2Size: %02x\tPlayer3Size: %02x\n"
		   "\tMissileSize: %02x\n"
		   "\tGraphPlyr0 : %02x\tGraphPlyr1 : %02x\tGraphPlyr2 : %02x\tGraphPlyr3 : %02x\n"
		   "\tGraphMssle : %02x\n"
		   "\tColorPlM0  : %02x\tColorPlM1  : %02x\tColorPlM2  : %02x\tColorPlM3  : %02x\n"
		   "\tPALSwitch  : %02x\tHPos       : %x\n"
		   "\tColorPF0   : %02x\tColorPF1   : %02x\tColorPF2   : %02x\tColorPF3   : %02x\n"
		   "\tColorBack  : %02x\tPriority   : %02x\tVDelay     : %02x\tGractl     : %02x\n"
		   "\tConsole    : %02x\tSpeaker    : %s\n"
		   "\tPlayer0PF  : %02x\tPlayer1PF  : %02x\tPlayer2PF  : %02x\tPlayer3PF  : %02x\n"
		   "\tPlayer0PFM : %02x\tPlayer1PFM : %02x\tPlayer2PFM : %02x\tPlayer3PFM : %02x\n"
		   "\tMissile0PF : %02x\tMissile1PF : %02x\tMissile2PF : %02x\tMissile3PF : %02x\n"
		   "\tMissile0PFM: %02x\tMissile1PFM: %02x\tMissile2PFM: %02x\tMissile3PFM: %02x\n"
		   "\tPlayer0Pl  : %02x\tPlayer1Pl  : %02x\tPlayer2Pl  : %02x\tPlayer3Pl  : %02x\n"
		   "\tPlayer0PlM : %02x\tPlayer1PlM : %02x\tPlayer2PlM : %02x\tPlayer3PlM : %02x\n"
		   "\tMissile0Pl : %02x\tMissile1Pl : %02x\tMissile2Pl : %02x\tMissile3Pl : %02x\n"
		   "\tMissile0PlM: %02x\tMissile1PlM: %02x\tMissile2PlM: %02x\tMissile3PlM: %02x\n"
		   "\tTrigger0   : %02x\tTrigger1   : %02x\tTrigger2   : %02x\tTrigger3   : %02x\n"
		   "\tArtifacts  :%3s\tVideoMode :%4s\tColorBlur  :%3s\tAntiFlicker:%3s\n",
		   (ChipGeneration == CTIA)?("CTIA"):((ChipGeneration == GTIA_1)?("GTIA_1"):("GTIA_2")),
		   Player[0].HPos, Player[1].HPos, Player[2].HPos, Player[3].HPos,
		   Missile[0].HPos,Missile[1].HPos,Missile[2].HPos,Missile[3].HPos,
		   Player[0].Size, Player[1].Size, Player[2].Size, Player[3].Size,
		   (Missile[0].Size>>6) | (Missile[1].Size>>4) | (Missile[2].Size>>2) | (Missile[3].Size),
		   Player[0].Graphics,Player[1].Graphics,Player[2].Graphics,Player[3].Graphics,
		   (Missile[0].Graphics >> 6) | (Missile[1].Graphics >> 4) | 
		   (Missile[2].Graphics >> 2) | (Missile[3].Graphics >> 0),
		   ColorLookup[Player_0],ColorLookup[Player_1],ColorLookup[Player_2],ColorLookup[Player_3],
		   PALFlagRead(),HPos,
		   ColorLookup[Playfield_0],ColorLookup[Playfield_1],ColorLookup[Playfield_2],ColorLookup[Playfield_3],
		   ColorLookup[Background],Prior,VertDelay,Gractl,
		   ConsoleRead(),(Speaker)?("on"):("off"),
		   PlayerPFCollisionRead(0),PlayerPFCollisionRead(1),PlayerPFCollisionRead(2),PlayerPFCollisionRead(3),
		   Player[0].PlayfieldColMask,Player[1].PlayfieldColMask,Player[2].PlayfieldColMask,Player[3].PlayfieldColMask,
		   MissilePFCollisionRead(0),MissilePFCollisionRead(1),MissilePFCollisionRead(2),MissilePFCollisionRead(3), 
		   Missile[0].PlayfieldColMask,Missile[1].PlayfieldColMask,Missile[2].PlayfieldColMask,Missile[3].PlayfieldColMask,
		   PlayerPLCollisionRead(0),PlayerPLCollisionRead(1),PlayerPLCollisionRead(2),PlayerPLCollisionRead(3),
		   Player[0].PlayerColMask,Player[1].PlayerColMask,Player[2].PlayerColMask,Player[3].PlayerColMask,
		   MissilePLCollisionRead(0),MissilePLCollisionRead(1),MissilePLCollisionRead(2),MissilePLCollisionRead(3),
		   Missile[0].PlayerColMask,Missile[1].PlayerColMask,Missile[2].PlayerColMask,Missile[3].PlayerColMask,
		   TrigRead(0),TrigRead(1),TrigRead(2),TrigRead(3),
		   (ColPF1FiddledArtifacts)?("on"):("off"),
		   (NTSC)?("NTSC"):("PAL"),
		   (PALColorBlur)?("on"):("off"),
		   (AntiFlicker)?("on"):("off")
		   );
}
///

/// GTIA::State
// Read or set the internal status
void GTIA::State(class SnapShot *sn)
{
  int i;
  char helptxt[80],id[32];
  LONG missile;

  sn->DefineTitle("GTIA");
  for(i=0;i<4;i++) {
    snprintf(id,31,"Player%dColor",i);
    snprintf(helptxt,79,"player %d color",i);
    sn->DefineLong(id,helptxt,0x00,0xff,ColorLookup[Player_0 + i]);
    ColorPlayerWrite(i,ColorLookup[Player_0 + i]);
    //
    snprintf(id,31,"Playfield%dColor",i);
    snprintf(helptxt,79,"playfield %d color",i);
    sn->DefineLong(id,helptxt,0x00,0xff,ColorLookup[Playfield_0 + i]);
    ColorPlayfieldWrite(i,ColorLookup[Playfield_0 + i]);
    //
    snprintf(id,31,"Player%dGraphics",i);
    snprintf(helptxt,79,"player %d graphics register",i);
    sn->DefineLong(id,helptxt,0x00,0xff,Player[i].Graphics);
    //
    snprintf(id,31,"Player%dSize",i);
    snprintf(helptxt,79,"player %d size",i);
    sn->DefineLong(id,helptxt,0x00,0xff,Player[i].Size);
    PlayerSizeWrite(i,Player[i].Size);
    //
    snprintf(id,31,"Player%dHPos",i);
    snprintf(helptxt,79,"player %d horizontal position",i);
    sn->DefineLong(id,helptxt,0x00,0xff,Player[i].HPos);
    PlayerHPosWrite(i,Player[i].HPos);
    //
    snprintf(id,31,"Missile%dHPos",i);
    snprintf(helptxt,79,"missile %d horizontal position",i);
    sn->DefineLong(id,helptxt,0x00,0xff,Missile[i].HPos);
    MissileHPosWrite(i,Missile[i].HPos);
  }
  sn->DefineLong("PlayfieldBackgroundColor","playfield background color",0x00,0xff,ColorLookup[Background]);
  ColorBKWrite(ColorLookup[Background]);

  missile = (Missile[0].Graphics >> 6) | (Missile[1].Graphics >> 4) | (Missile[2].Graphics >> 2) | (Missile[3].Graphics);
  sn->DefineLong("MissileGraphics","missiles graphic register",0x00,0xff,missile);
  GraphicsMissilesWrite(UBYTE(missile));

  missile = (Missile[0].Size) | (Missile[1].Size << 2) | (Missile[2].Size << 4) | (Missile[3].Size << 6);
  sn->DefineLong("MissilesSizes","missile combined size register",0x00,0xff,missile);
  MissileSizeWrite(UBYTE(missile));

  sn->DefineLong("Prior","graphics priority register",0x00,0xff,Prior);
  UpdatePriorityEngine(Prior);
  
  sn->DefineLong("GraCtl","graphics control register",0x00,0x07,Gractl);
  GractlWrite(GractlShadow = Gractl);
  sn->DefineLong("VDelay","player/missile vertical delay register",0x00,0xff,VertDelay);
  //
  // We don't store the collision registers. This is a bit incorrect, but so what.
  HitClearWrite();
  sn->DefineBool("Speaker","console speaker position",Speaker);
  machine->Sound()->ConsoleSpeaker(Speaker);
}
///

