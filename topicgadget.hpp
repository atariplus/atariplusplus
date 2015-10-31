/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: topicgadget.hpp,v 1.6 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a meta-gadget that holds all items of
 ** one topic, i.e. one "configurable" element.
 **********************************************************************************/

#ifndef TOPICGADGET_HPP
#define TOPICGADGET_HPP

/// Includes
#include "verticalgroup.hpp"
#include "filegadget.hpp"
#include "menutopic.hpp"
#include "optioncollector.hpp"
///

/// Class TopicGadget
// A gadget presenting a list of topics taken from the list. This gadget
// is already pretty much specialized for our application
class TopicGadget : public VerticalGroup {
  //
  // A flag that gets set as soon as one of/the gadget in the optiongadget
  // list has the focus.
  bool                                OptionFocus;
  //
  // The list of user defined topics in here.
  List<Topic>                        *Topics;
  //
  // The option gadget group gets its own gadget list here so
  // we can clean it up more easely
  List<Gadget>                        OptionGadget;
  //
  // The list of gadgets within the requester. This is constructed on demand.
  List<Gadget>                        Requester;
  //
  // The currently active topic.
  class Topic                        *ActiveTopic;
  //
  // The gadget that is requesting the requester. The resulting path of the requester
  // is inserted back into this gadget.
  class FileGadget                   *RequestingGadget;
  //
public:
  TopicGadget(List<Gadget> &gadgetlist,
	      class RenderPort *rp,LONG w,List<Topic> *tlist);
  virtual ~TopicGadget(void);
  //
  virtual bool HitTest(struct Event &ev);
  //
  virtual void Refresh(void);
  //
  // Re-install the option list on a outside-change of the options
  void ReInstallOptions(void);
  //
  // Allocate the name of the currently active topic. This is kept
  // amongst resets of the machine.
  // topic and option are the positions of the topic resp. option
  // scrollers.
  void ActiveTopicName(char *&topicname,UWORD &topic,UWORD &option);
  //
  // Scroll to the indicated topic name.
  void ScrollToName(const char *topicname,UWORD topic,UWORD option);
  //
  // Returns an boolean whether any topic is currently active.
  bool HaveActiveTopic(void) const
  {
    return ActiveTopic?true:false;
  }
  //
  // Check for the nearest gadget in the given direction dx,dy,
  // return this (or a sub-gadget) if a suitable candidate has been
  // found, alter x and y to a position inside the gadget, return NULL
  // if this gadget is not in the direction.
  virtual const class Gadget *FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const;
};
///

///
#endif
