/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: digitaljoystick.cpp,v 1.17 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Frontend for the Linux /dev/jsx handler with interface
 ** adapter for Amiga/Atari style joysticks
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "digitaljoystick.hpp"
#include "gamecontroller.hpp"
#include "monitor.hpp"
#include "argparser.hpp"
#include "exceptions.hpp"
#include "timer.hpp"
#include "unistd.hpp"
#if HAVE_FCNTL_H && HAVE_SYS_STAT_H && HAVE_LINUX_JOYSTICK_H && HAS_EAGAIN_DEFINE
extern "C" {
#define USE_JOYSTICK
#include <fcntl.h>
#include <sys/stat.h>
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_LINUX_IOCTL_H
#include <linux/ioctl.h>
#endif
#include <linux/joystick.h>
}
#endif
#include <errno.h>
///

/// DigitalJoystick::DigitalJoystick
DigitalJoystick::DigitalJoystick(class Machine *mach,int id)
  : Chip(mach,"DigitalJoystick"), VBIAction(mach), GamePort(mach,"DigitalJoystick",id), 
    unit(id), stream(-1), enable(true),
    up(false), down(false), left(false), right(false), button(false),
    UpButton(2), DownButton(0), LeftButton(3), RightButton(1),
    TriggerAxis(0), CalibrationAxis(1), TriggerThres(16384), InvertTrigger(false),
    Calibration(0)
{
}
///

/// DigitalJoystick::~DigitalJoystick
DigitalJoystick::~DigitalJoystick(void)
{
#ifdef USE_JOYSTICK
  // cleanup the stream if we have it open
  if (stream >= 0) {
    close(stream);
  }
#endif
}
///

/// DigitalJoystick::IsAvailable
// Check whether the indicated joystick is available. Return true
// if so.
bool DigitalJoystick::IsAvailable(void)
{
#ifdef USE_JOYSTICK
  char name[32];
  struct js_event js_data;
  int status,vers;
  char buttons;
  //
  // If the stream is already open, then the joystick must
  // be available (simple logic)
  if (stream >= 0)
    return true;
  
  sprintf(name,"/dev/js%d",unit);
  stream = open(name,O_RDONLY);
  if (stream < 0) {
    sprintf(name,"/dev/input/js%d",unit);
    stream = open(name,O_RDONLY);
    if (stream < 0) {
      // Did not open. Hence, no joystick is available
      enable = false;
      return false;
    }
  }
  //
  // Check wether we have the new joystick device
  if (ioctl(stream,JSIOCGVERSION,&vers) < 0) {
    close(stream);
    stream = -1;
    machine->PutWarning("Using obsolete joystick device, disabling joystick input.\n");
    enable = false;
    return false;
  }
  //
  // Check for number of buttons. We must have at least four to use this as a 
  // digital input.
  if (ioctl(stream,JSIOCGBUTTONS,&buttons) < 0) {
    enable = false;
  }
  if (buttons < 4) {
    enable = false;
  }
  //
  // Ok, try to read from the joystick now
  status = read(stream, &js_data, sizeof(js_data));
  if (status != sizeof(js_data)) {
    // Oops, did not work?
    enable = false;
    ThrowIo("DigitalJoystick::IsAvailable","Failed to read from the joystick device");
  }

  // that's all for the test
  close(stream);
  stream = -1;
  return enable;
#else
  return false;
#endif
}
///

/// DigitalJoystick::VBI
// Read off the joystick state here
void DigitalJoystick::VBI(class Timer *,bool,bool)
{
#ifdef USE_JOYSTICK
  struct js_event js_data;
  WORD dx,dy;
  // 
  // Do not handle if we are pausing
  // No longer, the menu requires this.
  if (enable /*&& pause == false*/) {
    LONG sum  = 0;
    LONG csum = 0;
    int  cnt  = 0;
    int ccnt  = 0;
    //
    // The stream is not yet open, so open it here.
    if (stream < 0) {
      char name[32];
      sprintf(name,"/dev/js%d",unit);
      stream = open(name,O_RDONLY | O_NONBLOCK);
      if (stream < 0) {
	sprintf(name,"/dev/input/js%d",unit);
	stream = open(name,O_RDONLY | O_NONBLOCK);
	if (stream < 0) {
	  enable = false;
	  ThrowIo("DigitalJoystick::VBI","cannot reopen the digital joystick, disabling it");
	}
      }
      if (stream >= 0) {
	unsigned char axes = 2;
	unsigned char buttons = 2;
	int version = 0x000800;
	char name[256] = "Unknown";
	UWORD btnmap[KEY_MAX - BTN_MISC + 1];
	UBYTE axmap[ABS_MAX + 1];
	ioctl(stream, JSIOCGVERSION, &version);
	ioctl(stream, JSIOCGAXES, &axes);
	ioctl(stream, JSIOCGBUTTONS, &buttons);
	ioctl(stream, JSIOCGNAME(255), name);
	ioctl(stream, JSIOCGAXMAP, axmap);
	ioctl(stream, JSIOCGBTNMAP, btnmap);
	fcntl(stream, F_SETFL, O_NONBLOCK);
      }
    }
    // Now run the read loop to get information how much the joystick changed
    // since last time.
    while (read(stream,&js_data,sizeof(js_data)) == sizeof(js_data)) {
      // Got a new event. Check what it is.
      switch(js_data.type & ~JS_EVENT_INIT) {
      case JS_EVENT_BUTTON:
	// Button state. Only read button #0 for now.
	if (js_data.number == UpButton) {
	  up    = js_data.value;
	} else if (js_data.number == DownButton) {
	  down  = js_data.value;
	} else if (js_data.number == LeftButton) {
	  left  = js_data.value;
	} else if (js_data.number == RightButton) {
	  right = js_data.value;
	}
	break;
      case JS_EVENT_AXIS:
	// Axis movement. We only read the trigger axis.
	if (js_data.number == TriggerAxis) {
	  // Try to integrate spurious errors out.
	  //printf("%d\n",js_data.value);
	  sum  += js_data.value;
	  cnt++;
	} else if (js_data.number == CalibrationAxis) {
	  csum += js_data.value;
	  ccnt++;
	}
	break;
      }
    }
    // Check for the cause of the fault. Might be something serious
    if (errno != EAGAIN) {
      enable = false;
      ThrowIo("DigitalJoystick::VBI","cannot read from the analog joystick");
    }
    //
    // Now generate dx and dy from the data.
    if (left) {
      dx = -32767;
    } else if (right) {
      dx = +32767;
    } else {
      dx = 0;
    }
    if (up) {
      dy = -32767;
    } else if (down) {
      dy = +32767;
    } else {
      dy = 0;
    }
    //
    if (ccnt) {
      Calibration = csum / ccnt;
    }
    //
    // Generate button data from the button integration.
    if (cnt) {
      sum = sum/cnt - Calibration;
      //
      if (InvertTrigger) {
	button = (sum > TriggerThres)?(false):(true);
      } else {
	button = (sum > TriggerThres)?(true):(false);
      }
    }
  } else {
    // On pausing, hold all joysticks.
    dx = 0, dy = 0;
    button = false;
  }
  //
  // Now feed all joystics on this port with the new and updated data.
  FeedAnalog(dx,dy);
  FeedButton(button);
#endif
}
///

/// DigitalJoystick::ColdStart
// Cleanup the joystick by shutting it down.
// This forces a re-open of the joystick devices
void DigitalJoystick::ColdStart(void)
{
#ifdef USE_JOYSTICK
  if (stream >= 0) {
    close(stream);
    stream = -1;
  }
#endif
}
///

/// DigitalJoystick::WarmStart
// Warmstart the joystick, does nothing.
void DigitalJoystick::WarmStart(void)
{
}
///

/// DigitalJoystick::ParseArgs
// Parse arguments for the joystick devices
void DigitalJoystick::ParseArgs(class ArgParser *args)
{  
#ifdef USE_JOYSTICK
  static const struct ArgParser::SelectionVector buttonvector[] = 
    { {"Button.1",0},
      {"Button.2",1},
      {"Button.3",2},
      {"Button.4",3},
      {NULL ,0}
    };  
  static const struct ArgParser::SelectionVector axisvector[] = 
    { {"XAxis.1" ,0},
      {"YAxis.1" ,1}, 
      {"XAxis.2" ,2},
      {"YAxis.2" ,3},
      {NULL      ,0}
    };
  char name[32];
  char upbuttonname[32];
  char downbuttonname[32];
  char leftbuttonname[32];
  char rightbuttonname[32];
  char triggeraxisname[32];
  char calibrationaxisname[32];
  char triggerthresname[32];
  char invertname[32];

  sprintf(name,"DigitalJoystick.%d",unit);
  sprintf(upbuttonname,"UpButton.%d",unit);
  sprintf(downbuttonname,"DownButton.%d",unit);
  sprintf(leftbuttonname,"LeftButton.%d",unit);
  sprintf(rightbuttonname,"RightButton.%d",unit);
  sprintf(triggeraxisname,"TriggerAxis.%d",unit);
  sprintf(calibrationaxisname,"CalibrationAxis.%d",unit);
  sprintf(triggerthresname,"TriggerThres.%d",unit);
  sprintf(invertname,"InvertTrigger.%d",unit);

  args->DefineTitle(name);
  args->DefineSelection(upbuttonname,
			"set the button input line for upwards movement",
			buttonvector,UpButton);
  args->DefineSelection(downbuttonname,
			"set the button input line for downwards movement",
			buttonvector,DownButton);
  args->DefineSelection(leftbuttonname,
			"set the button input line for leftwards movement",
			buttonvector,LeftButton);
  args->DefineSelection(rightbuttonname,
			"set the button input line for rightwards movement",
			buttonvector,RightButton);
  args->DefineSelection(triggeraxisname,
			"set the input axis for the trigger input",
			axisvector,TriggerAxis);
  args->DefineSelection(calibrationaxisname,
			"set the input axis for the calibration input",
			axisvector,CalibrationAxis);
  args->DefineLong(triggerthresname,"set the button press/release threshold",
		   -32768,32768,TriggerThres);
  args->DefineBool(invertname,"invert the trigger input",InvertTrigger);
#endif
}
///

/// DigitalJoystick::DisplayStatus
// Display the status of the joystick device
void DigitalJoystick::DisplayStatus(class Monitor *mon)
{
#ifdef USE_JOYSTICK
  mon->PrintStatus("Joystick #%d status:\n"
		   "\tJoystick available: %s\n"
		   "\tUp button line    : " LD "\n"
		   "\tDown button line  : " LD "\n"
		   "\tLeft button line  : " LD "\n"
		   "\tRight button line : " LD "\n"
		   "\tTrigger input axis: %s\n"
		   "\tCalibration axis  : %s\n"
		   "\tTrigger threshold : " LD "\n"
		   "\tInvert trigger    : %s\n",
		   unit,
		   IsAvailable()?("yes"):("no"),
		   UpButton+1,DownButton+1,LeftButton+1,RightButton+1,
		   (TriggerAxis)?("YAxis"):("XAxis"),
		   (CalibrationAxis)?("YAxis"):("XAxis"),
		   TriggerThres,
		   (InvertTrigger)?("yes"):("no")
		   );
#else
   mon->PrintStatus("Joystick #%d status:\n"
		    "\tJoystick not compiled in\n",
		    unit);
#endif
}
///
