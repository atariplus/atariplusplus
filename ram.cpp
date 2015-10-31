/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: ram.cpp,v 1.6 2015/05/21 18:52:42 thor Exp $
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
    Pages(new class RamPage[256])
{
}
///

/// RAM::~RAM
// Cleanup the RAM pages here.
RAM::~RAM(void)
{
  delete[] Pages;
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
  }
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
  mon->PrintStatus("RAM status:\n"
		   "\tThe RAM is fine.\n");
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
