/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: digitaljoystick.hpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Frontend for the Linux /dev/jsx handler with interface
 ** adapter for Amiga/Atari style joysticks
 **********************************************************************************/

#ifndef DIGITALJOYSTICK_HPP
#define DIGITALJOYSTICK_HPP

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

/// Class Digitaloystick
// This class is the input frontend for the Linux analogue joystick
// device, /dev/js0 and related, but expects an additional input
// adapter to connect a digital joystick to the port. This interface
// uses the buttons 0..3 as direction input and one of the analog
// lines as button input.
class DigitalJoystick : public Chip, public VBIAction, public GamePort {
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
  bool up,down,left,right;
  bool button;
  //
  // Joystick Preferences:
  //
  // Various button numbers to emulate the directions.
  LONG UpButton,DownButton,LeftButton,RightButton;
  //
  // The axis that is responsible for the trigger input
  LONG TriggerAxis;
  //
  // The axis that performs the calibration.
  LONG CalibrationAxis;
  //
  // The threshold above which the trigger is read as pressed
  LONG TriggerThres;
  //
  // If this boolean is set, then the trigger is read in negative
  // logic.
  bool InvertTrigger;
  //
  // Current calibration value.
  LONG Calibration;
  //
  // Frequent activities: Poll the joystick device
  virtual void VBI(class Timer *time,bool quick,bool pause);
  //
  //
public:
  DigitalJoystick(class Machine *mach,int unit);
  ~DigitalJoystick(void);
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

    
