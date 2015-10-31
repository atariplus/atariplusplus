/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menutopic.hpp,v 1.6 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of a topic, i.e. a collection of options.
 **********************************************************************************/

#ifndef MENU_TOPIC_HPP
#define MENU_TOPIC_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "stdio.hpp"
#include "event.hpp"
///

/// Forwards
class Gadget;
class Option;
///

/// Class Topic
// A sideways gadget that selects a list of options under a common
// topic, or a root menu item that collects a subset of items.
class Topic : public Node<class Topic> {
protected:
  //
  // Name of the topic. This gets allocated by this class.
  char                     *Title;
  //
public:
  Topic(const char *title);
  virtual ~Topic(void);
  //
  // Check whether this topic matches a given name: This is used for
  // the preferences handling to find a given setting by its name.
  // Non-option topics must never match here as we never want them 
  // within preferences processing.
  virtual bool Matches(const char *)
  {
    return false;
  }
  //
  // Return the name of the item.
  const char *NameOf(void) const
  {
    return Title;
  }
  //
  // Create the right-hand menu of all options for the given
  // topic.
  virtual void CreateOptionGadgets(List<class Gadget> &GList) = 0;
  //
  // Re-install the defaults into all topics and options in case a setting
  // is invalid. This should hopefully fix it. We may only implement this
  // for preferences related topics.
  virtual void InstallDefaults(void)
  {
  }
  //
  // Handle an even that is related/created by this topic. Return a boolean
  // whose meaning/interpretation is up to the caller.
  virtual bool HandleEvent(struct Event &ev) = 0;
  //
  // Save the state of the options under this topic to the configuration file.
  // Default is not to save anything because there's nothing saveable.
  virtual void SaveTopic(FILE *)
  {
  }
};  
///

/// Class OptionTopic
// A list of "topics" given by the "titles" of the various configurables.
class OptionTopic  : public Topic {
protected:
  //
  // All options that are available under this topic.
  List<Option> OptionList;
  //
public:
  OptionTopic(const char *title);
  virtual ~OptionTopic(void);
  //
  // Find an option by name and return its setting as an ASCII string
  class Option *FindOption(const char *name);
  //
  // Attach a new option to a given topic.
  virtual void AddOption(class Option *option);
  //
  // Check whether this topic matches a given name
  virtual bool Matches(const char *name);
  //
  // Re-install the defaults into all topics and options in case a setting
  // is invalid. This should hopefully fix it.
  virtual void InstallDefaults(void);    
  //
  // Save the state of the options under this topic to the configuration file.
  virtual void SaveTopic(FILE *to);
};
///

///
#endif
