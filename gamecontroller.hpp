/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gamecontroller.hpp,v 1.14 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of the interface towards game controller inputs
 **                 This defines the class that keeps and collects the input from
 **                 the outside and hence mimincs an instance within the emulated
 **                 machine. It is complemented by the GamePort class which mimics
 **                 the outside counterpart. Sorry for the bad naming...
 **********************************************************************************/

#ifndef GAMECONTROLLER_HPP
#define GAMECONTROLLER_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "configurable.hpp"
#include "gamecontrollernode.hpp"
#include "gameport.hpp"
#include "argparser.hpp"
///

/// Forwards
class Machine;
///

/// Class GameController
// This is the generic interface class for game controllers
// of all kind: Paddles, Joysticks and lightpens.
class GameController : public Configurable, public GameControllerNode {
  //
public:
  GameController(class Machine *mach,int unit,const char *name,bool ispaddle);
  ~GameController(void);
  //
  // Implementation of the configurable interface
  virtual void ParseArgs(class ArgParser *args)
  {
    GameControllerNode::ParseArgs(args);
  }
};
///

///
#endif
    
