/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: gamecontrollernode.hpp,v 1.5 2015/05/21 18:52:39 thor Exp $
 **
 ** In this module: Definition of the interface towards game controller inputs
 **                 This defines the class that keeps and collects the input from
 **                 the outside and hence mimincs an instance within the emulated
 **                 machine. It is complemented by the GamePort class which mimics
 **                 the outside counterpart. Sorry for the bad naming...
 **********************************************************************************/

#ifndef GAMECONTROLLERNODE_HPP
#define GAMECONTROLLERNODE_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "configurable.hpp"
#include "gameport.hpp"
#include "argparser.hpp"
///

/// Forwards
class Machine;
///

/// Class GameControllerNode
// This is the generic interface class for game controllers
// of all kind: Paddles, Joysticks and lightpens.
class GameControllerNode : public Node<class GameControllerNode> {
  //
protected:
  // Pointer back to the controlling machine
  class Machine                     *machine;
private:
  //
  // The game port we are linked into. This link is defined here
  // to unlink us quickly and easily to move from one port to
  // another.
  class GamePort                    *Port;
  char                              *PortName;
  //
  // Temporary array containing the possible game ports we get input from
  struct ArgParser::SelectionVector *PossiblePorts;
  //
  // From the list of game ports, build the above selection vector to simplify
  // the option parsing process.
  void BuildPortVector(void);
  //
  // Dispose the list of possible ports together with their names
  void DisposePortList(void);
  //
protected:
  //
  // For pairs of paddles or joysticks: This is the unit number
  // of the game controller.
  int         Unit;
  //
  // For Joysticks: Responsiveness: Units outside this range
  // are identified as movements
  LONG        Responseness;
  //
  // Defines whether this is an analog (paddle-like) device.
  // If so, then the buttons are mapped thru Stick.
  bool        IsPaddle;
  //
  // If this is a paddle input, then the paddle direction can be inverted
  // by means of the following flag
  bool        InvertPaddle;
  //
  // The current position of the game controller
  WORD        position[2];
  //
  // The current position of the button(s)
  bool        button[2];
  //
  // Name of this joystick: We need this for the configuration
  char       *DeviceName;
  //
  // In case of paddles: Which axis of the delivering hardware device is
  // used to generate the signal?
  LONG        Axis;
  //
public:
  GameControllerNode(class Machine *mach,int unit,const char *name,bool ispaddle);
  virtual ~GameControllerNode(void);
  //
  // Read the position of a joystick and return the Atari-typical
  // bitmask.
  UBYTE Stick(void);
  //
  // Return the button state of the joystick, true when pressed
  bool Strig(void);
  //
  // Return the position of a paddle
  UBYTE Paddle(void);
  //
  // Return the Lightpen X coordinate
  UBYTE LightPenX(void);
  //
  // Return the Lightpen Y coordinate
  UBYTE LightPenY(void);
  //
  // GTIA service: Turn on button R/S flipflop that
  // keeps the last button press
  void StoreButtonPress(bool on);
  //
  // Implementation of the configurable interface
  void ParseArgs(class ArgParser *args);
  //
  // The following are the input methods to be called from the user interface
  // and feed data into the class.
  //
  // Feed an analog input in the range -32767..32767 for both directions. This
  // this for analog joystics.
  virtual void FeedAnalog(WORD x,WORD y);
  virtual void FeedButton(bool value,int button = 0);
  //
  // Link into a list of controllers feed by a game port
  // or unlink if port is NULL.
  void Link(class GamePort *port);
};
///

///
#endif
    
