/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: pokey.cpp,v 1.103 2008-11-24 21:24:41 thor Exp $
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
  : Chip(mach,(unit)?"ExtraPokey":"Pokey"), Saveable(mach,(unit)?"ExtraPokey":"Pokey"), HBIAction(mach), IRQSource(mach),
    Outcnt(0), PolyCounter17(new UBYTE[Poly17Size]), SampleCnt(0), OutputMapping(new BYTE[256]),
    Unit(unit)
{
  int n;
  //
  // Initialize the random generator now
#ifdef HAVE_TIME
  srand((unsigned int)(time(NULL)));
#endif
  //
  // Install default gamma, volume here.
  // Sublinear mapping, full volume
  Gamma  = 70;
  Volume = 100;  
  // Initialize the DC level shift.
  DCLevelShift     = 128;
  DCAverage        = 0;
  DCFilterConstant = 512;
 
  UpdateAudioMapping();
  //
  // Fill the 17 bit poly counter with random data. That is not quite
  // the original, but shouldn't make any noticable difference.
  for(n=0;n<Poly17Size;n++) {
    if (rand() > (RAND_MAX >> 1)) {
      PolyCounter17[n] = 0x0f;
    } else {
      PolyCounter17[n] = 0x00;
    }
  }
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
  Poly9Ptr             = PolyCounter17;
  Poly17Ptr            = PolyCounter17;
  PolyNoneEnd          = PolyCounterNone + 1;
  Poly4End             = Poly4Ptr  + Poly4Size;
  Poly5End             = Poly5Ptr  + Poly5Size;
  Poly9End             = Poly9Ptr  + Poly9Size;
  Poly17End            = Poly17Ptr + Poly17Size;
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
  // Slow pot read.
  PotNInc          = 1;
  // PAL operation
  NTSC             = false;
  SIOSound         = true;
  Outcnt           = 0;
  //
  // Initialize serial timing, will be overridden
  // by SkCtrl writes
  SerIn_Delay      = 9;
  SerOut_Delay     = 9;
  SerXmtDone_Delay = 9;
  //
  // Initialize pointers to be on the safe side.
  sound            = NULL;
  keyboard         = NULL;
  sio              = NULL;
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
  SkStat             = 0xf0; // no pending errors
  SkCtrl             = 0x00;
  SerInBytes         = 0x00; // reset serial port
  //
  //
  // Initialize poly counter registers
  PolyNonePtr          = PolyCounterNone;
  Poly4Ptr             = PolyCounter4;
  Poly5Ptr             = PolyCounter5;
  Poly9Ptr             = PolyCounter17;
  Poly17Ptr            = PolyCounter17;
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
  PotNInc            = 1;
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
  // Release the poly counter buffers: Only the 17 bit polycounter is hand-
  // allocated.
  delete[] PolyCounter17;
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
	  ch->DivFullMax = 255        + 7; // full wraparound required.
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
      audc = ch->AudioC;
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
      SerIn_Delay      = (20 * Ch[3].DivNMax + Base15kHz - 1) / Base15kHz;
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
      SerOut_Delay     = (20 * Ch[3].DivNMax + Base15kHz - 1) / Base15kHz;
      SerXmtDone_Delay = SerOut_Delay;
    }
    break;
  case 0x60:
    // Output clocked by channel 1 (or 0 and 1)
    if (mask & 0x03) { 
      SerOut_Delay     = (20 * Ch[1].DivNMax + Base15kHz - 1) / Base15kHz;
      SerXmtDone_Delay = SerOut_Delay;
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
      if (SkCtrl & 0x08) {
	// Two-tone modulation turned on. Here, only the higher
	// of the channels 0,1 survives, unless channel 1 is lower
	// channel 0 in which case bit 7 (and the serial output data)
	// controls which channel has to be sent.
	if (Ch[0].DivNMax > Ch[1].DivNMax) {
	  // Channel 0 has the lower tone. Block channel 0.
	  Ch[0].ChannelOn = false;
	} else {
	  // Otherwise, channel 1 has the lower tone, but
	  // which channel survives here depends on the serial
	  // output. We do not yet emulate the serial shift
	  // register, but we can check for a serial break,
	  // generated by SkCtrl, at least.
	  if (SkCtrl & 0x80) {
	    // Send a serial break. Let channel 1 pass
	    Ch[0].ChannelOn = false;
	  } else {
	    // Keep serial output at high, send channel
	    // zero.
	    // FIXME: We would need to test the serial
	    // shift register here instead if the
	    // serial mode control is 32 or 64
	    Ch[1].ChannelOn = false;
	  }
	}
      } else if ((SkCtrl & 0x60) == 0x60) {
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
	    // FIXME: Ugly hack to get the sound 
	    // approximately right.
	    // See above for how it really works.
	    Ch[3].DivNMax  *= 21;
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
      current += mask & c->AudioC;
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
	Poly4Ptr     = ((Poly4Ptr - PolyCounter4)   % Poly4Size ) + PolyCounter4;
      }
      if (Poly5Ptr  >= Poly5End) {
	Poly5Ptr     = ((Poly5Ptr - PolyCounter5)   % Poly5Size ) + PolyCounter5;
      }
      if (Poly9Ptr  >= Poly9End) {
	Poly9Ptr     = ((Poly9Ptr - PolyCounter17)  % Poly9Size ) + PolyCounter17;
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
      audc = c->AudioC >> 5;
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

/// Pokey::HBI
// Signal Pokey that a new scanline just
// begun. This is not related to any real effect. 
// In fact, Pokey doesn't care about scanlines in
// reality at all. All we do here is to drive Pokey
// by a 15Khz (= one horizontal blank) frequency to
// emulate the timer and serial port specific settings.
void Pokey::HBI(void)
{
  struct Channel *c;
  int ch;
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
      SerIn_Counter   = 1;    // generate an interrupt below.
      SerInBytes      = 1;    // have one byte in the queue now
      ConcurrentBusy  = true; // do not try to deliver this until the atari collects it
    }
  }
  // Check for serial input done.
  if (SerIn_Counter > 0) {
    if (--SerIn_Counter == 0) {  
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
  // Check for serial output register empty.
  if (SerOut_Counter > 0) {
    if (--SerOut_Counter == 0) {
      // Serial output register empty and free to
      // be reloaded. Generate an interrupt by bit 4.
      GenerateIRQ(0x10);
      //
      // SerOut is now empty. Generate XMTDone when completely
      // done.
      SerXmtDone_Counter = SerXmtDone_Delay;
    }
  }

  // Check whether the serial output register just
  // became empty. If so, generate an IRQ. Note
  // that we actually do not drive bit 3 of IRQStat
  // from here as this bit is unlatched (hardware manual)
  if (SerXmtDone_Counter > 0) {
    if (--SerXmtDone_Counter == 0) {
      GenerateIRQ(0x08);
    }
  }

  // Now check for Pokey timers. Actually, we won't
  // need to emulate them very precisely. Or not?
  for(ch = 0, c = Ch;ch < 4;ch++,c++) {
    static const UBYTE irqbits[4] = {0x01,0x02,0x00,0x04}; 
    // Yes, channel 3 cannot generate interrupts at all...
    if ((c->DivNIRQ += Base15kHz) > c->DivNMax) {
      // Generate an IRQ now (or not for channel 3)
      if (IRQEnable & irqbits[ch]) {
	GenerateIRQ(irqbits[ch]);
      }	
      //c->DivNIRQ = 0; Allow accumulation of errors on average.
      c->DivNIRQ  -= c->DivNMax;
    }
  }

  //
  // Now check keyboard input if we are connected to
  // the keyboard
  if (keyboard && (SkCtrl & 2)) {
    if (IRQEnable & 0x80) {
      if (keyboard->BreakInterrupt()) {
	GenerateIRQ(0x80);
      }
    }
    if (IRQEnable & 0x40) {
      if (keyboard->KeyboardInterrupt()) {
	GenerateIRQ(0x40);
      }
    }
  }
  //
  //
  // Now update the pot reader.
  for(ch = 0;ch < 8;ch++) {
    int updated;
    // Bump the counter by the counter increment. If we
    // reach the maximum (or beyond), then stop.
    updated = PotNCnt[ch] + PotNInc;
    if (updated >= PotNMax[ch]) {
      // Read is ready, signal in allPot by clearing the
      // apropriate bit, and insert the proper value.
      AllPot      &= ~(1<<ch);
      PotNCnt[ch]  = PotNMax[ch];
    } else {
      PotNCnt[ch]  = updated;
    }
  }
  //
  // Shuffle the random generator a bit.
  rand();
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
  case 0x0b:
  case 0x0c:
    // Unused registers.
    return 0;
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
  // Shut up the compiler
  return 0xff;
}
///    

/// Pokey::ComplexWrite
// Emulate a pokey byte write to a given
// address.
bool Pokey::ComplexWrite(ADR mem,UBYTE val)
{
  switch(mem & 0x0f) {
  case 0x00:
  case 0x02:
  case 0x04:
  case 0x06:
    // Set an audio frequency write now
    AudioFWrite((mem & 0x0f) >> 1,val);
    return false;
  case 0x01:
  case 0x03:
  case 0x05:
  case 0x07:
    // Set an audio control register here.
    AudioCWrite((mem & 0x0f) >> 1,val);
    return false;
  case 0x08:
    AudioCtrlWrite(val);
    return false;
  case 0x09:
    STimerWrite();
    return false;
  case 0x0a:
    SkStatClear();
    return false;
  case 0x0b:
    PotGoWrite();
    return false;
  case 0x0c:
    // This does nothing
    return false;
  case 0x0d:
    // Serial output register put
    SerOutWrite(val);
    return false;
  case 0x0e:
    // IRQ Enable definition
    IRQEnWrite(val);
    return false;
  case 0x0f:
    // Serial Control Register Write
    SkCtrlWrite(val);
    return false;
  }
  // Shut up the compiler
  return false;
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
  // Never use the lower-order bits.
  // This here should be a bit better since
  // it has at least a period of 2^16
  if (RAND_MAX >= 65536) {
    return (rand() >> 8);
  } else {
    return (rand() >> 4);
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
	  UBYTE byte = *SerInBuffer++;
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
  
  if (SkCtrl & 2) 
    out |= keyboard->KeyboardStatus();
  
  if (SerIn_Counter == 0) // Serial input register is currently busy?
    out |= 0x02; // no, it's not.

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
    Ch[ch].DivNCnt = Ch[ch].DivNIRQ = 0;
  }
  //
  // Also resets the polycounters 
  PolyNonePtr          = PolyCounterNone;
  Poly4Ptr             = PolyCounter4;
  Poly5Ptr             = PolyCounter5;  
  Poly9Ptr             = PolyCounter17;
  Poly17Ptr            = PolyCounter17;
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
  if (sio) {
    // Note that we have to modify this here
    // in case we have to emulate Happys
    if ((SkCtrl & 0xf0) == 0x20) {    // Transfer mode fine?
      if ((AudioCtrl & 0x28) == 0x28) { // Timer setup fine? Connect channels 3,4
	sio->WriteByte(val);
	// Signal the serial out and serial done IRQ
	SerOut_Counter     = SerOut_Delay;  
	// Serial port cannot go empty as we just refilled it.
	SerXmtDone_Counter = 0;
	if (SIOSound)
	  UpdateSound(0x0c);
	return;
      } 
    } else if ((SkCtrl & 0xf0) == 0x70) { 
      // This is the concurrent mode transfer setting.
      if ((AudioCtrl & 0x28) == 0x28) {
	// Transmit the byte thru the concurrent channel
	sio->ConcurrentWrite(val);
	// Wait until the byte has been written. We perform this
	// relatively fast here.
	SerOut_Counter     = 1;
	SerXmtDone_Counter = 0;
	return;
      }
    }
    // Note: We may otherwise still trigger a serial done here.
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
    int ch;
    //
    // Audio reset?
    for(ch=0;ch < 4;ch++) {
      Ch[ch].DivNCnt = Ch[ch].DivNIRQ = 0;
    }
    //
    // Also resets the polycounters 
    PolyNonePtr          = PolyCounterNone;
    Poly4Ptr             = PolyCounter4;
    Poly5Ptr             = PolyCounter5;  
    Poly9Ptr             = PolyCounter17;
    Poly17Ptr            = PolyCounter17;
    // Channel 0,1 output is set to low, Channel 2,3 output
    // is set to high. Strange enough, but the manual says so.
    Ch[0].OutBit = Ch[1].OutBit = 0x00;
    Ch[2].OutBit = Ch[3].OutBit = 0x0f;
    //
    // Ensure that the sound gets updated.
    SkCtrl = ~val;
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
  //
  // Check how fast we should update the counters. We
  // set the counter increment value depending on whether
  // fast or slow pot reading is set. Maybe this is not quite
  // correct since we cannot remove the loading capacitors
  // in the real emulator. Need to check this.
  PotNInc = (SkCtrl & 0x04)?(114):(1);
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
void Pokey::SignalSerialBytes(UBYTE *data,int num,UWORD delay)
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
  SerIn_Counter  = delay + SerIn_Delay;
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

/// Pokey::DisplayStatus
// Print the status of the chip over the monitor
void Pokey::DisplayStatus(class Monitor *mon)
{
   mon->PrintStatus("Pokey.%d Status:\n"
		    "\tAudioFreq0: %02x\tAudioFreq1: %02x\tAudioFreq2: %02x\tAudioFreq3: %02x\n"
		    "\tAudioCtrl0: %02x\tAudioCtrl1: %02x\tAudioCtrl2: %02x\tAudioCtrl3: %02x\n"
		    "\tAudioCtrl : %02x\tSkStat    : %02x\tSkCtrl    : %02x\tKeyCode   : %02x\n"
		    "\tIRQStat   : %02x\tIRQEnable : %02x\n"
		    "\tSerInDly  : " LD "\tSerOutDly : " LD "\tSerXmtDly : " LD "\n"
		    "\tSerInCnt  : " LD "\tSerOutCnt : " LD "\tSerXmtCnt : " LD "\n"
		    "\tSerInBytes: %d\n",
		    Unit,
		    Ch[0].AudioF,Ch[1].AudioF,Ch[2].AudioF,Ch[3].AudioF,
		    Ch[0].AudioC,Ch[1].AudioC,Ch[2].AudioC,Ch[3].AudioC,
		    AudioCtrl,SkStat   ,SkCtrl,KBCodeRead(),
		    IRQStat  ,IRQEnable,
		    SerIn_Delay,SerOut_Delay,SerXmtDone_Delay,
		    SerIn_Counter,SerOut_Counter,SerXmtDone_Counter,SerInBytes
		    );
}
///

/// Pokey::ParseArgs
// Parse arguments for the pokey class
void Pokey::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector palvector[] = 
    { {"PAL"          ,false},
      {"NTSC"         ,true },
      {NULL           ,0}
    };
  LONG ntsc;
  //
  //
  ntsc = LONG(NTSC);
  args->DefineTitle((Unit)?"ExtraPokey":"Pokey");
  args->DefineLong("Volume","set Pokey volume in percent",0,300,Volume);
  args->DefineLong("Gamma","set Pokey output linearity in percent",50,150,Gamma);
  args->DefineSelection("VideoMode","set POKEY base frequency",palvector,ntsc);
  args->DefineBool("SIOSound","emulate serial transfer sounds",SIOSound);
  args->DefineLong("FilterConstant","set high-pass filtering constant",0,1024,DCFilterConstant);
  //
  // Initialize the pokey base clock.
  Frequency17Mhz     = (NTSC)?(1789773):(1773447); 
  //
  // Regenerate the output map.
  UpdateAudioMapping();
  //
  // If the video mode changed, ensure that the sound-subsystem is restarted.
  if (ntsc != LONG(NTSC)) {
    NTSC = (ntsc)?(true):(false);
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
  // FIXME:
  // Serial input cannot be setup completely. The problem is that we cannot save the serial input
  // queue here. *sigh*  
  SerIn_Counter      = 0;    // no serial IRQ pending
  SerOut_Counter     = 0;
  SerXmtDone_Counter = 0;
  SerInBytes         = 0;
  ConcurrentBusy     = false;
}
///
