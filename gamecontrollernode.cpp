/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gamecontrollernode.cpp,v 1.7 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of the interface towards game controller inputs
 **                 This is the Atari side of the game controller input.
 **                 GamePorts leave their data here, and emulator components
 **                 read the provided data from here.
 **********************************************************************************/

/// Includes
#include "gamecontrollernode.hpp"
#include "exceptions.hpp"
#include "machine.hpp"
#include "gameport.hpp"
#include "argparser.hpp"
#include "new.hpp"
#include "string.hpp"
#include "stdio.hpp"
///

/// GameControllerNode::GameControllerNode
// This class is virtual and cannot be constructed or destructed
// except by subclasses.
GameControllerNode::GameControllerNode(class Machine *mach,int unit,const char *name,
			       bool ispaddle = false)
  : machine(mach), Port(NULL), PortName(NULL),  
    PossiblePorts(NULL),
    Unit(unit), Responseness(8192), IsPaddle(ispaddle), InvertPaddle(false),
    DeviceName(new char[strlen(name)+1])
{ 
  strcpy(DeviceName,name);
  position[0] = 0;
  position[1] = 0;
  button[0]   = false;
  button[1]   = false;
  Axis        = Unit & 0x01;
  
  if (Unit == 0 && !IsPaddle && strcmp("Lightpen",name)) {
    // FIX: Do not connect the paddle as it will also feed joystick input
    // Set the default device to the keypad stick
    PortName = new char[strlen("KeypadStick.0") + 1];
    strcpy(PortName,"KeypadStick.0");
  }
}
///

/// GameControllerNode::~GameControllerNode
GameControllerNode::~GameControllerNode(void)
{
  // Unlink us from the port should be linked in somewhere
  Link(NULL);
  delete[] PortName;
  delete[] DeviceName;
  DisposePortList();
}
///

/// GameControllerNode::Link
// Link into a list of controllers: This adds us to an input
// supply given by the port.
void GameControllerNode::Link(class GamePort *port)
{
  // Check whether we're part of another controller chain already. If so,
  // we'd better unlink ourselves here.
  if (Port) {
    // Need to specify the list we want to remove us from. This is not the
    // configchain, of course.
    Node<GameControllerNode>::Remove();
    Port = NULL;
    position[0] = 0;
    position[1] = 0;
    button[0]   = false;
    button[1]   = false;
  }
  //
  //
  if (port) {
    // Now link us into the port
    port->ControllerChain().AddHead(this);
  }
  Port = port;
}
///

/// GameControllerNode::StoreButtonPress
// GTIA service: Turn on button R/S flipflop that
// keeps the last button press
void GameControllerNode::StoreButtonPress(bool)
{
  // FIXME: This is currently not implemented
  // (but who needs it anyhow...)
}
///

/// GameControllerNode::Stick
// Read the position of a joystick and return the Atari-typical
// bitmask.
UBYTE GameControllerNode::Stick(void)
{
  UBYTE bitmask = 0;
  if (IsPaddle) {
    // Works differently if this is a paddle. We read then buttons
    if (button[0]) bitmask |= 0x01;
    if (button[1]) bitmask |= 0x02;
  } else {
    int x,y;
    // Read the coordinates now.
    x = position[0];
    y = position[1];
    //
    if (y <= -Responseness) bitmask |= 0x01; // up
    if (y >=  Responseness) bitmask |= 0x02; // down
    if (x <= -Responseness) bitmask |= 0x04; // left
    if (x >=  Responseness) bitmask |= 0x08; // right
  }
  return UBYTE(~bitmask & 0x0f);
}
///

/// GameControllerNode::Strig
// Return the button state of the joystick, true when pressed
bool GameControllerNode::Strig(void)
{
  if (IsPaddle) {
    return button[Axis];
  }
  return button[0];
}
///

/// GameControllerNode::Paddle
// Return the position of a paddle
UBYTE GameControllerNode::Paddle(void)
{
  int pot;
  // Depends on the unit now
  pot = position[Axis];
  // If the paddle direction should be inverted, check here.
  if (InvertPaddle)
    pot = -pot;
  //
  // Scale now linearly between the limits.
  pot = 114 + ((pot * 114)/Responseness);
  // Interestingly, some games mis-react if we read here the pure maximum, 228.
  // This happens for the 5200 emulation mode in vanguard.
  if (pot < 1)   pot = 1;
  if (pot > 227) pot = 227;
  return UBYTE(pot);
}
///

/// GameControllerNode::LightPenX
// Return the Lightpen X coordinate
UBYTE GameControllerNode::LightPenX(void)
{
  int lpx;
  lpx = position[0];
  // Scale the light pen position
  lpx = 97 + (lpx * 194) / (Responseness * 2);
  if (lpx < 61)  lpx = 61;
  if (lpx > 255) lpx = 255;
  //printf("x: %d -> %d\n",position[0],lpx);
  return UBYTE(lpx);
}
///

/// GameControllerNode::LightPenY
// Return the Lightpen Y coordinate
UBYTE GameControllerNode::LightPenY(void)
{
  int lpy;
  lpy = position[1];  
  lpy = 62 + (lpy * 124) / (Responseness * 2);
  if (lpy < 0)   lpy = 0;
  if (lpy > 124) lpy = 124;
  //printf("y: %d -> %d\n",position[1],lpy);
  return UBYTE(lpy);
}
///

/// GameControllerNode::FeedAnalog
// Feed an analog input in the range -32768..32767 for both directions. This
// this for analog joystics
void GameControllerNode::FeedAnalog(WORD x,WORD y)
{
  position[0] = x;
  position[1] = y;
}
///

/// GameControllerNode::FeedButton
// Feed the button state (on,off)
void GameControllerNode::FeedButton(bool value, int number)
{
  if (number >= 0 && number <= 1)
    button[number] = value;
}
///

/// GameControllerNode::DisposePortList
// Dispose the list of possible ports together with their names
void GameControllerNode::DisposePortList(void)
{
  struct ArgParser::SelectionVector *sel;
  //
  if ((sel = PossiblePorts)) {
    while(sel->Name) {
      delete[] TOCHAR(sel->Name);
      sel->Name = NULL;
      sel++;
    }
    delete[] PossiblePorts;
    PossiblePorts = NULL;
  }
}
///

/// GameControllerNode::BuildPortVector
// From the list of game ports, build the above selection vector to simplify
// the option parsing process.
void GameControllerNode::BuildPortVector(void)
{
  struct ArgParser::SelectionVector *sel;
  class GamePort *port;
  char *newname;
  LONG count = 2; // "none" is also a selection.
  LONG id    = 0;
  
  DisposePortList();
  //
  // First count the number of possible ports.
  for(port = machine->GamePortChain().First();port;port = port->NextOf()) {
    count++;
  }
  //
  // Now build up the list.
  PossiblePorts = sel = new struct ArgParser::SelectionVector[count];
  //
  // And fill in all the names. 
  for(port = machine->GamePortChain().First();port;port = port->NextOf()) {
    size_t namesize;
    const char *name = port->NameOf();
    namesize   = strlen(name)+1+1+5; // enough room for name, dot and unit
    newname    = new char[namesize];
    sel->Name  = newname;
    sel->Value = id;
    snprintf(newname,namesize,"%s.%d",name,port->UnitOf());
    sel++;
    id++;
  }
  //
  // Build up the final dummy entry 
  newname    = new char[strlen("None") + 1];
  sel->Name  = newname;
  sel->Value = id;
  strcpy(newname,"None");
  sel++;
  id++;
  sel->Name  = NULL;
  sel->Value = 0;
}
///

/// GameControllerNode::ParseArgs
// Implementation of the configurable interface
void GameControllerNode::ParseArgs(class ArgParser *args)
{
  class GamePort *port = NULL;
  char optionname[80];
  char portname[80];
  char invertname[80];
  char axisname[80];
  LONG id;
  struct ArgParser::SelectionVector *sel;
  static const struct ArgParser::SelectionVector axisvector[] = 
    { {"Horizontal"    ,0 },
      {"Vertical"      ,1 },
      {NULL ,0}
    };     

  // Build up the list of game ports we have.
  BuildPortVector();
  sel = PossiblePorts;
  if (Unit == 0) {
    char *unit;
    strcpy(portname,DeviceName);
    // Remove the trailing unit separator for the
    // target name.
    unit = strrchr(portname,'.');
    if (unit)
      *unit = 0;
    args->DefineTitle(portname);
  }
  snprintf(optionname,80,"%s.Sensitivity",DeviceName);
  snprintf(portname,80,"%s.Port",DeviceName);
  snprintf(invertname,80,"%s.Invert",DeviceName);
  snprintf(axisname,80,"%s.InputAxis",DeviceName);
  args->DefineLong(optionname,"set the game controller sensitivity",
		   0,32767,Responseness);
  // Find the id of the currently active port within the build up array of
  // possible selections.
  id = 0;
  while(sel->Name) {
    if (PortName && (!strcmp(sel->Name,PortName))) break;
    sel++;
    id++;
  }
  if (sel->Name == NULL) {
    // Found no matching entry->set to "none"
    id--;
  }  
  if (IsPaddle) {
    args->DefineSelection(axisname,"paddle input axis",axisvector,Axis);
  }
  args->DefineSelection(portname,"set the game controller input device",PossiblePorts,
			id);
  if (IsPaddle) {
    args->DefineBool(invertname,"invert paddle input",InvertPaddle);
  }
  //
  // Now check whether we have a useful port name here. If it is "none", then
  // ignore.    
  delete[] PortName;
  PortName = NULL;
  if (strcasecmp(PossiblePorts[id].Name,"None")) {
    const char *src = PossiblePorts[id].Name;
    // Copy the name over, it is non-trivial.
    PortName = new char[strlen(src) + 1];
    strcpy(PortName,src);
    Link(NULL);
  }
  if (PortName) {
    // Check whether we have a game port for it to attach it to.
    port = machine->GamePortChain().First()->FindPort(PortName);
    if (port) {
      Link(port);
    } else {
      args->PrintError("%s argument %s invalid: Input device does not exist.\n",
		       portname,PortName);
      Throw(ObjectDoesntExist,"GameControllerNode::ParseArgs",
	    "selected input device does not exist");
    }
  } else {
    // Unlink us otherwise and leave the device unconnected.
    Link(NULL);
  }
}
///
