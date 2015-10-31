/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gamecontroller.cpp,v 1.32 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of the interface towards game controller inputs
 **                 This is the Atari side of the game controller input.
 **                 GamePorts leave their data here, and emulator components
 **                 read the provided data from here.
 **********************************************************************************/


/// Includes
#include "gamecontroller.hpp"
///

/// GameController::GameController
GameController::GameController(class Machine *mach,int unit,const char *name,bool ispaddle)
  : Configurable(mach), GameControllerNode(mach,unit,name,ispaddle)
{
}
///

/// GameController::~GameController
GameController::~GameController(void)
{
}
///
