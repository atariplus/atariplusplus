/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: choicerequester.hpp,v 1.4 2015/05/21 18:52:37 thor Exp $
 **
 ** In this module: A requester that allows the user to pick multiple choices
 **********************************************************************************/

#ifndef CHOICEREQUESTER_HPP
#define CHOICEREQUESTER_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "gadget.hpp"
#include "requester.hpp"
#include "listbrowsergadget.hpp"
///

/// Forwards
class ButtonGadget;
class TextGadget;
class RenderPort;
///

/// Class ChoiceRequester
// A requester that allows the user to pick
// one of several options on the command line.
class ChoiceRequester : public Requester {
  //
  // The pointer to the body text that appears
  // in the topmost listview gadget.
  const char              *BodyText;
  //
  // The gadget containing the body text.
  class ListBrowserGadget *BodyGadget;
  //
  // The following additional list keeps the gadgets
  // the user could possibly pick.
  struct GadgetNode : public Node<struct GadgetNode> {
    class ButtonGadget    *Button;
    const char            *GadgetText;
    int                    Id;
    //
    // Build up a gadget node from a text. Do not build a gadget
    // yet in the constructor.
    GadgetNode(const char *text,int id);
    // No destructor. No need to destroy anything: The gadget
    // gets destructed by the superclass, and the text isn't ours.
    //
    // Build-up a gadget from the text.
    void BuildUpGadget(List<class Gadget> &glist,class RenderPort *rp,LONG le,LONG te,LONG w,LONG h);
  };
  //
  // The list of available options is here
  List<struct GadgetNode>  GadgetList;
  //  
  // A structure required for passing the text into the listbrowser.
  // Line breaking is done in the list browser automatically.
  struct TextContents : public TextNode {
    // The text to display here.
    const char *Contents;
    //
    // Deliver the text to the listbrowser. This is a one-time
    // list required for build-up of the listbrowser.
    virtual const char *Text(void) const
    {
      return Contents;
    }
    //
    // Build-up of the gadget.
    TextContents(const char *txt)
      : Contents(txt)
    { }
  };
  //
  // The following virtual class is here to install additional gadgets.
  // This hook must be provided by all implementors of requesters.
  virtual void BuildGadgets(List<class Gadget> &glist,class RenderPort *rport);
  virtual void CleanupGadgets(void);
  //
  // Event handling callback. This is called after processing and filtering
  // an event by all the gadgets. It should return an integer of either
  // RQ_Nothing (continue processing) or an int larger than RQ_Comeback to
  // signal requester abortion.
  virtual int HandleEvent(struct Event &event);
  //
public:
  //
  // Build a constructor. This requires a machine. 
  ChoiceRequester(class Machine *mach);
  virtual ~ChoiceRequester(void);
  //
  // Build up a choice requester. This requires a body text, and a list of
  // choices terminated by a NULL to pick from.
  int Request(const char *body,...);
  //
};
///

///
#endif

