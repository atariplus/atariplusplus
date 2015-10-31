/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: configurable.hpp,v 1.7 2015/05/21 18:52:37 thor Exp $
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
