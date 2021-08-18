/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: analogjoystick.cpp,v 1.29 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Frontend for the Linux /dev/jsx handler
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "chip.hpp"
#include "vbiaction.hpp"
#include "analogjoystick.hpp"
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

/// AnalogJoystick::AnalogJoystick
AnalogJoystick::AnalogJoystick(class Machine *mach,int id)
  : Chip(mach,"AnalogJoystick"), VBIAction(mach), GamePort(mach,"AnalogJoystick",id), 
    unit(id), stream(-1), enable(true), dx(0), dy(0),
    HAxis(0), VAxis(1)
{
  int i;
  for(i = 0;i < 4;i++) {
    button[i]   = false;
    ButtonId[i] = i;
  }
}
///

/// AnalogJoystick::~AnalogJoystick
AnalogJoystick::~AnalogJoystick(void)
{
#ifdef USE_JOYSTICK
  // cleanup the stream if we have it open
  if (stream >= 0) {
    close(stream);
  }
#endif
}
///

/// AnalogJoystick::IsAvailable
// Check whether the indicated joystick is available. Return true
// if so.
bool AnalogJoystick::IsAvailable(void)
{
#ifdef USE_JOYSTICK
  char name[32];
  struct js_event js_data;
  int status,vers;
  //
  // If the stream is already open, then the joystick must
  // be available (simple logic)
  if (stream >= 0)
    return true;

  sprintf(name,"/dev/js%d",unit);
  stream = open(name,O_RDONLY);
  if (stream < 0) {
    // Did not open. Next try, use the input system.
    sprintf(name,"/dev/input/js%d",unit);
    stream = open(name,O_RDONLY);
    if (stream < 0) {
      // Hence, no joystick is available
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
  // Ok, try to read from the joystick now
  status = read(stream, &js_data, sizeof(js_data));
  if (status != sizeof(js_data)) {
    // Oops, did not work?
    enable = false;
    ThrowIo("AnalogJoystick::IsAvailable","Failed to read from the joystick device");
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

/// AnalogJoystick::VBI
// Read off the joystick state here
void AnalogJoystick::VBI(class Timer *,bool,bool)
{
#ifdef USE_JOYSTICK
  struct js_event js_data;
  int i;
  // 
  // Do not handle if we are pausing
  // No longer, the menu requires this.
  if (enable /*&& pause == false*/) {
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
	  ThrowIo("AnalogJoystick::VBI","cannot reopen the analog joystick, disabling it");
	}
      }
    }
    // Now run the read loop to get information how much the joystick changed
    // since last time.
    while (read(stream,&js_data,sizeof(js_data)) == sizeof(js_data)) {
      // Got a new event. Check what it is.
      switch(js_data.type & ~JS_EVENT_INIT) {
      case JS_EVENT_BUTTON:
	// Button state. Only read button #0 for now.
	for(i = 0;i < 4;i++) {
	  if (js_data.number == ButtonId[i])
	    button[i] = js_data.value;
	}
	break;
      case JS_EVENT_AXIS:
	// Axis movement. We only read axis #0 and #1.
	if (js_data.number == HAxis) {
	  dx     = js_data.value;
	} else if (js_data.number == VAxis) {
	  dy     = js_data.value;
	}
	break;
      }
    }
    // Check for the cause of the fault. Might be something serious
    if (errno != EAGAIN) {
      ThrowIo("AnalogJoystick::VBI","cannot read from the analog joystick");
      enable = false;
    }
  } else {
    // On pausing, hold all joysticks.
    dx = 0, dy = 0;
    for(i = 0;i < 4;i++)
      button[i] = false;
  }
  //
  // Now feed all joystics on this port with the new and updated data.
  FeedAnalog(dx,dy);
  for(i = 0; i < 4; i++)
    FeedButton(button[i],i);
#endif
}
///

/// AnalogJoystick::ColdStart
// Cleanup the joystick by shutting it down.
// This forces a re-open of the joystick devices
void AnalogJoystick::ColdStart(void)
{
#ifdef USE_JOYSTICK
  if (stream >= 0) {
    close(stream);
    stream = -1;
  }
#endif
}
///

/// AnalogJoystick::WarmStart
// Warmstart the joystick, does nothing.
void AnalogJoystick::WarmStart(void)
{
}
///

/// AnalogJoystick::ParseArgs
// Parse arguments for the joystick devices
void AnalogJoystick::ParseArgs(class ArgParser *args)
{
#ifdef USE_JOYSTICK
  static const struct ArgParser::SelectionVector axisvector[] = 
    { {"XAxis.1" ,0},
      {"YAxis.1" ,1},
      {"XAxis.2" ,2},
      {"YAxis.2" ,3},
      {NULL      ,0}
    };
  char name[32];
  char buttonname1[32];
  char buttonname2[32];
  char buttonname3[32];
  char buttonname4[32];
  char haxisname[32];
  char vaxisname[32];
  LONG button[4];
  int i;

  for(i = 0; i < 4;i++)
    button[i] = ButtonId[i] + 1;

  sprintf(name,"AnalogJoystick.%d",unit);
  sprintf(buttonname1,"First_Button.%d",unit);
  sprintf(buttonname2,"Second_Button.%d",unit);
  sprintf(buttonname3,"Third_Button.%d",unit);
  sprintf(buttonname4,"Forth_Button.%d",unit);
  sprintf(haxisname,"HAxis.%d",unit);
  sprintf(vaxisname,"VAxis.%d",unit);

  args->DefineTitle(name);
  args->DefineLong(buttonname1,"set the first joystick input button",1,16,button[0]);
  args->DefineLong(buttonname2,"set the second joystick input button",1,16,button[1]);
  args->DefineLong(buttonname3,"set the third joystick input button",1,16,button[2]);
  args->DefineLong(buttonname4,"set the forth joystick input button",1,16,button[3]);
  args->DefineSelection(haxisname,"set the horizontal joystick axis",
			axisvector,HAxis);
  args->DefineSelection(vaxisname,"set the vertical joystick axis",
			axisvector,VAxis);

  for(i = 0;i < 4;i++)
    ButtonId[i] = button[i] - 1;
#endif
}
///

/// AnalogJoystick::DisplayStatus
// Display the status of the joystick device
void AnalogJoystick::DisplayStatus(class Monitor *mon)
{
#ifdef USE_JOYSTICK
  mon->PrintStatus("Joystick #%d status:\n"
		   "\tJoystick available     : %s\n"
		   "\tFirst Polled button  # : " ATARIPP_LD "\n"
		   "\tSecond Polled button # : " ATARIPP_LD "\n"
		   "\tThird Polled button  # : " ATARIPP_LD "\n"
		   "\tFourth Polled button # : " ATARIPP_LD "\n"
		   "\tHorizontal Axis      # : " ATARIPP_LD "\n"
		   "\tVertical Axis        # : " ATARIPP_LD "\n",
		   unit,
		   IsAvailable()?("yes"):("no"),
		   ButtonId[0],ButtonId[1],ButtonId[2],ButtonId[3],
		   HAxis,VAxis);
#else
  mon->PrintStatus("Joystick #%d status:\n"
		   "\tJoystick not compiled in.\n",
		   unit);
#endif
}
///
