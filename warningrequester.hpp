/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: warningrequester.hpp,v 1.6 2015/05/21 18:52:44 thor Exp $
 **
 ** In this module: A requester class that prints and logs warnings.
 **********************************************************************************/

#ifndef WARNINGREQUESTER_HPP
#define WARNINGREQUESTER_HPP

/// Includes
#include "list.hpp"
#include "requester.hpp"
#include "event.hpp"
#include "listbrowsergadget.hpp"
///

/// Forwards
class ButtonGadget;
class TextGadget;
class Machine;
///

/// Class WarningRequester
// This requester prints and logs warnings
class WarningRequester : public Requester {
  //
  enum WarningAction {
    WRQ_Nothing   = 0,                 // requester build-up failed
    WRQ_Cancel    = RQ_Abort,          // terminate the program
    WRQ_Menu,                          // enter the menu
    WRQ_Retry                          // retry the run
  };
  // The following structure keeps recent warnings
  // as a text list. We must use the TextNode as base
  // class to use the list browser requester.
  struct Warning : public TextNode {
    //
    // Pointer to the text forming the warning.
    const char *Warn;
    //
    Warning(const char *msg);
    virtual ~Warning(void);
    //
    // Get the text node.
    virtual const char *Text(void) const
    {
      return Warn;
    }
  };
  //
  List<struct TextNode> WarnLog;
  //
  // A couple of gadgets in here. (More to come)
  //
  class TextGadget     *Headline;
  class ButtonGadget   *OKGadget; 
  class ButtonGadget   *MenuGadget;
  //
  // Gadget build up and destroy routines.
  virtual void BuildGadgets(List<Gadget> &glist,class RenderPort *rport);
  virtual void CleanupGadgets(void);
  //
  // Event handling callback.
  virtual int HandleEvent(struct Event &event);
  //
public:
  // Form a warning requester. Requires only the machine class.
  WarningRequester(class Machine *mach);
  ~WarningRequester(void);
  //
  // Run the requester with the warning text. If this returns zero, then
  // the requester could not be build for whatever reason and the message
  // must be made available in another way.
  int Request(const char *msg);
};
///

///
#endif
