/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: pokey.cpp,v 1.143 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Pokey emulation 
 **
 ** CREDIT NOTES:
 ** This pokey emulation uses a sound emulation process that works aLONG the same
 ** idea as Ron Fries' pokey emulator, though it is not directly based on, but 
 ** influenced by the original code. 
 ** Specifically, it differs significantly in its implementation of the high-pass 
 ** filters, polycounter implementation, channel muting to cut down complexity
 ** and VOLONLY emulation. It additionally implements a sound anti-alias filtering 
 ** and an interface towards various data types the sound front-end may provide.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "sio.hpp"
#include "sound.hpp"
#include "keyboard.hpp"
#include "monitor.hpp"
#include "gamecontroller.hpp"
#include "pokey.hpp"
#include "tape.hpp"
#include "cpu.hpp"
#include "snapshot.hpp"
#include "time.hpp"
#include "audiobuffer.hpp"
#include "stdlib.hpp"
#include "stdio.hpp"
#include "new.hpp"
#include <math.h>
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int Pokey::Base64kHz = 28;     // Divisor from 1.79Mhz to 64 KHz
const int Pokey::Base15kHz = 114;    // Divisor from 1.79Mhz to 15 Khz
#endif
///

/// Pokey::PolyCounterNone
const UBYTE Pokey::PolyCounterNone[1] = 
  { 15 };
///

/// Pokey::PolyCounter4
const UBYTE Pokey::PolyCounter4[Poly4Size] = 
  { 15,15,0,15,15,15,0,0,0,0,15,0,15,0,0 };
///

/// Pokey::PolyCounter5
const UBYTE Pokey::PolyCounter5[Poly5Size] = 
  { 0,0,15,15,0,0,0,15,15,15,15,0,0,15,0,15,0,15,15,0,15,15,15,0,15,0,0,0,0,0,15};
///

/// Pokey::Pokey
// Pokey constructor
Pokey::Pokey(class Machine *mach,int unit)
  : Chip(mach,(unit)?"ExtraPokey":"Pokey"), Saveable(mach,(unit)?"ExtraPokey":"Pokey"), 
    VBIAction(mach), HBIAction(mach), CycleAction(mach), IRQSource(mach),
    Outcnt(0), 
    PolyCounter9(new UBYTE[Poly9Size]), PolyCounter17(new UBYTE[Poly17Size]),
    Random9(new UBYTE[Poly9Size]), Random17(new UBYTE[Poly17Size]),
    SampleCnt(0), OutputMapping(new BYTE[256]), Unit(unit), 
    SAPOutput(NULL), SongName(NULL), AuthorName(NULL), EnableSAP(false)
{
  int n;
  //
  // Install default gamma, volume here.
  // Sublinear mapping, full volume
  Gamma  = 70;
  Volume = 100;  
  // Initialize the DC level shift.
  DCLevelShift     = 128;
  DCAverage        = 0;
  DCFilterConstant = 512;
 
  InitPolyCounter(Random9 ,PolyCounter9 ,9 ,4 );
  InitPolyCounter(Random17,PolyCounter17,17,12);
  //
  UpdateAudioMapping();
  //
  //
  Frequency17Mhz     = 1789790; // 1.79Mhz clock
  // Initialize the poly counter pointers now to the corresponding main class
  // pointers.
  PolyPointerFirst[0]  = &Poly5Ptr;
  PolyPointerSecond[0] = &Poly17Ptr;
  PolyPointerFirst[1]  = &Poly5Ptr;
  PolyPointerSecond[1] = NULL;
  PolyPointerFirst[2]  = &Poly5Ptr;
  PolyPointerSecond[2] = &Poly4Ptr;
  PolyPointerFirst[3]  = &Poly5Ptr;
  PolyPointerSecond[3] = NULL;
  PolyPointerFirst[4]  = &PolyNonePtr;
  PolyPointerSecond[4] = &Poly17Ptr;
  PolyPointerFirst[5]  = &PolyNonePtr;
  PolyPointerSecond[5] = NULL;
  PolyPointerFirst[6]  = &PolyNonePtr;
  PolyPointerSecond[6] = &Poly4Ptr;
  PolyPointerFirst[7]  = &PolyNonePtr;
  PolyPointerSecond[7] = NULL;
  //
  // Initialize poly counter registers
  PolyNonePtr          = PolyCounterNone;
  Poly4Ptr             = PolyCounter4;
  Poly5Ptr             = PolyCounter5;  
  Poly9Ptr             = PolyCounter9;
  Poly17Ptr            = PolyCounter17;
  Random9Ptr           = Random9;
  Random17Ptr          = Random17;
  PolyNoneEnd          = PolyCounterNone + 1;
  Poly4End             = Poly4Ptr  + Poly4Size;
  Poly5End             = Poly5Ptr  + Poly5Size;
  Poly9End             = Poly9Ptr  + Poly9Size;
  Poly17End            = Poly17Ptr + Poly17Size;
  Random9End           = Random9   + Poly9Size;
  Random17End          = Random17  + Poly17Size;
  //
  // Reset the channels here. We cannot do this in the
  // reset routine since the async sound generation may
  // require samples before the coldstart is even
  // entered...
  for(n = 0;n < 4;n++)
    Ch[n].Reset();
  //
  // Reset pot counters here. The maximum value for unconnected POTs is 288,
  // and we initialize all of them to this value.
  memset(PotNCnt,228,sizeof(PotNCnt));
  memset(PotNMax,228,sizeof(PotNMax));
  // Pot values are all valid.
  AllPot           = 0x00;
  // PAL operation
  NTSC             = false;
  isAuto           = true;
  SIOSound         = true;
  CycleTimers      = false;
  Outcnt           = 0;
  //
  // Initialize serial timing, will be overridden
  // by SkCtrl writes
  SerIn_Delay      = 9 * Base15kHz;
  SerOut_Delay     = 9 * Base15kHz;
  SerIn_Clock      = 9 * Base15kHz;
  SerOut_Clock     = 9 * Base15kHz;
  SerXmtDone_Delay = 9 * Base15kHz;
  SerBitOut_Delay  = Base15kHz;
  SerInRate        = 0;
  SerInManual      = false;
  //
  // Initialize pointers to be on the safe side.
  sound            = NULL;
  keyboard         = NULL;
  sio              = NULL;
}
///

/// Pokey::InitPolyCounter
// Initialize the poly counter for audio usage and the bit-output of
// the random generator, composed of the uppermost bits of the
// generator. Arguments are the random output, the audio output,
// the size of the polycounter in bits (and also the last tap) and
// the first tap.
void Pokey::InitPolyCounter(UBYTE *rand,UBYTE *audio,int size,int tap)
{
  int shift[17]; // the shift register. Maximum possible size is 17.
  int length = (1L << size) - 1; // Size of the output in entries. This should better be correct.
  int i,j,n;

  //
  // Pokey reset sets them all to zero.
  for(i = 0;i < size;i++)
    shift[i] = 1;
  
  for(i = 0;i < length;i++) {
    // Compute the random output by picking the first size bits from the shift register.
    for(j = 0,n = 0;j < 8;j++)
      n |= shift[j] << (7-j);
    *rand++  = n;
    *audio++ = shift[0]?15:0; // normalized to maximum audio volume: 15 or 0.

    // Compute the new input to the register from the two taps. 
    // The first tab is always the register size.
    n = shift[tap-1]^shift[size-1];
    for(j = size - 1;j > 0;j--)
      shift[j] = shift[j-1];
    shift[0] = n;
  }
}
///

/// Pokey::WarmStart
void Pokey::WarmStart(void)
{
  int n;
  // Poly counter init etc. is not done by Pokey on
  // warmstart, but only on coldstart!
  
  // Initialize various audio registers
  IRQStat            = 0xff; // reset IRQ pending bits
  IRQEnable          = 0;    // no IRQ enabled
  SerIn_Counter      = 0;    // no serial IRQ pending
  SerOut_Counter     = 0;
  SerXmtDone_Counter = 0;
  SerBitOut_Counter  = 0;
  SerOut_BitCounter  = 0;
  SerOut_Buffer      = 0xff;
  SerOut_Register    = 0xffff;
  SkStat             = 0xf0; // no pending errors
  SkCtrl             = 0x00;
  SerInBytes         = 0x00; // reset serial port
  SerInRate          = 0;
  SerInManual        = false;
  //
  //
  // Initialize poly counter registers
  PolyNonePtr          = PolyCounterNone;
  Poly4Ptr             = PolyCounter4;
  Poly5Ptr             = PolyCounter5;
  Poly9Ptr             = PolyCounter9;
  Poly17Ptr            = PolyCounter17;
  Random9Ptr           = Random9;
  Random17Ptr          = Random17;
  PolyAdjust           = 0;    
  // Now reset the states of all channels
   for(n = 0;n < 4;n++) {
    Ch[n].Reset();
  }
  Ch[0].OutBit = Ch[1].OutBit = 0x00;
  Ch[2].OutBit = Ch[3].OutBit = 0x0f;  // strange init, but HW manual says so
  //
  // Initialize various audio registers
  AudioCtrl          = 0;    // reset audio control
  TimeBase           = Base64kHz; // Base timer is 64Khz
  UpdateSound(0x0f); // Update all four channels
  // Initialize the audio counters now
  SampleCnt          = 0;
  Output             = 0;
  Outcnt             = 0;
  //
  // 
  // Reset pot counters here. The maximum value for unconnected POTs is 288,
  // and we initialize all of them to this value.
  memset(PotNCnt,228,sizeof(PotNCnt));
  memset(PotNMax,228,sizeof(PotNMax));
  AllPot             = 0x00;
  ConcurrentInput    = 0xff;
  ConcurrentBusy     = false;
}
///

/// Pokey::ColdStart
void Pokey::ColdStart(void)
{
  // Get system linkage if there is something
  // to link to. This is not required for units
  // higher than two.
  if (Unit == 0) {
    keyboard    = machine->Keyboard();
    sio         = machine->SIO();
  }
  sound         = machine->Sound();
  
  WarmStart();
}
///

/// Pokey::~Pokey
// Destructor of the pokey class
Pokey::~Pokey(void)
{
  
  // Shut down the SAP output in case it is
  // still running.
  if (SAPOutput) {
    fclose(SAPOutput);
  }

  delete[] SongName;
  delete[] AuthorName;

  // If the cycle-precise timers were active, remove it here.
  if (CycleTimers) {
    Node<CycleAction>::Remove();
    CycleTimers = false;
  }
  //
  // Release the poly counter buffers: Only the 9,17 bit polycounter is hand-
  // allocated.
  delete[] PolyCounter9;
  delete[] PolyCounter17;
  delete[] Random9;
  delete[] Random17;
  delete[] OutputMapping;
}
///

/// Pokey::UpdateSound
// Update the internal state machine after modifications
// to some audio registers have been made
void Pokey::UpdateSound(UBYTE mask)
{
  struct Channel *ch;
  static const UBYTE Mhz17Flag[4]  = { 0x40, 0x00, 0x20, 0x00 };
  static const UBYTE LinkHiFlag[4] = { 0x00, 0x10, 0x00, 0x08 };
  static const UBYTE LinkLoFlag[4] = { 0x10, 0x00, 0x08, 0x00 };
  static const UBYTE FilterMask[4] = { 0x00, 0x00, 0x04, 0x02 };
  UBYTE channelmask;
  int n;

  // Re-set the timer base for the frequency counters
  if (AudioCtrl & 0x01) { // 15Khz clock instead of 64Khz?
    TimeBase = Base15kHz;
  } else {
    TimeBase = Base64kHz;
  }
  //
  // Check for 17 vs. 9 bit polycounter and adjust masks accordingly
  if (AudioCtrl & 0x80) { // 9 bit polycounter instead of 17 bit?
    PolyPointerSecond[0]  = &Poly9Ptr;
    PolyPointerSecond[4]  = &Poly9Ptr;
  } else {   
    PolyPointerSecond[0]  = &Poly17Ptr;
    PolyPointerSecond[4]  = &Poly17Ptr;
  }
  // If channel 2 gets updated and SkCtrl requests an update of
  // the delay, reload channel 3 as well since we overwrote its
  // timer settings to emulate the serial sound properly. 
  if (SIOSound && (SkCtrl & 0x30) && (mask & 0x04)) {
    mask |= 0x08;
  }
  
  for(n=0,ch = Ch,channelmask = mask;channelmask;n++,channelmask >>= 1,ch++) {
    // Update the channel?
    if (channelmask & 0x01) {
      UBYTE audc;
      // Check whether we are clocked by the lower level.
      if (AudioCtrl & LinkHiFlag[n]) {
	// Channel is linked to the corresponding even channel, check
	// the even channel for 17Mhz clocking. This is the high-part
	// of the 16 bit counter.
	ch->HiPtr     = &(ch->Zero);
	if (AudioCtrl & Mhz17Flag[n-1])
	  ch->DivNMax = ( ch->AudioF << 8) + ch[-1].AudioF + 7; // clock by 1.79Mhz
	else
	  ch->DivNMax = ((ch->AudioF << 8) + ch[-1].AudioF + 1) * TimeBase;
      } else if (AudioCtrl & LinkLoFlag[n]) {
	// Channel is the low-part counter of the corresponding odd
	// channel. Also adjust the base frequency of this channel and
	// link its counter to the high-part.
	//
	// Must test the corresponding high-part of the channel for
	// short or LONG runs.
	ch->HiPtr        = &(ch[1].DivNCnt);

	if (AudioCtrl & Mhz17Flag[n]) {
	  ch->DivNMax    = ch->AudioF + 7; // clock by 1.79Mhz
	  ch->DivFullMax = 255        + 1; // full wraparound required. FIX: No wraparound full reset required.
	} else {
	  ch->DivNMax    = (ch->AudioF + 1) * TimeBase;
	  ch->DivFullMax = (255        + 1) * TimeBase;
	}
      } else {
	// Channel is unlinked, do not test for high-words.
	ch->HiPtr     = &(ch->Zero);
	if (AudioCtrl & Mhz17Flag[n])
	  ch->DivNMax =  ch->AudioF + 4; // clock by 1.79Mhz
	else
	  ch->DivNMax = (ch->AudioF + 1) * TimeBase;
      }
      //
      // Now truncate the timers: FIX: No, this does not happen on the
      // real pokey. It really starts again only after the timer has run
      // out, so we leave it like this.
      //
      // Check whether we are part of a filtering process. If so, then we cannot
      // disable this channel unless the linked channel is also disabled. Since
      // we check here for the filtering channel and not the filtered channel
      // (see the definition of the mask above), and the filtered channel
      // is always *below* the filtering channel, we can simply check
      // for the ChannelOn flag of the filtered channel which has then
      // been processed already.
      if (AudioCtrl & FilterMask[n]) {
	// Channel is filtering. Enable this channel if the filtered channel
	// is also on. Note that we checked the filtered channel in a previous
	// loop.
	ch->ChannelOn   = ch[-2].ChannelOn;
      } else if (AudioCtrl & LinkHiFlag[n]) {
	// Also unmute if this is the high-part of a linked filter pair because
	// the frequency of the high-filter now impacts the maximum cycle count
	// of the low filter.
	ch->ChannelOn   = ch[-1].ChannelOn;
      } else {
	// Otherwise, disable the channel unless we have reason not to.
	// We also clear the hi-flop of the filtered channel here to reset
	// it back to its original state.
	ch->ChannelOn   = false;

      }
      // We also enable the channel if either the frequency is low enough,
      // VolOnly is turned off and the volume is larger than zero.
      // Unless the channel has volonly set or has a divisor which is too
      // low. 1.79MHz / 22kHz = 81.
      audc       = ch->AudioC;
      ch->AudioV = audc & 0x0f; // volume
      ch->AudioP = audc >> 5;   // polycounter
      if ((audc & 0x0f) && (audc & 0x10) == 0) {
	audc &= 0xe0;
	if (audc != 0xa0 && audc != 0xe0) {
	  // Never mute, this always generates sound
	  ch->ChannelOn = true;
	} else if (ch->DivNMax >= 80) {
	  // Special muting logic for channels that cuts off sound
	  // generation beyond 22kHz
	  ch->ChannelOn = true;
	} else if (ch->HiPtr != &(ch->Zero)) {
	  // Also never mute if the channel is chained.
	  ch->ChannelOn = true;
	} 
      } else if (ch->DivNMax < 80) {
	int m,diff;
	UBYTE audc2;
	// Also unmute the channel in case we generate a mixing frequency
	// that is audible.
	for(m = 0;m < 4;m++) {
	  if (m != n && Ch[m].DivNMax < 80) {
	    audc2    = Ch[m].AudioC;
	    if (AudioCtrl & FilterMask[m]) {
	      // Use the audio control of the filtered channel then for testing.
	      audc2  = audc;
	    }
	    if (audc2 & 0x0f) {
	      audc2 &= 0xf0;
	      if (audc2 == 0xa0 || audc2 == 0xe0) {
		diff = ch->DivNMax - Ch[m].DivNMax;
		if (diff && diff < 10 && diff > -10) {
		  ch->ChannelOn   = true;
		  Ch[m].ChannelOn = true;
		}
	      }
	    }
	  }
	}
      }
      // If the channel is off, disable the counter and the filter.
      if (ch->ChannelOn == false) {
	if (n > 2) 
	  ch[-2].HiFlop = 0x00;
	// Also ensure that we don't misread the counter.
	ch->DivNCnt = 0;
      }
    }
  }
  // 
  // Compute serial transfer timers.
  // If we transmit with 19.200 baud, we get at
  // 1+8+1 bits 1920 byte/second. That makes
  // approximately a byte each eight scanlines.
  // We are a bit more carefully and make this
  // nine. (FS_II)
  // First check for the input clock. This is given
  // by the bits 5 and 4 of SkCtrl. No matter whether
  // this is async or not, the in-clock is controlled
  // by channel 3.
  if (SkCtrl & 0x30) {
    // Not clocked externally, otherwise we do not alter 
    // or change the settings, and hope the best that 
    // no device requires this.
    if (mask & 0x0c) {
      // Input is driven by channel 3 (or 2 and 3)
      SerIn_Delay      = 20 * Ch[3].DivNMax;
      SerIn_Clock      = UWORD((Ch[3].DivNMax < 0xffff)?(Ch[3].DivNMax):(0xffff));
    }
  }
  // Check the output clock mode. This is controlled
  // by bits 6 and 5 of SkCtrl (no typo, bit 5 is for both!)
  switch (SkCtrl & 0x60) {
  case 0x00:
    // Output clock not used. Let's hope the best and do not
    // alter the settings.
    break;
  case 0x20:
  case 0x40:
    // Output clocked by channel 3 (or 2 and 3)
    if (mask & 0x0c) {
      SerOut_Delay     = Ch[3].DivNMax;
      SerOut_Clock     = UWORD((Ch[3].DivNMax < 0xffff)?(Ch[3].DivNMax):(0xffff));
      SerBitOut_Delay  = 2  * SerOut_Delay;
      SerXmtDone_Delay = 20 * SerOut_Delay;
      SerOut_Delay     = (SerOut_Delay > 10)?(SerOut_Delay - 10):(1);
    }
    break;
  case 0x60:
    // Output clocked by channel 1 (or 0 and 1)
    if (mask & 0x03) { 
      SerOut_Delay     = Ch[1].DivNMax;
      SerBitOut_Delay  = 2  * SerOut_Delay;
      SerXmtDone_Delay = 20 * SerOut_Delay;
      SerOut_Delay     = (SerOut_Delay > 10)?(SerOut_Delay - 10):(1);
    }
    break;
  }
  //  
  // Now test the influence of SkCtrl on the audio system. It really
  // makes a difference (it has to, since Pokey is used to drive the
  // serial subsystem), though I wonder whether this has been used
  // to generate audio effects. Anyhow, here we go...
  // This is typically only of importance if we also want to enable 
  // serial transfer sounds.
  if (SIOSound) {
    // Check for channel 0,1 dependencies
    if (mask & 0x03) {
      if ((SkCtrl & 0x60) == 0x60) {
	// Channels 0,1 also used as output clock here.
	// We need to enable them to hear a difference.
	if (Ch[0].AudioC & 0x0f)
	  Ch[0].ChannelOn = true;
	if (Ch[1].AudioC & 0x0f)
	  Ch[1].ChannelOn = true;
      }
    }
    //
    // Test for special async mode hack. Here, the clock
    // is generated internally, but gets synchronized by
    // the external clock input. The timer remains halted(!)
    // until the start bit is received, and then runs freely,
    // up to the stop bit, when it is again halted. Since we
    // cannot hear the high frequency of 19.2 kHz for disk
    // transfer, we only hear the modulation of the signal
    // due to the channel start/stop. This happens
    // approximately at half the transfer rate, therefore
    // this strange formula.
    // *21 approximately *10 * 2. 10 for = 1 start bit,
    // 8 data bits, one stop bit.
    if (mask & 0x0c) {
      if (SkCtrl & 0x10) {
	if (SerInBytes && SerIn_Counter  <= SerIn_Delay) {
	  // Transfer busy, let timer run freely.
	  if (Ch[2].AudioC & 0x0f)
	    Ch[2].ChannelOn = true;
	  if (Ch[3].AudioC & 0x0f) {
	    Ch[3].ChannelOn = true;
	    if (SerInRate < 500) {
	      // FIXME: Ugly hack to get the sound 
	      // approximately right.
	      // See above for how it really works.
	      Ch[3].DivNMax  *= 21;
	    }
	  }
	} else {
	  // Transfer blocked, stop it.
	  Ch[2].ChannelOn = false;
	  Ch[3].ChannelOn = false;
	}
      } else if (SkCtrl & 0x70) {
	// To hear serial output activity, we need
	// to enable channel 3 here as well, unless
	// no output is intended and pokey is reset.
	if (Ch[2].AudioC & 0x0f)
	  Ch[2].ChannelOn = true;	
	if (Ch[3].AudioC & 0x0f)
	  Ch[3].ChannelOn = true;
      }
    }
  }
}
///

/// Pokey::ComputeSamples
// Private for the sound generator: Generate a given number
// of new samples
void Pokey::ComputeSamples(struct AudioBufferBase *to,int size,int dspsamplerate,UBYTE delta)
{
  // Mask in AudioCtrl wether filter is on
  static const UBYTE FilterMask[4] = {0x04,0x02,0x00,0x00};
  int ch;           // channel counter
  ULONG SampleMax    = (Frequency17Mhz << 8) / dspsamplerate;
  LONG  offset       = delta;
  
  if (SIOSound) {
    // Check whether the tape is running and some data comes actually in.
    // If so, misuse the audio channels to re-generate the sound that would
    // normally come from the tape.
    if (AudioCtrl == 0x28) {
      bool bit   = true;
      //
      if (SkCtrl & 0x10) {
	if (SerIn_Counter > 0 && SerInBuffer && SerInRate > 0 && SerInBytes > 0) {
	  int bitposition   = (SerIn_Counter + SerInRate - 1) / SerInRate; // "half-bits"
	  //
	  if (bitposition == 20 || bitposition == 19) { 
	    // The start bit. Actually, 1 1/2 start bits. If we assume that the
	    // receiver waits 1 1/2 bits to sample at the middle of the bit, this
	    // might be ok.
	    bit   = false;
	  } else if (bitposition < 19 && bitposition > 2) {
	    bit   = ((*SerInBuffer >> (7 - ((bitposition - 3) >> 1))) & 0x01)?true:false;
	  }
	}
	Ch[3].AudioV = Ch[3].AudioC & 0x0f;
	if (bit)
	  Ch[3].AudioV = (Ch[3].AudioV * 3) >> 2;
      }
      if (sio && sio->isMotorEnabled()) {
	class Tape *tape = machine->Tape();
	if (tape && tape->isPlaying() && tape->isRecording() == false) {
	  // Misuse channel 3 to generate the sound. It is actually the serial counter.
	  Ch[2].ChannelOn = true;
	  Ch[2].AudioV    = 0x08;
	  Ch[2].AudioP    = 5;
	  if (bit) {
	    Ch[2].DivNMax   = (0x05 + 1) * Base64kHz;
	  } else {
	    Ch[2].DivNMax   = (0x07 + 1) * Base64kHz;
	  }
	}
      }
    }
  }

  if ((SkCtrl & 0x03) == 0) {
    // Sound completely disabled.
    while (size) {
      to->PutSample(0);
      size--;
    }
  } else 
  // Loop until the buffer is filled completely
  while (size) {
    struct Channel *c;
    LONG current    = 0;
    int next_event  = -1;
    LONG event_min  = LONG(SampleCnt >> 8);
    // Check for the next possible event.
    // If no other counter wraparound happens, then this is a
    // "generate output sample" instead.
    //
    // Now check all channels for wrap-around
    for(ch=0,c = Ch;ch<4;ch++,c++) {
      if (c->ChannelOn && (c->DivNCnt <= event_min)) {
	// Yes, this channel is next
	event_min  = c->DivNCnt;
	next_event = ch; // get the channel that caused the event
      }
    }
    //
    // event_min is now the number of cycles to the next possible
    // event, which is either a "generate output sample", or 
    // a "counter wraparound".
    //
    // Now update the output volume so far.
    for(ch = 0,c = Ch;ch < 4;ch++,c++) {
      UBYTE mask     = 0x0f; // volonly: Take output directly, ignore hiflop/outbit.
      // Now check whether the VOLUME_ONLY bit is set. If so, insert the
      // output now. 
      if ((c->AudioC & 0x10) == 0) {
	if (c->ChannelOn) {
	  // Check whether the filter for this channel is on, i.e.
	  // whether this channel gets filtered. The D-FlipFlop
	  // output is updated as soon as the channel driving the flip-flop
	  // gets updated as well.
	  // eXOr with the output of the D-FlipFlop
	  // Generate the XOR of the output channel by the DFlop
	  mask        =  c->HiFlop ^ c->OutBit;
	  // Decrement all counters by the amount of ticks
	  c->DivNCnt -= event_min;
	} else {
	  // Block ("mute") the output completely.
	  mask = 0x00;
	}
      }
      current += mask & c->AudioV;
    }
    //
    // Now generate the output:
    Output     += current * event_min * 3;
    Outcnt     += event_min; // Update tick counter
    // Decrement the sample output counter
    SampleCnt  -= event_min << 8;
    // Adjust the counter for the polycounter offset:
    // keep the number of cycles for the poly counters
    PolyAdjust += event_min;
    //
    // Adjust the polycounter now in case we are not
    // sampling but updating a channel
    if (next_event >= 0) {
      struct Channel *c = Ch + next_event;
      UBYTE audc;
      //
      // Is a channel event. Update polycounters as well
      // Since taking the modulo is a relatively costly operation, try to defer this as much as possible
      // and avoid this computation
      Poly4Ptr      += PolyAdjust;      
      Poly5Ptr      += PolyAdjust;      
      Poly9Ptr      += PolyAdjust;      
      Poly17Ptr     += PolyAdjust;
      if (Poly4Ptr  >= Poly4End) {
	Poly4Ptr     = ((Poly4Ptr  - PolyCounter4)   % Poly4Size ) + PolyCounter4;
      }
      if (Poly5Ptr  >= Poly5End) {
	Poly5Ptr     = ((Poly5Ptr  - PolyCounter5)   % Poly5Size ) + PolyCounter5;
      }
      if (Poly9Ptr  >= Poly9End) {
	Poly9Ptr     = ((Poly9Ptr  - PolyCounter9)  % Poly9Size ) + PolyCounter9;
      }
      if (Poly17Ptr >= Poly17End) {
	Poly17Ptr    = ((Poly17Ptr - PolyCounter17) % Poly17Size) + PolyCounter17;
      }
      //
      // reset the counters now
      PolyAdjust  = 0;
      //
      // adjust channel counter, reset back. The frequency depends on the state
      // of the corresponding high-counter.
      if (*c->HiPtr <  c->DivNMax) {
	c->DivNCnt  += c->DivNMax;
      } else {
	// Otherwise, use a frequency counter base of 256
	c->DivNCnt  += c->DivFullMax;
      }
      //        
      // Now emulate the high-pass filters by triggering the
      // D-FlipFlops of the channels; only channels 2 or 3
      // can do that. Note that the HiFlop gets reset as soon
      // as we poke AUDIOCTRL.
      if (next_event >= 2) {
	int filtered_channel = next_event - 2;
	if (AudioCtrl & FilterMask[filtered_channel]) {
	  // Put the output the channel onto the alternate input of the EXOR gate
	  // resulting in a total volume of zero for the filtered channel.
	  Ch[filtered_channel].HiFlop = Ch[filtered_channel].OutBit; 
	} 
      }
      // Now check the AudioControl for which polycounter we should
      // use for updating. All this information is encoded in the polypointer
      // array indexed by the audc >> 5.
      audc = c->AudioP;
      // Check for the first gate. This is the polycounter #5 or no polycounter
      // at all.
      if (**PolyPointerFirst[audc]) {
	UBYTE out;
	// Polycounter 5 disabled or Polycounter generated
	// an event.      
	// Toggle the output already. The next code checks whether we
	// make use of the result.
	out  = c->OutBit ^ 0x0f;
	// Now check for the secound polycounter. This is not a simple gate,
	// but a comparison with the current out to emulate the N-2 rule
	// that applies here for the divisor.
	if (PolyPointerSecond[audc] == NULL || **PolyPointerSecond[audc] == out) {
	  c->OutBit = out;
	}
      }
      //
      // Experimental two-tone emulation.
      if (SkCtrl & 0x08) {
	switch(next_event) {
	case 1: // Channel 1 syncs channel 0 always.
	  Ch[0].DivNCnt   = Ch[0].DivNMax;
	  break;
	case 0: // Channel 0 syncs channel 1 if the serial register is set.
	  if ((SerOut_Register & 0x01) && (SkCtrl & 0x80) == 0)
	    Ch[1].DivNCnt = Ch[1].DivNMax;
	  break;
	}
      }
      //
      // End of audio channel event.
    } else {
      UBYTE out;
      LONG  val;
      // Sample output generation starts now. Update counter
      // for event generation now. This is again a 56.8 bit
      // fractional
      SampleCnt  += SampleMax;
      // If we generated any output so far, enter it now.
      if (Outcnt > 0) {
	// Average over all samples computed so far
	out = UBYTE(offset + Output / Outcnt);
      } else {
	// Output zero
	out = UBYTE(offset);
      }  
      //
      // Fetch output value.
      if (DCFilterConstant) {
	val = OutputMapping[out] - DCLevelShift;
	// Update the DC average now.
	DCAverage += val;
	if (val > 127 || DCAverage > DCFilterConstant) {
	  if (DCLevelShift < 127)
	    DCLevelShift++;
	  DCAverage = 0;
	}
	if (val < -128 || DCAverage < -DCFilterConstant) {
	  if (DCLevelShift > -128)
	    DCLevelShift--;
	  DCAverage = 0;
	}
	if (val < -128)
	  val = -128;
	if (val > 127)
	  val = 127;
      } else {
	val = OutputMapping[out];
      }
      // Output the sample over the channel.
      to->PutSample(UBYTE(val));
      size--;
      // Decrement the number of generated samples now
      Output = 0;
      Outcnt = 0;
    } // of "generated a channel, else part"
  } // of generation loop
  //printf("%ld loops required\n",lpcnt);
}
///

/// Pokey::GenerateIRQ
// Generate a pokey IRQ of the given bits
// in positive logic.
void Pokey::GenerateIRQ(UBYTE bits)
{
  // Signal the bits in IRQStat
  IRQStat  &= ~bits;
  if (IRQEnable & bits) {
    // Forward this to the CPU if enabled
    PullIRQ();
  }
}
///

/// Pokey::UpdatePots
// Advance the measurement of the potentiometer (A/D converter) input
// by N steps. Depending on the measurement mode, this is either a
// line-based or a cycle-based measurement.
void Pokey::UpdatePots(int steps)
{
  int ch;

  // Now update the pot reader.
  for(ch = 0;ch < 8;ch++) {
    int updated;
    // Bump the counter by the counter increment. If we
    // reach the maximum (or beyond), then stop.
    updated = PotNCnt[ch] + steps;
    if (updated >= PotNMax[ch]) {
      // Read is ready, signal in allPot by clearing the
      // apropriate bit, and insert the proper value.
      AllPot      &= ~(1<<ch);
      PotNCnt[ch]  = PotNMax[ch];
    } else {
      PotNCnt[ch]  = updated;
    }
  }
}
///

/// Pokey::GoNSteps
// Forward the pokey status for N steps.
void Pokey::GoNSteps(int steps)
{
  static const UBYTE irqbits[4]    = { 0x01, 0x02, 0x00, 0x04 };
  static const UBYTE LinkLoFlag[4] = { 0x10, 0x00, 0x08, 0x00 };
  struct Channel *c;
  int ch,lastch;
  //
  // Serial bus handling. That is, updating the serial input,
  // output and done flags.
  if (SkCtrl & 0x03) {
    // Check whether we are at concurrent reading. If so,
    // Check whether there's new input data for us.
    // Do not try to receive a new byte when the sender
    // still pretends to be busy.
    if (ConcurrentBusy     == false && 
	SerIn_Counter      == 0 &&
	(SkCtrl & 0xf0)    == 0x70) {
      UBYTE in;
      //
      if (sio && sio->ConcurrentRead(in)) {
	ConcurrentInput = in;
	SerIn_Counter   = 1;    // generate an interrupt below, immediately.
	SerInBytes      = 1;    // have one byte in the queue now
	ConcurrentBusy  = true; // do not try to deliver this until the atari collects it
      }
    }
    //
    // Check for serial input done.
    if (SerIn_Counter > 0) {
      bool update = (SerInBytes && SerIn_Counter > SerIn_Delay)?true:false;
      if ((SerIn_Counter -= steps) <= 0) {  
	// If we parsed the line manually, check whether
	// the stop bit was parsed as well. If so, the data
	// is here (incorrectly) not passed into the serial shift
	// register but removed manually, and serial input proceeds.
	if (SerInManual && SerInRate) {
	  SerIn_Counter += SerInRate * 20;
	  SerInManual    = false;
	  if (SerInBytes > 0) {
	    SerInBuffer++;
	    // More serial data available? If so, expect the next byte soon
	    SerInBytes--;
	  }
	} else {
	  // Automatic mode, receive through pokey.
	  SerIn_Counter = 0;
	  // Check whether SIO delivered any new bytes. If so,
	  // generate an IRQ to deliver them.
	  if (SerInBytes) {
	    // Serial input just finished. Generate an IRQ.   
	    GenerateIRQ(0x20);
	  } else if (sio) {
	    // Ask SIO again.
	    sio->RequestInput();
	  }
	}
      }
      if (update && SerInBytes && SerIn_Counter <= SerIn_Delay && SIOSound)
	UpdateSound(0x0c);
    }
    //
    // Check whether there is some output from the serial register.
    if (SerBitOut_Counter > 0) {
      if ((SerBitOut_Counter -= steps) <= 0) {
	// Reload the counter if there are any bits left.
	// While this logic should actually drive the SerXmtDone mechanism,
	// it does not - for hysterical raisons. The bitcounter logic is
	// only part of the two-tone emulation.
	if (SerOut_BitCounter && --SerOut_BitCounter) {
	  // Align the phase of timers 0 and 1 in two-tone mode.
	  if (SkCtrl & 0x08) {
	    Ch[0].DivNIRQ = Ch[0].DivNMax;
	    Ch[1].DivNIRQ = Ch[1].DivNMax;
	    Ch[0].DivNCnt = 0;
	    Ch[1].DivNCnt = 0;
	  }
	  //
	  SerBitOut_Counter = SerBitOut_Delay;
	  SerOut_Register >>= 1;      // next bit.
	  SerOut_Register  |= 0x8000; // shift in empty bits.
	}
      }
    }
    //
    // Check for serial output register empty.
    if (SerOut_Counter > 0) {
      if ((SerOut_Counter -= steps) <= 0) {
	SerOut_Counter = 0;
	// Serial output register empty and free to
	// be reloaded. Generate an interrupt by bit 4.
	GenerateIRQ(0x10);
	//
	// SerOut is now empty. Generate XMTDone when completely
	// done.
	SerXmtDone_Counter = SerXmtDone_Delay;
	//
	// Load the serial output buffer into the shift register.
	// Remember that there is an additional start bit and an
	// additional stop bit.
	SerOut_Register    = SerOut_Buffer << 1; // insert the zero stop bit.
	SerOut_Register   |= 0xfe00;             // insert stop bits
	SerOut_BitCounter  = 10; // 10 bits to be shifted.
	SerBitOut_Counter  = SerBitOut_Delay;
      }
    }
    //
    // Check whether the serial output register just
    // became empty. If so, generate an IRQ. Note
    // that we actually do not drive bit 3 of IRQStat
    // from here as this bit is unlatched (hardware manual)
    if (SerXmtDone_Counter > 0) {
      if ((SerXmtDone_Counter -= steps) <= 0) {
	SerXmtDone_Counter = 0;
	GenerateIRQ(0x08);
      }
    }
    //
    // Now check for Pokey timers. Actually, we won't
    // need to emulate them very precisely. Or not?
    if (SkCtrl & 0x10) {
      // Async mode on, timer 4 is triggered externally.
      lastch = 2;
      // This locks timer 4 into reset. There is actually no
      // timer 3 interrupt, but for consisteny, reset it as well.
      Ch[2].DivNIRQ = Ch[2].DivNMax;
      Ch[3].DivNIRQ = Ch[3].DivNMax;
    } else {
      lastch = 4;
    }
    //
    // Pokey timers. In reality, audio and IRQ are synchronous,
    // they are not here - reason is that the audio timing is
    // the real-world clock, but the IRQ timing is the emulated
    // CPU clock.
    for(ch = 0, c = Ch;ch < lastch;ch++,c++) {
      // Yes, channel 3 cannot generate interrupts at all...
      if ((c->DivNIRQ -= steps) <= 0) {
	//
	// Synchronized two-tone mode?
	if (SkCtrl & 0x08) {
	  switch(ch) {
	  case 1:
	    // Channel 1 syncs channel 0 always.
	    Ch[0].DivNIRQ = Ch[0].DivNMax;
	    break;
	  case 0:
	    // Channel 0 syncs channel 1 if the serial register is set.
	    if ((SerOut_Register & 0x01) && (SkCtrl & 0x80) == 0)
	      Ch[1].DivNIRQ = Ch[1].DivNMax;
	    break;
	  }
	}
	//
	// Generate an IRQ now (or not for channel 3)
	if (IRQEnable & irqbits[ch]) {
	  GenerateIRQ(irqbits[ch]);
	}	
	//
	// Reset the counters.
	if ((AudioCtrl & LinkLoFlag[ch]) && c[1].DivNIRQ >= 0x100) {
	  // This is the tricky case. The low-part of
	  // the 16-bit counter with a partial underflow.
	  c->DivNIRQ  += c->DivFullMax;
	} else {
	  // Average out errors by adding the timer constant
	  // Equivalent if underflow to zero, otherwise its
	  // the correct timing at least on average.
	  c->DivNIRQ  += c->DivNMax;
	}
      }
    }
    //
    // Potentiometer increment in the fast most. 
    // Check how fast we should update the counters. We
    // set the counter increment value depending on whether
    // fast or slow pot reading is set. Maybe this is not quite
    // correct since we cannot remove the loading capacitors
    // in the real emulator. Need to check this.
    if (SkCtrl & 0x04) {
      UpdatePots(steps);
    }
    //
    // Update the random generator, advance by the given number of steps.
    Random9Ptr  += steps;
    if (Random9Ptr  >= Random9End) {
      Random9Ptr     = ((Random9Ptr  - Random9 ) % Poly9Size ) + Random9;
    } 
    Random17Ptr += steps;
    if (Random17Ptr >= Random17End) {
      Random17Ptr    = ((Random17Ptr - Random17) % Poly17Size) + Random17;
    }
  }
}
///

/// Pokey::Step
// Forward by one CPU step
void Pokey::Step(void)
{
  GoNSteps(1);
}
///

/// Pokey::VBI
// VBI activity: This is only used here for dumping POKEY registers to generate
// SAP files if this feature is enabled.
void Pokey::VBI(class Timer *,bool,bool pause)
{
  if (pause == false && EnableSAP) {
    // Check whether it makes sense to open up the SAP output.
    if (SAPOutput == NULL && SongName && *SongName) {
      bool enable = false;
      int channel;
      for(channel = 0;channel < 4;channel++) {
	struct Channel *ch = Ch + channel;
	if ((ch->AudioC & 0x0f) && (ch->AudioC & 0x10) == 0x00)
	  enable = true;
      }
      if (enable) {
	char filename[256];
	snprintf(filename,sizeof(filename),"%s.sap",SongName);
	SAPOutput = fopen(filename,"wb");
	if (SAPOutput == NULL) {
	  ThrowIo("Pokey::ParseArgs","unable to create the SAP output file");
	} else {
	  fprintf(SAPOutput,"SAP\r\n"
		  "AUTHOR \"%s\"\r\n"
		  "NAME \"%s\"\r\n"
		  "TYPE R\r\n"
		  "FASTPLAY %d\r\n"
		  "\r\n",
		  (AuthorName && *AuthorName)?AuthorName:("<?>"),
		  SongName,
		  NTSC?262:312);
	}
      }
    }
    if (SAPOutput) {
      int channel;
      for(channel = 0;channel < 4;channel++) {
	struct Channel *ch = Ch + channel;
	putc(ch->AudioF,SAPOutput);
	putc(ch->AudioC,SAPOutput);
      }
      putc(AudioCtrl,SAPOutput);
    }
  }
}
///

/// Pokey::HBI
// Signal Pokey that a new scanline just
// begun. This is not related to any real effect. 
// In fact, Pokey doesn't care about scanlines in
// reality at all. All we do here is to drive Pokey
// by a 15Khz (= one horizontal blank) frequency to
// emulate the timer and serial port specific settings.
void Pokey::HBI(void)
{
  // First, update the audio machine if required to do so.
  // This must be run very frequently to ensure that
  // we get proper audio output and the output buffers don't
  // run empty.
  // No, we no longer do this. TriggerSoundScanline could
  // do if the driver requires, but UpdateSound should only
  // be called if a pokey change has been made.
  // sound->UpdateSound();
  // Let the sound driver know that 1/15Khz seconds passed.
  // This might be required for resynchronization of the
  // sound driver. This is now run by the HBI timer.
  // sound->TriggerSoundScanline();
  //
  // Nothing happens if pokey is resetting.
  if (SkCtrl & 0x03) {
    // Potentiometer increment in the slow most. 
    // Check how fast we should update the counters. We
    // set the counter increment value depending on whether
    // fast or slow pot reading is set. Maybe this is not quite
    // correct since we cannot remove the loading capacitors
    // in the real emulator. Need to check this.
    if ((SkCtrl & 0x04) == 0) {
      UpdatePots(1);
    }
    //
    // Now check keyboard input if we are connected to
    // the keyboard
    if (keyboard) {
      if (IRQEnable & 0x80) {
	if (keyboard->BreakInterrupt()) {
	  GenerateIRQ(0x80);
	}
      }
      if (SkCtrl & 0x02) {
	if (IRQEnable & 0x40) {
	  if (keyboard->KeyboardInterrupt()) {
	    GenerateIRQ(0x40);
	  }
	}
      }
    }
  } 
  //
  // Update the HBI event by one.
  if (!CycleTimers) {
    GoNSteps(Base15kHz);
  }
}
///

/// Pokey::ComplexRead
// Read a byte from Pokey
UBYTE Pokey::ComplexRead(ADR mem)
{
  switch(mem & 0x0f) {
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x07:
    // Potentiometer/Paddle input read
    return PotNRead(mem & 0x0f);
  case 0x08:
    // Read the potentiometer flags
    return AllPotRead();
  case 0x09:
    // The keyboard input read
    return KBCodeRead();
  case 0x0a:
    // The random number generator
    return RandomRead();
  case 0x0d:
    // Serial shift register read.
    return SerInRead();
  case 0x0e:
    // Interrupt pending bits
    return IRQStatRead();
  case 0x0f:
    // Serial port status read
    return SkStatRead();
  }
  // Unused registers.
  return 0xff;
}
///

/// Pokey::ComplexWrite
// Emulate a pokey byte write to a given
// address.
void Pokey::ComplexWrite(ADR mem,UBYTE val)
{
  switch(mem & 0x0f) {
  case 0x00:
  case 0x02:
  case 0x04:
  case 0x06:
    // Set an audio frequency write now
    AudioFWrite((mem & 0x0f) >> 1,val);
    return;
  case 0x01:
  case 0x03:
  case 0x05:
  case 0x07:
    // Set an audio control register here.
    AudioCWrite((mem & 0x0f) >> 1,val);
    return;
  case 0x08:
    AudioCtrlWrite(val);
    return;
  case 0x09:
    STimerWrite();
    return;
  case 0x0a:
    SkStatClear();
    return;
  case 0x0b:
    PotGoWrite();
    return;
  case 0x0c:
    // This does nothing
    return;
  case 0x0d:
    // Serial output register put
    SerOutWrite(val);
    return;
  case 0x0e:
    // IRQ Enable definition
    IRQEnWrite(val);
    return;
  case 0x0f:
    // Serial Control Register Write
    SkCtrlWrite(val);
    return;
  }
}
///

/// Pokey::PotNRead
// Read one of the paddles
UBYTE Pokey::PotNRead(int n)
{
  return PotNCnt[n];
}
///

/// Pokey::KBCodeRead
// Return the latest keyboard code now
UBYTE Pokey::KBCodeRead(void)
{
  if (keyboard && (SkCtrl & 2)) {
    if (machine->MachType() != Mach_5200) {
      return keyboard->ReadKeyCode();
    } else {
      UBYTE code = keyboard->ReadKeyCode();
      if (keyboard->KeyboardStatus() & 0x04) {
	code    |= 0x20;
      }
      return code;
    }
  }
  return 0x3f;
}
///

/// Pokey::RandomRead
// Read the random number generator
UBYTE Pokey::RandomRead(void)
{
  if (SkCtrl & 0x03) {
    // Go to the pokey random generator, which is either the 9 or 17 bit
    // output, depending on AudCtrl.
    if (AudioCtrl & 0x80) {
      // The 9 bit poly counter.
      if (CycleTimers) {
	return *Random9Ptr;
      } else {
	// Use also the CPU because otherwise the random generator is not very random...
	return Random9[(Random9Ptr - Random9 + machine->CPU()->CurrentXPos()) % Poly9Size];
      }
    } else {
      // The 17 bit poly counter.
      if (CycleTimers) {
	return *Random17Ptr;
      } else {
	return Random17[(Random17Ptr - Random17 + machine->CPU()->CurrentXPos()) % Poly17Size];
      }
    }
  } else {
    // This is also the first entry.
    return 0xff;
  }
}
///

/// Pokey::SerInRead
// Read the serial shift register of Pokey
UBYTE Pokey::SerInRead(void)
{
  // Check whether all the hardware registers are setup
  // correctly to receive data at 19200 baud for emulating
  // the disk station. 
  if (sio) {
    // Note that this might require changes if we are emulating
    // some kind of happy.
    if ((SkCtrl & 0xf0) == 0x10) {    // Transfer mode fine?
      if ((AudioCtrl & 0x28)==0x28) { // Timer setup fine? Connect channels 3,4	
	//
	if (ConcurrentBusy) {
	  machine->PutWarning("Pokey::SerInRead: Concurrent serial input pending.\n");
	  ConcurrentBusy = false;
	  SerInBytes     = 0;
	}
	//
	// No else here, this is not redundant. The above might
	// have caused a side effect!
	if (SerInBytes > 0) {
	  UBYTE byte;
	  // Check whether the baud rate from the external source fits
	  // the expectations. If not, read in something garbled.
	  byte = *SerInBuffer++;
	  if (SerInRate != SerialReceiveSpeed()) {
	    // Ok, does not agree. Actually, for the tape the serial speed will
	    // almost never agree. So check whether the speed varies "too much",
	    // i.e. more than one bit in a byte, and fail then.
	    int bitrate  = SerialReceiveSpeed();
	    int recrate  = 20 * SerInRate;
	    int byterate = 20 * bitrate;
	    int delta    = recrate - byterate;
	    if (delta < -bitrate || delta > bitrate) {
	      // Difference is too large, fail
	      do {
		byte ^= UBYTE(rand() >> 8);
	      } while(byte == 'A' || byte == 'C');
	      // Just make sure that we don't generate a SIO 'ok' on error...
	    }
	  } 
	  //
	  // More serial data available? If so, expect the next byte soon
	  SerInBytes--;
	  // Check whether the input buffer is empty. If so, check whether
	  // SIO has any new bytes for us to offer. If so, then this method
	  // would call SignalSerialBytes to feed more.
	  if (SerInBytes == 0) {
	    sio->RequestInput();
	    // UpdateSound gets called implicitly by this thru
	    // SignalSerialBytes
	  } else {
	    SerIn_Counter  = SerIn_Delay;	  
	    // Update the sound as we received the next byte
	    /*
	    ** If we do, then the sound doesn't sound right, so
	    ** it's disabled here again.
	    if (SIOSound)
	      UpdateSound(0x0c);
	    **
	    */
	  }
	  //
	  //
	  return byte;
	} else {
	  machine->PutWarning("Pokey::SerInRead: Unexpected serial port reading.\n");
	}
      }
      // Otherwise, signal a framing error
      SkStat    |= 0x80;
      SerInBytes = 0;
    } else if ((SkCtrl & 0xf0) == 0x70) {
      // This transfer mode is ised for the concurrent mode of the 850
      // interface box.
      if (AudioCtrl == 0x78) {
	// Return the input byte of the last concurrent read.
	SerInBytes     = 0; // Declare serial input as handled.
	ConcurrentBusy = false;
	return ConcurrentInput;
      } else {
	// Otherwise, again a framing error.
	SkStat        |= 0x80;
	SerInBytes     = 0;
	ConcurrentBusy = false;
      }
    } else {
      if (SerInBytes) {
	machine->PutWarning("Pokey::SerInRead: Serial transfer mode unsuitable for waiting serial data.\n");
	ConcurrentBusy = false;
	SerInBytes     = 0;
	SkStat        |= 0x80;
      }
    }
  }
  return 0xff;
}
///

/// Pokey::IRQStatRead
// Read the IRQ status register of Pokey
UBYTE Pokey::IRQStatRead(void)
{
  // Note that bit 3 is not a latch. It is controlled directly
  // by the status of the serial output register.
  if (SerXmtDone_Counter > 0) {
    // Serial out register is not empty.
    return IRQStat | 0x08; // set bit 3
  } else {
    return IRQStat & 0xf7; // clear bit 3, serial output done
  }
}
///

/// Pokey::SkStatRead
// Return the serial port status of Pokey
UBYTE Pokey::SkStatRead(void)
{
  UBYTE out = SkStat | 0x01; // bit 0 is always set
  
  if (keyboard && (SkCtrl & 2)) {
    out |= keyboard->KeyboardStatus();
  } else {
    out |= 0x0c;
  }
  
  if (SerIn_Counter == 0) // Serial input register is currently busy?
    out |= 0x02; // no, it's not.

  //
  // Emulate direct reading from the serial input and mask in
  // the serial data when we're close to completing the input.
  // This emulates a device that operates at the 19200 baud the
  // SIO chain operates with, and does not recognize any internal
  // clock.
  if (SerIn_Counter > 0 && SerInBuffer && SerInRate > 0) {
    int bitposition   = (SerIn_Counter + SerInRate - 1) / SerInRate; // "half-bits"
    bool bit          = true; // default level on the serial line is high.
    //
    if (bitposition == 20 || bitposition == 19) { 
      // The start bit. Actually, 1 1/2 start bits. If we assume that the
      // receiver waits 1 1/2 bits to sample at the middle of the bit, this
      // might be ok.
      bit = false;
    } else if (bitposition < 19 && bitposition > 2) {
      bit = ((*SerInBuffer >> (7 - ((bitposition - 3) >> 1))) & 0x01)?true:false;
    } else if (bitposition <= 2) {
      // We are reading the stop bits. Consider this serial input as "missed".
      SerInManual = true;
    }
    //
    // Stop bits are one.
    //
    // Now insert the bit into the output.
    if (bit) {
      out |=  0x10;
    } else {
      out &= ~0x10;
    }
  }

  return out;
}
///

/// Pokey::AudioFWrite
// Setup the frequency of a channel. Might
// require updating for other channels as
// well.
void Pokey::AudioFWrite(int channel,UBYTE val)
{
  static const UBYTE connectmask[4] = {0x10,0x00,0x08,0x00};
  struct Channel *ch                = Ch + channel;
  
  // Check whether this differs in any way from the
  // current value. If it doesn't, we don't need to
  // care about this trouble.
  if (val != ch->AudioF) {
    ch->AudioF = val;
    // Now this is the modified value. Check whether this
    // audio channel is connected to some else.
    if (AudioCtrl & connectmask[channel]) {
      // This channel drives the next higher one
      UpdateSound((1<<channel) | (1<<(channel+1)));
    } else {
      // This channel is alone
      UpdateSound(1<<channel);
    }
  }
  sound->UpdateSound();
}
///

/// Pokey::AudioCWrite
// Write into an audio channel control register
void Pokey::AudioCWrite(int channel,UBYTE val)
{
  struct Channel *ch = Ch + channel;
  //
  if (val != ch->AudioC) {
    ch->AudioC = val;
    UpdateSound(1<<channel);
  }
  sound->UpdateSound();
}
///

/// Pokey::AudioCtrlWrite
// Write into the audio control global register
void Pokey::AudioCtrlWrite(UBYTE val)
{
  if (val != AudioCtrl) {
    AudioCtrl = val;
    UpdateSound(0x0f); // Update all four channels
  }
  sound->UpdateSound();
}
///

/// Pokey::STimerWrite
// Write into the Pokey STimer to reset
// all timers to a definite state
void Pokey::STimerWrite(void)
{
  int ch;
  // This resets all counters to zero and
  // initializes the output flip-flops
  for(ch=0;ch < 4;ch++) {
    Ch[ch].DivNCnt =  Ch[ch].DivNMax+4;
    Ch[ch].DivNIRQ =  Ch[ch].DivNMax+4;
  }
  //
  // Clock reset is offset if timers drive each
  // other since the underrun requires some time
  // to propagade.
  if ((AudioCtrl & 0x50) == 0x50) {
    // 1.79 Mhz clock on channel 0 also driving channel 1.
    Ch[0].DivNCnt -= 3;
    Ch[0].DivNIRQ -= 3;
  }
  if ((AudioCtrl & 0x28) == 0x38) {
    // 1.79 Mhz clock on channel 2 also driving channel 3.
    Ch[0].DivNCnt -= 3;
    Ch[0].DivNIRQ -= 3;
  }
  //
  // STimer does not reset the polycounters
  // Channel 0,1 output is set to low, Channel 2,3 output
  // is set to high. Strange enough, but the manual says so.
  Ch[0].OutBit = Ch[1].OutBit = 0x00;
  Ch[2].OutBit = Ch[3].OutBit = 0x0f;
  sound->UpdateSound();
}
///

/// Pokey::SerOutWrite
// Write a data item over the serial port
void Pokey::SerOutWrite(UBYTE val)
{
  // First check whether we are connected at
  // 19200 baud. If not, ignore the request.
  // Note that we have to modify this here
  // in case we have to emulate Happys
  if (SkCtrl & 0xf0) {    // Transfer mode fine?
    if (sio && (AudioCtrl & 0x28) == 0x28) { // Timer setup fine? Connect channels 3,4
      if ((SkCtrl & 0xf0) == 0x70) {
	// Hacky. This is used for the concurrent mode.
	// Transmit the byte thru the concurrent channel
	sio->ConcurrentWrite(val);
      } else if (SkCtrl & 0x08) {
	// Two-tone mode enabled. Ok, write tape data.
	sio->TapeWrite(val);
      } else {
	// This is used for the regular SIO mode.
	sio->WriteByte(val);
      }
    }
    // Signal the serial out and serial done IRQ
    // Any serial output that is still running must be included in
    // the delay.
    if (SerXmtDone_Counter) {
      SerOut_Counter     = SerXmtDone_Delay;
    } else {
      SerOut_Counter     = SerOut_Delay;
    }
    //
    // Place into the output buffer from where it is transfered into
    // the shift register.
    SerOut_Buffer = val;
    //
    // Serial port cannot go empty as we just refilled it.
    SerXmtDone_Counter = 0;
    if (SIOSound)
      UpdateSound(0x0c);
  }
}
///

/// Pokey::IRQEnWrite
// Emulate a write into the Pokey interrupt enable
// register here.
void Pokey::IRQEnWrite(UBYTE val)
{
  IRQEnable = val;
  IRQStat  |= ~val; // clear the corresponding interrupts as well. HW manual says so
  //
  // Special case for XmtDone: As LONG as the counter is still zero, try to
  // drop the interrupt immediately.
  if (SerXmtDone_Counter > 0) {
    // Serial out register is not empty.
    IRQStat |= 0x08; // set bit 3
  } else {
    IRQStat &= 0xf7; // clear bit 3, serial output done
    GenerateIRQ(0x08);
  }
  //
  // Check whether any IRQ is still pending. If not so, drop the line at the CPU
  // here.
  if ((IRQEnable & IRQStat) == IRQEnable) {
    DropIRQ();
  }
}
///

/// Pokey::SkStatClear
// Reset all accumulated errors of the SkStat register
void Pokey::SkStatClear(void)
{
  // All this write does is to reset bits 7..5 of
  // skstat.
  SkStat |= 0xe0;
}
///

/// Pokey::SkCtrlWrite
// Emulate a write into the serial status register
void Pokey::SkCtrlWrite(UBYTE val)
{
  if ((val & 0x03) == 0) {
    // Also resets the polycounters: The CPU steps us
    // *after* the execution, so we must be one step ahead. Bummer!
    PolyNonePtr          = PolyCounterNone;
    Poly4Ptr             = PolyCounter4  + Poly4Size  - 1;
    Poly5Ptr             = PolyCounter5  + Poly5Size  - 1;  
    Poly9Ptr             = PolyCounter9  + Poly9Size  - 1;
    Poly17Ptr            = PolyCounter17 + Poly17Size - 1;
    Random9Ptr           = Random9       + Poly9Size  - 1;
    Random17Ptr          = Random17      + Poly17Size - 1; 
    //
    // Reset the serial port output. Cannot reset the input
    // since that's on a different hardware.
    SerOut_Counter     = 0;
    SerXmtDone_Counter = 0;
    SerOut_Buffer      = 0xff;
    SerOut_Register    = 0xffff;
    SerBitOut_Counter  = 0;
    SerOut_BitCounter  = 0;
    //
    // Also reset the counters.
    STimerWrite();
  }
  //
  // SkCtrl also has influence on the audio subsystem
  if (SkCtrl != val) {
    SkCtrl = val;
    UpdateSound(0x0f); // Update all four channels
  }
}
///

/// Pokey::PotGoWrite
// Write the pot write and start the potentiometer
// measurements.
void Pokey::PotGoWrite(void)
{
  int ch;
  // Reset all counters, read paddle values for the maximum values,
  // clear the AllPot registers.
  memset(PotNCnt,0,sizeof(PotNCnt));
  //
  for(ch=0;ch<8;ch++) {
    // Set the maximum value of the potentiometer to the
    // controller value. Only unit #0 is connected to the
    // paddles. All others are "open".
    if (Unit == 0) {
      PotNMax[ch] = machine->Paddle(ch)->Paddle();
    } else {
      PotNMax[ch] = 228;
    }
  }
  AllPot = 0xff;
}
///

/// Pokey::AllPotRead
// Read the potentiometer flag register.
UBYTE Pokey::AllPotRead(void)
{
  return AllPot;
}
///

/// Pokey::SignalSerialBytes
// Signal the arrival of one/several serial bytes after
// n 15Khz steps steps. Note that we might signal
// zero bytes here in case we just want pokey to ask
// back later...
void Pokey::SignalSerialBytes(UBYTE *data,int num,UWORD delay,UWORD baudrate)
{
  if (SerIn_Counter > 0 || SerInBytes) {
    // Serial problem of pokey: Serial device miscommunication,
    // we haven't read the last byte yet
    machine->PutWarning("Clashing read on serial input line:\n"
			"Trying to feed another input while serial transfer "
			"is still busy.\n");
  }
  SerInBuffer    = data;
  SerInBytes     = num;
  SerInRate      = baudrate;
  SerIn_Counter  = delay * Base15kHz + SerIn_Delay;
  //
  //
  // Update channels 3&4 that need to sync to this input
  if (SIOSound)
    UpdateSound(0x0c);
}
///

/// Pokey::SignalCommandFrame
// Signal that a command frame has been signaled and that we therefore
// abort incoming IO traffic. This is a hack to enforce resynchronization
// and it shoudn't do anything if all goes right.
void Pokey::SignalCommandFrame(void)
{
  if (SerIn_Counter > 0 || SerOut_Counter > 0 || SerInBytes) {
    // Serial problem of pokey: Serial device miscommunication,
    // we haven't read the last byte yet
    machine->PutWarning("Clashing command frame on serial input line:\n"
			"Trying to send another command while serial transfer "
			"is still busy.\n");
    SerInBytes     = 0;
    SerIn_Counter  = 0;
    SerOut_Counter = 0;
  }
}
///

/// Pokey::UpdateAudioMapping
// Recompute the output mapping now.
void Pokey::UpdateAudioMapping(void)
{
  double gamma = double(Gamma)  / 100.0;
  double vol   = (double(Volume) / 100.0) * 127.0;
  double in;
  int i,out;
  //
  // This is the traditional gamma curve mapping with amplification after
  // mapping.
  for(i=0;i<256;i++) {
    // Get the input amplitude normalized to 1.0
    in  = double(i) / 255.0;
    out = int(0.5 + pow(in,gamma) * vol) - 128;
    // Clip to the nominal range of 0..127 (note that we're unsigned seven bit)
    if (out > 127)
      out = 127;
    if (out < -128)
      out = -128;
    OutputMapping[i] = out;
  }
}
///


/// Pokey::SerialTransmitSpeed
// Return the serial output speed as cycles of the 1.79Mhz clock.
UWORD Pokey::SerialTransmitSpeed(void) const
{
  return SerOut_Clock;
}
///

/// Pokey::SerialReceiveSpeed
// Return the serial input speed as cycles of the 1.79Mhz clock.
UWORD Pokey::SerialReceiveSpeed(void) const
{ 
  return SerIn_Clock;
}
///

/// Pokey::DisplayStatus
// Print the status of the chip over the monitor
void Pokey::DisplayStatus(class Monitor *mon)
{
  char serin[2];

  if (SerInBytes > 0 && SerInBuffer) {
    UBYTE t  = *SerInBuffer >> 4;
    serin[0] = (t > 9)?('a'+t-10):('0'+t);
    t        = *SerInBuffer & 0x0f;
    serin[1] = (t > 9)?('a'+t-10):('0'+t);
  } else {
    serin[0] = '?';
    serin[1] = '?';
  }

  mon->PrintStatus("Pokey.%d Status:\n"
		   "\tAudioFreq0: %02x\tAudioFreq1: %02x\tAudioFreq2: %02x\tAudioFreq3: %02x\n"
		   "\tAudioCtrl0: %02x\tAudioCtrl1: %02x\tAudioCtrl2: %02x\tAudioCtrl3: %02x\n"
		   "\tCounter0: %04x\tCounter1: %04x\tCounter2: %04x\tCounter3  : %04x\n"
		   "\tMax0    : %04x\tMax1    : %04x\tMax2    : %04x\tMax3      : %04x\n"
		   "\tAudioCtrl : %02x\tSkStat    : %02x\tSkCtrl    : %02x\tKeyCode   : %02x\n"
		   "\tIRQStat   : %02x\tIRQEnable : %02x\n"
		   "\tSerInDly  : " ATARIPP_LD "\tSerOutDly : " ATARIPP_LD "\tSerXmtDly : " ATARIPP_LD "\n"
		   "\tSerInCnt  : " ATARIPP_LD "\tSerOutCnt : " ATARIPP_LD "\tSerXmtCnt : " ATARIPP_LD "\n"
		   "\tSerInBytes: %d\tSerInData : %c%c\n",
		   Unit,
		   Ch[0].AudioF,Ch[1].AudioF,Ch[2].AudioF,Ch[3].AudioF,
		   Ch[0].AudioC,Ch[1].AudioC,Ch[2].AudioC,Ch[3].AudioC,
		   int((Ch[0].DivNIRQ > 0)?(Ch[0].DivNIRQ):(0)),
		   int((Ch[1].DivNIRQ > 0)?(Ch[1].DivNIRQ):(0)),
		   int((Ch[2].DivNIRQ > 0)?(Ch[2].DivNIRQ):(0)),
		   int((Ch[3].DivNIRQ > 0)?(Ch[3].DivNIRQ):(0)),
		   int(Ch[0].DivNMax),int(Ch[1].DivNMax),int(Ch[2].DivNMax),int(Ch[3].DivNMax),
		   AudioCtrl,SkStat   ,SkCtrl,KBCodeRead(),
		   IRQStatRead()      ,IRQEnable,
		   SerIn_Delay,SerOut_Delay,SerXmtDone_Delay,
		   SerIn_Counter,SerOut_Counter,SerXmtDone_Counter,SerInBytes,
		   serin[0],serin[1]
		   );
}
///

/// Pokey::ParseArgs
// Parse arguments for the pokey class
void Pokey::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector palvector[] = 
    { {"Auto"         ,2    },
      {"PAL"          ,0    },
      {"NTSC"         ,1    },
      {NULL           ,0}
    };
  LONG ntsc;
  bool cycle        = CycleTimers;
  bool saprecording = EnableSAP;
  //
  //
  ntsc = NTSC?1:0;
  if (isAuto)
    ntsc = 2;
  args->DefineTitle((Unit)?"ExtraPokey":"Pokey");
  args->DefineLong("Volume","set Pokey volume in percent",0,300,Volume);
  args->DefineLong("Gamma","set Pokey output linearity in percent",50,150,Gamma);
  args->DefineSelection("PokeyTimeBase","set POKEY base frequency",palvector,ntsc);
  args->DefineBool("SIOSound","emulate serial transfer sounds",SIOSound);
  args->DefineBool("CyclePrecise","cycle precise pokey timers",cycle);
  args->DefineLong("FilterConstant","set high-pass filtering constant",0,1024,DCFilterConstant);
  args->DefineBool("RecordAsSAP","record pokey output in a SAP file",saprecording);
  //
  // Potentially re-run the output.
  if (saprecording != EnableSAP)
    args->SignalBigChange(ArgParser::Reparse);
  //
  // Collect additional information in case SAP recording is enabled.
  if (saprecording) {
    args->DefineString("SAPName","name of the SAP song to record",SongName);
    args->DefineString("SAPAuthor","author of the SAP song to record",AuthorName);
  }
  //
  // Enable SAP.
  if (saprecording) {
    EnableSAP = true;
  } else {
    EnableSAP = false;
    if (SAPOutput) {
      fclose(SAPOutput);
      SAPOutput = NULL;
    }
  }
  //
  // Initialize the pokey base clock.
  Frequency17Mhz     = (NTSC)?(1789773):(1773447); 
  //
  // Regenerate the output map.
  UpdateAudioMapping();
  //
  // Update the cycle precise pokey timers.
  if (cycle != CycleTimers) {
    if (cycle) {
      // Enable cycle precise timers.
      machine->CycleChain().AddTail(this);
    } else {
      // Disable cycle precise timers.
      Node<CycleAction>::Remove();
    }
    CycleTimers = cycle;
  }
  //
  // For the auto selection, get the video mode from the machine.
  if (ntsc == 2) {
    ntsc   = machine->isNTSC();
    isAuto = true;
  } else {
    isAuto = false;
  }
  //
  // If the video mode changed, ensure that the sound-subsystem is restarted.
  if (ntsc != LONG(NTSC)) {
    args->SignalBigChange(ArgParser::Reparse);
  }
  // The global sound might have been redefined. Re-read here.
  sound         = machine->Sound();
}
///

/// Pokey::State
// Read or set the internal status
void Pokey::State(class SnapShot *sn)
{
  int i;
  char id[32],helptxt[80];
  struct Channel *ch;
  
  sn->DefineTitle(Unit?"ExtraPokey":"Pokey");
  for(i=0,ch=Ch;i<4;i++,ch++) {
    snprintf(id,31,"Audio%dFreq",i);
    snprintf(helptxt,79,"audio frequency register %d",i);
    sn->DefineLong(id,helptxt,0x00,0xff,ch->AudioF);
    //
    snprintf(id,31,"Audio%dCtrl",i);
    snprintf(helptxt,79,"audio control register %d",i);
    sn->DefineLong(id,helptxt,0x00,0xff,ch->AudioC);
    //
    // Reset all other pokey settings. They aren't too important and will be re-set after an audio update.
    ch->OutBit  = 0;
    ch->HiFlop  = 0;
    ch->DivNCnt = 0;
    ch->DivNIRQ = 0;
  }
  sn->DefineLong("AudioCtrl","global audio control register",0x00,0xff,AudioCtrl);
  // Update all four channels now.
  UpdateSound(0x0f);
  //
  // Update serial port status:
  sn->DefineLong("SkStat","serial port status register",0x00,0xff,SkStat);
  sn->DefineLong("SkCtrl","serial port control register",0x00,0xff,SkCtrl);
  sn->DefineLong("IRQStat","interrupt status register",0x00,0xff,IRQStat);
  sn->DefineLong("IRQEnable","interrupt enable register",0x00,0xff,IRQEnable);
  sn->DefineLong("SerInCnt","serial input IRQ event counter",0,0xffff,SerIn_Counter);
  sn->DefineLong("SerOutCnt","serial output IRQ event counter",0,0xffff,SerOut_Counter);
  sn->DefineLong("SerXmtCnt","serial transmission done IRQ event counter",0,0xffff,SerXmtDone_Counter);
  sn->DefineLong("SerBitOutCnt","serial output bit timer",0,0xffff,SerBitOut_Counter);
  sn->DefineLong("SerOutRegister","hidden serial output register",0,0xffff,SerOut_Register);
  sn->DefineLong("SerOutBuffer","user addressable serial register",0,0xff,SerOut_Buffer);
  sn->DefineLong("SerOutBitCounter","bits in the serial output register",0,16,SerOut_BitCounter);
  //
  // FIXME:
  // Serial input cannot be setup completely. The problem is that we cannot save the serial input
  // queue here. *sigh*  
  SerIn_Counter      = 0;    // no serial IRQ pending
  SerOut_Counter     = 0;
  SerXmtDone_Counter = 0;
  SerInBytes         = 0;
  SerOut_Register    = 0xffff;
  SerOut_Buffer      = 0xff;
  SerBitOut_Counter  = 0;
  SerOut_BitCounter  = 0;
  ConcurrentBusy     = false;
}
///
