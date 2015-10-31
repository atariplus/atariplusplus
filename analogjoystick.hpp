/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: analogjoystick.hpp,v 1.10 2015/05/21 18:52:35 thor Exp $
 **
 ** In this module: Frontend for the Linux /dev/jsx handler
 **********************************************************************************/

#ifndef ANALOGJOYSTICK_HPP
#define ANALOGJOYSTICK_HPP

/// Includes
#include "types.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "gameport.hpp"
///

/// Forward declarations
class Monitor;
class ArgParser;
///

/// Class AnalogJoystick
// This class is the input frontend for the Linux analogue joystick
// device, /dev/js0 and related.
class AnalogJoystick : public Chip, public VBIAction, public GamePort {
  //
  // The unit number of the device. This is the last letter of
  // the joystick class.
  int unit;
  //
  // The stream that links to the device (hopefully)
  int stream;
  //
  // Set to TRUE if this joystick works.
  bool enable;
  //
  // Current movement and axis position.
  WORD dx,dy;
  bool button[4];
  //
  // Joystick Preferences:
  //
  // The number of the button that should trigger the joystick
  // button.
  LONG ButtonId[4];
  //
  // The axis positions for the joystick device, horizontal and
  // vertical axis
  LONG HAxis,VAxis;
  //
  //
  // Frequent activities: Poll the joystick device
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  //
public:
  AnalogJoystick(class Machine *mach,int unit);
  ~AnalogJoystick(void);
  //
  // 
  // Check whether the joystick is available and if so, open it.
  bool IsAvailable(void);
  //
  // Implement the interfaces of the chip class.
  //
  // Cold and warmstart activity.
  virtual void WarmStart();
  virtual void ColdStart();
  //
  // Parse off arguments for this class.
  virtual void ParseArgs(class ArgParser *args);
  //
  // Display the status of this joystick class
  virtual void DisplayStatus(class Monitor *mon);
  //
};
///

///
#endif

    
