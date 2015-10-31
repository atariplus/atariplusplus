/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gameport.cpp,v 1.12 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Interface between the gamecontroller and various
 **                 input modules that possibly feed joysticks/paddle/lightpen
 **********************************************************************************/

/// Includes
#include "gameport.hpp"
#include "machine.hpp"
#include "gamecontrollernode.hpp"
#include "exceptions.hpp"
#include "string.hpp"
#include "stdlib.hpp"
///

/// GamePort::GamePort
GamePort::GamePort(class Machine *mach,const char *name,int unit)
  : Name(name), Unit(unit)
{
  mach->GamePortChain().AddHead(this);
}
///

/// GamePort::~GamePort
GamePort::~GamePort(void)
{
  class GameControllerNode *node;
  //
  Remove();
  // Now also remove all nodes from our input list
  while((node = InputList.First())) {
    // Note that this call will remove nodes from the list such
    // that we have to check on each call once again.
    node->Link(NULL);
  }
}
///

/// GamePort::FindPort
// Given the first entry of the list, check for a game port of a given name
// and unit number.
class GamePort *GamePort::FindPort(const char *name,int unit)
{
  class GamePort *that = this;

  while(that) {
    if (!strcmp(name,that->Name) && unit==that->Unit)
      return that;
    that = that->NextOf();
  }

  return NULL;
}
///

/// GamePort::FindPort
// Given just an identifier in the format "name.unit", find the gameport
// of that identifier.
class GamePort *GamePort::FindPort(const char *name)
{
  char *c,buf[64];
  int unit;

  if (strlen(name) > 63) {
    Throw(OutOfRange,"GamePort::FindPort","Desired GamePort name too LONG");
  }
  strcpy(buf,name);
  if ((c = strchr(buf,'.'))) {
    char *last;
    *c = '\0'; // Terminate the name here.
    unit = strtol(c+1,&last,10);
    if (*last != 0) {
      Throw(InvalidParameter,"GamePort::FindPort","Game Port unit number is invalid");
    }
  } else {
    unit = 0;
  }
  //
  // And now use the regular call
  return FindPort(buf,unit);
}
///

/// GamePort::FeedAnalog
// Feed an analog input in the range -32767..32767 for both directions. This
// this for analog joystics.
void GamePort::FeedAnalog(WORD x,WORD y)
{
  class GameControllerNode *ctrl = InputList.First();

  while(ctrl) {
    ctrl->FeedAnalog(x,y);
    ctrl = ctrl->NextOf();
  }
}
///

/// GamePort::FeedButton
// Feed button input into all controllers that are handled by this port
void GamePort::FeedButton(bool value,int button)
{
  class GameControllerNode *ctrl = InputList.First();
  
  while(ctrl) {
    ctrl->FeedButton(value,button);
    ctrl = ctrl->NextOf();
  }
}
///
