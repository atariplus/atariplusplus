/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gameport.hpp,v 1.9 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Interface between the gamecontroller and various
 **                 input modules that possibly feed joysticks/paddle/lightpen
 **********************************************************************************/

#ifndef GAMEPORT_HPP
#define GAMEPORT_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
///

/// Forwards
class GameControllerNode;
class Machine;
///

/// Class GamePort
// This class defines an interface all possible joystick event generators
// must derive; it connects the GameController input class with the
// implementation generating the movement events.
class GamePort : public Node<class GamePort> {
  //
  // Every class generating input must have a name that is given here.
  const char               *Name;
  // and a unit number to make this unique
  const int                 Unit;
  //
  // A game port might (unlike reality) feed several game controller
  // inputs. Here's the list of game controllers we feed.
  List<GameControllerNode>  InputList;
  //
protected:
  // We cannot destroy this class directly without a base class.
  ~GamePort(void);
  //
public:
  GamePort(class Machine *mach,const char *name,int unit);
  //
  //
  // Get a pointer to the first game controller we feed by this chain
  List<GameControllerNode> &ControllerChain(void)
  {
    return InputList;
  }
  //
  // Given the first entry of the list, check for a game port of a given name
  // and unit number.
  class GamePort *FindPort(const char *name,int unit);
  //
  // Given just an identifier in the format "name.unit", find the gameport
  // of that identifier.
  class GamePort *FindPort(const char *name);
  //
  // Input feeder methods: These go into the game controller chain of this
  // game port
  // Feed an analog input in the range -32767..32767 for both directions. This
  // this for analog joystics.
  void FeedAnalog(WORD x,WORD y);
  void FeedButton(bool value,int button = 0);
  //
  // Overload the next/prev functions to allow iteration within the
  // list of game ports.
  class GamePort *NextOf(void) const
  {
    return Node<GamePort>::NextOf();
  }
  class GamePort *PrevOf(void) const
  {
    return Node<GamePort>::PrevOf();
  }
  //
  // Return the name of this port
  const char *NameOf(void) const
  {
    return Name;
  }
  // Return the unit number of this port
  int UnitOf(void) const
  {
    return Unit;
  }
};
///

///
#endif
