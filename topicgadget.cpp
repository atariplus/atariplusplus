/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: topicgadget.cpp,v 1.11 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of a meta-gadget that holds all items of
 ** one topic, i.e. one "configurable" element.
 **********************************************************************************/

/// Includes
#include "topicgadget.hpp"
#include "buttongadget.hpp"
#include "filelist.hpp"
#include "new.hpp"
///

/// TopicGadget::TopicGadget
TopicGadget::TopicGadget(List<Gadget> &gadgetlist,
			 class RenderPort *rp,LONG w,List<Topic> *tlist)
  : VerticalGroup(gadgetlist,rp,0,0,w,rp->HeightOf()),
    OptionFocus(false), Topics(tlist), ActiveTopic(NULL), RequestingGadget(NULL)
{
  class Topic *topic;
  LONG y=0;
  
  // Add all button gadgets to the gadget group. Note that the gadget
  // group is responsible for destruction which is at this point already constructed
  // successfully. Hence, even if we throw here, the gadgets get disposed.
  for(topic = Topics->First();topic;topic = topic->NextOf()) {
    class Gadget *gadget;
    gadget = new class ButtonGadget(*this,RPort,0,y,w - 12,12,topic->NameOf());
    gadget->UserPointerOf() = topic;
    y += 12;
  }
}
///

/// TopicGadget::~TopicGadget
TopicGadget::~TopicGadget(void)
{
  class Gadget *next;
  // The gadget group will clean up for us the list we
  // got from the vertical group. Now clean up the internal option gadget list
  
  // Gadgets remove themselves from the list when deleted, hence do not 
  // unlink us here.
  while((next = OptionGadget.First())) {
    delete next;
  }
  // Cleanup the requester as well.
  while((next = Requester.First())) {
    delete next;
  }
}
///

/// TopicGadget::Refresh
// Refresh the topic gadget by first refreshing the vertical
// gadget and then refreshing the option gadget.
void TopicGadget::Refresh(void)
{
  class Gadget *gadget;
  
  // If the requester is active, hide the gadgets below and just show the
  // requester contents.
  if (Requester.IsEmpty()) {
    VerticalGroup::Refresh();
  } else {
    for(gadget = Requester.First();gadget;gadget=gadget->NextOf()) {
      gadget->Refresh();
    }
  }
  for(gadget = OptionGadget.First();gadget;gadget=gadget->NextOf()) {
    gadget->Refresh();
  }
}
///

/// TopicGadget::HitTest
// Perform the event handling for the topic gadget
// This also calls the handler of the corresponding topic and
// returns the boolean result code of the event handler of
// the corresponding topic.
bool TopicGadget::HitTest(struct Event &ev)
{  
  // get the first gadget and iterate until its over: We only have two
  // gadgets here: The toplevel gadget topic selector and (possibly) the
  // topic specific vertical gadget group administrating the gadgets under
  // the topic.
  if (OptionFocus == false) {
    // This is the toplevel gadget.
    if ((VerticalGroup::HitTest(ev))) {
      switch(ev.Type) {
      case Event::GadgetUp:
	{
	  class Gadget *gadget = ev.Object;
	  ActiveGadget = NULL;
	  if (gadget) {
	    // Got the gadget. Now get its topic.
	    class Topic *topic = (class Topic *)gadget->UserPointerOf();
	    if (topic) {
	      ActiveTopic = topic;
	      ReInstallOptions();
	      // We do not return this.
	      ev.Object = NULL;
	      return false;
	    }
	  }
	}
	break;
      case Event::Request:
	ActiveGadget = NULL;
	// runs into the following
      default:
	ev.Object    = NULL;
	return false;
      }
    }
  }
  //
  // Check now the right-hand side option gadgets for an action.
  if (Requester.IsEmpty()) {  
    class Gadget *optiongroup;
    optiongroup = OptionGadget.First();
    if (optiongroup) {
      if ((optiongroup->HitTest(ev))) {
	switch(ev.Type) {
	case Event::GadgetDown:
	  // Return the active gadget event now; we return the gadget
	  // getting active, not ourselfs.
	  OptionFocus  = true;
	  return false;
	case Event::GadgetUp:
	  OptionFocus  = false;
	  if (ActiveTopic) {
	    return ActiveTopic->HandleEvent(ev);
	  } else {
	    return false;
	  }
	case Event::Request:
	  {
	    class FileList *fl;
	    LONG le,te,w,h;
	    // Build up a requester here to get a file name. This requester overlays the gadget group
	    // until it is satisfied. We also keep the gadget that performed the request to insert
	    // the result of the request later on.
	    w  = ((RPort->WidthOf() - Width) & (~7)) - 4;
	    h  = Height;
	    le = RPort->WidthOf() - w;
	    te = LeftEdge;
	    //
	    RPort->CleanBox(Width,0,RPort->WidthOf() - Width,RPort->HeightOf(),0x08);
	    RequestingGadget = (class FileGadget *)ev.Object;
	    fl               = new class FileList(Requester,RPort,le,te,w,h,
						  RequestingGadget->GetStatus(),
						  (ev.ControlId & 1)?true:false,
						  (ev.ControlId & 2)?true:false,
						  (ev.ControlId & 4)?true:false);
	    fl->Refresh();
	  }
	  break;
	default: // shut up the compiler
	  break;
	}
      }
    }
  } else {
    class Gadget *requester,*gadget;
    requester = Requester.First(); 
    // Exists since we checked above
    if (requester->HitTest(ev)) {
      // Perform requester action. There's really only one we could care about, and this is when the
      // requester gadget goes up again.
      switch(ev.Type) {
      case Event::GadgetDown:
	OptionFocus = true;
	return false;
      case Event::GadgetUp:
	OptionFocus = false;
	// Ok, get the status of the requester, insert it back into the requesting gadget
	// and dispose the requester and its contents. However, we only do this if we get an
	// "ok" from the requester. For that, we check whether ev.Object exists.
	if (ev.Object) {
	  // Check whether we got an ok or a cancel. On Ok, re-install the contents.
	  if (ev.Button) {
	    RequestingGadget->SetContents(((class FileList *)ev.Object)->GetStatus());
	  }
	  // Now dispose the file list ("the requester") and restore/reactivate the other gadgets
	  while ((gadget = Requester.First())) {
	    delete gadget;
	  }
	  // Forward this event as a "gadgetup" of the requesting gadget to forward
	  // the changes to the front-end.
	  ev.Object        = RequestingGadget;
	  RequestingGadget = NULL;
	  // Refresh our contents to make the changes visible:
	  // Clean up old mess
	  RPort->CleanBox(Width,0,RPort->WidthOf() - Width,RPort->HeightOf(),0x08);
	  // Refresh us.
	  Refresh();
	  // Forward the result: Hit or not.	  
	  if (ev.Button) {
	    if (ActiveTopic) {
	      return ActiveTopic->HandleEvent(ev);
	    }
	  }
	  return false;
	default:
	  // shut up the compiler
	  break;
	}
      }
    }
  }
  return false;
}
///

/// TopicGadget::ReInstallOptions
// Re-install the option list on a outside-change of the options
void TopicGadget::ReInstallOptions(void)
{
  LONG width,leftedge;
  class Gadget *gadget;
  // Got a topic. We need to dispose the last topic group and need to attach
  // a new one.
  leftedge = Width;
  width    = RPort->WidthOf() - leftedge;
  while ((gadget = OptionGadget.First())) {
    delete gadget;
  }
  // Also dispose the requester.
  while ((gadget = Requester.First())) {
    delete gadget;
  }
  RequestingGadget = NULL;
  //
  RPort->CleanBox(leftedge,0,width,RPort->HeightOf(),0x08);
  if (ActiveTopic)
    ActiveTopic->CreateOptionGadgets(OptionGadget);
}
///

/// TopicGadget::ActiveTopicName
// Allocate the name of the currently active topic. This is kept
// amongst resets of the machine.
void TopicGadget::ActiveTopicName(char *&topicname,UWORD &topic,UWORD &option)
{
  const char *name = NULL;
  delete[] topicname;
  topicname = NULL;
  //
  // Now allocate the name of the new gadget
  if (ActiveTopic) {
    class VerticalGroup *optiongadget;
    name = ActiveTopic->NameOf();
    topicname = new char[strlen(name) + 1];
    strcpy(topicname,name);
    topic        = VerticalGroup::GetScroll();	
    optiongadget = (class VerticalGroup *)OptionGadget.First();
    if (optiongadget) {
      option     = optiongadget->GetScroll();
    }
  }
}
///

/// TopicGadget::ScrollToName
// Scroll to the indicated topic name.
void TopicGadget::ScrollToName(const char *topicname,UWORD topicp,UWORD optionp)
{
  class Topic *topic;

  if (topicname) {
    // Iterate thru the list of the topics until we find the
    // indicated topic.
    for(topic = Topics->First();topic;topic = topic->NextOf()) {
      if (!strcmp(topic->NameOf(),topicname)) {
	class VerticalGroup *optiongadget;
	ActiveTopic = topic;
	ReInstallOptions();
	VerticalGroup::Refresh(); // Enforce recomputation of the layout
	VerticalGroup::ScrollTo(topicp);
	optiongadget = (class VerticalGroup *)OptionGadget.First();
	if (optiongadget) {
	  optiongadget->ScrollTo(optionp);
	}
	break;
      }
    }
  }
}
///

/// TopicGadget::FindGadgetInDirection
// Check for the nearest gadget in the given direction dx,dy,
// return this (or a sub-gadget) if a suitable candidate has been
// found, alter x and y to a position inside the gadget, return NULL
// if this gadget is not in the direction.
const class Gadget *TopicGadget::FindGadgetInDirection(LONG &x,LONG &y,WORD dx,WORD dy) const
{
  LONG mx = x,my = y;
  LONG xmin = x,ymin = y;
  const class Gadget *f,*gad = NULL;
  LONG dist = 0;
  LONG mdist;
  //
  mx = x,my = y;
  f = VerticalGroup::FindGadgetInDirection(mx,my,dx,dy);
  if (f) {
    dist  = (mx - x) * (mx - x) + (my - y) * (my - y);
    gad   = f;
    xmin  = mx;
    ymin  = my;
  }
  //
  mx = x,my = y;
  if (Requester.IsEmpty()) {
    f = Gadget::FindGadgetInDirection(OptionGadget,mx,my,dx,dy);
  } else {
    f = Gadget::FindGadgetInDirection(Requester,mx,my,dx,dy);
  }
  if (f) {
    // Check whether this is a better candidate than before.
    mdist  = (mx - x) * (mx - x) + (my - y) * (my - y);
    if (gad == NULL || (mdist < dist)) {
      gad  = f;
      xmin = mx;
      ymin = my;
      dist = mdist;
    }
  }
  //
  if (gad) {
    x = xmin;
    y = ymin;
  }
  return gad;
}
///
