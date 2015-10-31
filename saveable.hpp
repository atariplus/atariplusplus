/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: saveable.hpp,v 1.4 2015/05/21 18:52:42 thor Exp $
 **
 ** This class defines the interface for reading and writing snapshots of 
 ** configurations.
 **********************************************************************************/

#ifndef SAVEABLE_HPP
#define SAVEABLE_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
///

/// Forward Declarations
class SnapShot;
class Machine;
///

/// The Saveable class
// This class defines an object a snapshot can be taken from
// and into which a snapshot can be installed into. 
// Hence, this is part of a "status saver" that allows to
// re-play games from a snapshot file.
class Saveable : public Node<class Saveable> {
  //
  // Name of this saveable: This is the name by which this
  // saveable happens to appear in the shapshot
  const char *SaveName;
  //
  // Optionally, a unit number can be given identifying this
  // saveable amongst a group of similar objects.
  int         SaveUnit;
  //
protected:
  // This is only an interface definition class without
  // a function, and hence purely virtual.
  //
  Saveable(class Machine *mach,const char *name,int unit = 0);
  virtual ~Saveable(void);
  //
public:
  //
  // Read and write the state into a snapshot class
  // This is the main purpose why we are here. This method
  // should read the state from a snapshot, providing the current
  // settings as defaults. Hence, we can also use it to save the
  // configuration by putting the defaults into a file.
  //
  // Concering the philosphy: The snapshot should *not* include
  // settings that concern the interface, i.e. data that is read
  // thru the standard "configurable" interface. Rather, it should
  // only save data concering the internal state of the machine.
  virtual void State(class SnapShot *snap) = 0;
  //
  // Return the name of the saveable
  const char *NameOf(void) const
  {
    return SaveName;
  }
  //
  // Return the unit of the saveable
  int UnitOf(void) const
  {
    return SaveUnit;
  }
  //
  // Saveables are queued in a list. To resolve any ambiguities when
  // handling these lists, implement manually a next/prev-function
  // for these objects.
  class Saveable *NextOf(void) const
  {
    return Node<Saveable>::NextOf();
  }
  class Saveable *PrevOf(void) const
  {
    return Node<Saveable>::PrevOf();
  }
};
///

///
#endif

  
