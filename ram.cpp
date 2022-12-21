/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: ram.cpp,v 1.7 2022/12/20 16:35:48 thor Exp $
 **
 ** In this module: Definition of the RAM as a complete object with a single
 ** state.
 **********************************************************************************/

/// Includes
#include "ram.hpp"
#include "monitor.hpp"
#include "stdio.hpp"
#include "snapshot.hpp"
#include "new.hpp"
///

/// RAM::RAM
// Construction of the RAM. This mainly constructs all RAM pages and then
// runs off.
RAM::RAM(class Machine *mach)
  : Chip(mach,"RAM"), Saveable(mach,"RAM"),
    Pages(new class RamPage[256]), UsedFlags(new UBYTE[256 * 256])
{
}
///

/// RAM::~RAM
// Cleanup the RAM pages here.
RAM::~RAM(void)
{
  delete[] Pages;
  delete[] UsedFlags;
}
///

/// RAM::ColdStart
// Coldstart the RAM. Uhm. Clean it up.
void RAM::ColdStart(void)
{
  int i;
  // Clear memory pages to really emulate a coldstart
  for(i=0;i<256;i++) {
    Pages[i].Blank();
    Pages[i].setUsedFlags(UsedFlags + (i << 8));
  }
  memset(UsedFlags,0,256 * 256 * sizeof(UBYTE));
}
///

/// RAM::WarmStart
// Warmstart the RAM. This does nothing.
void RAM::WarmStart(void)
{
}
///

/// RAM::ParseArgs
void RAM::ParseArgs(class ArgParser *)
{
}
///

/// RAM::DisplayStatus
// Print the status of the RAM. Is there anything to display?
void RAM::DisplayStatus(class Monitor *mon)
{
  int i,o,lines = 0;

  for(i=0;i<256;i++) {
    int used = 0;
    mon->PrintStatus("RAM status page 0x%02x:",i);
    for(o = 0;o < 256;o++) {
      used += UsedFlags[(i << 8) | o];
    }
    if (used == 0) {
      mon->PrintStatus(" <untouched>\n");
      lines++;
    } else if (used == 256) {
      mon->PrintStatus(" <all used>\n");
      lines++;
    } else {
      for(o = 0;o < 256;o++) {
	if ((o & 0x1f) == 0)
	  mon->PrintStatus("\n");
	if (UsedFlags[(i << 8) | o])
	  mon->PrintStatus("*");
	else
	  mon->PrintStatus(".");
      }
      mon->PrintStatus("\n");
      lines += 8 + 1;
    }
    if (lines > 32) {
      mon->WaitKey();
      lines = 0;
    }
  }
  memset(UsedFlags,0,256 * 256 * sizeof(UBYTE));
}
///

/// RAM::State
// Define the state machine of the RAM.
void RAM::State(class SnapShot *sn)
{
  int i;
  char id[32],helptxt[80];
  
  sn->DefineTitle("RAM");
  for(i = 0;i<256;i++) {
    snprintf(id,31,"Page%d",i);
    snprintf(helptxt,79,"RAM page %d contents",i);
    sn->DefineChunk(id,helptxt,Pages[i].Memory(),256);
  }
}
///
