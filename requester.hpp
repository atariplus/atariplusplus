/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: requester.hpp,v 1.8 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: A generic requester class
 **********************************************************************************/

#ifndef REQUESTER_HPP
#define REQUESTER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "event.hpp"
#include "gadgetgroup.hpp"
///

/// Forwards
class Machine;
class AtariDisplay;
class RenderPort;
class Gadget;
class EventFeeder;
///

/// Class Requester
// This class describes the base type for all sorts of requesters
// we might come up with. It is used to enforce a user reaction in
// case of a system fault.
class Requester {
  //
  // The machine of this requester: The main class
  class Machine          *Machine;
  //
  // The render port for creating the graphics in
  class RenderPort       *RPort;
  //
  // The gatherer of input events.
  class EventFeeder      *EventFeeder;
  //
  // The list of all gadgets within here.
  List<class Gadget>      GList;
  //
  // The top-level gadget that contains all other
  // gadgets: This works similar to the gadgetgroup
  // except that its main event callback calls the user
  // requester callback.
  class RequesterGadget : public GadgetGroup {
    //
    class Requester      *Container;
    //
  public:
    RequesterGadget(List<class Gadget> &GList,class RenderPort *rport,
		    LONG le,LONG te,LONG w,LONG h,
		    class Requester *top);
    virtual ~RequesterGadget(void);
    //
    // Perform action if the gadget was hit, resp. release the gadget.
    virtual bool HitTest(struct Event &ev);
  }                      *TopLevel;
  //
  // The following virtual class is here to install additional gadgets.
  // This hook must be provided by all implementors of requesters.
  virtual void BuildGadgets(List<Gadget> &glist,class RenderPort *rport)   = 0;  
  virtual void CleanupGadgets(void);
  //
  // Event handling callback. This is called after processing and filtering
  // an event by all the gadgets. It should return an integer of either
  // RQ_Nothing (continue processing) or an int larger than RQ_Comeback to
  // signal requester abortion.
  //
  virtual int HandleEvent(struct Event &event) = 0;
  //
  // Build the requester and all the gadgets.
  // This internal method prepares the graphics
  // for the requester. Returns false if the requester
  // could not be build up.
  bool BuildUp(void);
  //
  // Shut down the requester (without deleting it entirely)
  void ShutDown(void);
  //  
protected: 
  //
  enum RequesterAction {
    RQ_Nothing    = 0,
    RQ_Comeback,       // These two are pre-defined by the event feeder class.
    // all others are return types for the individual requester implementations.
    RQ_Abort           // A typical definition.
  };
  //
  // Build up the requester and come back again with a picked Id or whatever.
  int Request(void);
  //
  // Return an indicator whether this requester is head-less, i.e. has no GUI.
  bool isHeadLess(void) const;
  //
  // Make the GUI visible or invisible.
  void SwitchGUI(bool foreground) const;
  //
  class Machine *MachineOf() const
  {
    return Machine;
  }
  //
public:
  // Build a constructor. This requires a machine. Maybe not a display, though
  Requester(class Machine *mach);
  virtual ~Requester(void);
  //
};
///

///
#endif
