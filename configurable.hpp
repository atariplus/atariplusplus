/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: configurable.hpp,v 1.5 2002-09-25 21:40:50 thor Exp $
 **
 ** In this module: Configurable module(s)
 **********************************************************************************/

#ifndef CONFIGURABLE_HPP
#define CONFIGURABLE_HPP

/// Includes
#include "argparser.hpp"
#include "list.hpp"
///

/// Forwards
class Machine;
///

/// Class Configurable
// This is an element that is configurable
class Configurable : public Node<class Configurable> {
public:
  Configurable(class Machine *mach);
  virtual ~Configurable(void);
  //
  // Parse the arguments off
  virtual void ParseArgs(class ArgParser *args) = 0;
};
///

///
#endif
