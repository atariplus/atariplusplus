/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menutopic.cpp,v 1.21 2015/05/21 18:52:41 thor Exp $
 **
 ** In this module: Definition of a topic, i.e. a collection of options.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "stdio.hpp"
#include "menutopic.hpp"
#include "menuoption.hpp"
#include "optioncollector.hpp"
#include "renderport.hpp"
#include "textgadget.hpp"
#include "verticalgroup.hpp"
#include "separatorgadget.hpp"
#include "buttongadget.hpp"
#include "filegadget.hpp"
///

/// Topic::Topic
// Construct a topic node from a name
Topic::Topic(const char *title)
  : Title(new char[strlen(title)+1])
{ 
  // We must allocate the title as the class that owns this string might have
  // setup this as a temporary such that its lifetime does not extend that
  // of the call of this constructor.
  strcpy(Title,title);
}
///

/// Topic::~Topic
Topic::~Topic(void)
{
  delete[] Title;
}
///

/// OptionTopic::OptionTopic
OptionTopic::OptionTopic(const char *title)
  : Topic(title)
{
}
///

/// OptionTopic::~OptionTopic
OptionTopic::~OptionTopic(void)
{  
  class Option *node;
  // Remove all nodes from the option list and delete them.
  // As the nodes do not add them themselves to this list,
  // we have to dispose them as well.
  while((node = OptionList.RemHead())) {
    delete node;
  }
}
///

/// OptionTopic::Matches
// Check whether the indicated name matches the topic. This is just a
// string compare.
bool OptionTopic::Matches(const char *name)
{
  // Now, this is easy: If the name string matches, return that we match...
  if (!strcasecmp(name,Title))
    return true;
  return false;
}
///

/// OptionTopic::AddOption
// Attach a new option to a given topic.
void OptionTopic::AddOption(class Option *option)
{
  OptionList.AddTail(option);
}
///

/// OptionTopic::InstallDefaults
// Re-install the defaults into all options of this topic to
// fix a configuration error and re-establish the old setting.
void OptionTopic::InstallDefaults(void)
{
  class Option *option;

  for(option = OptionList.First();option;option = option->NextOf()) {
    option->InstallDefault();
  }
}
///

/// OptionTopic::FindArgument
// Find an option by name and return its setting as an ASCII string
class Option *OptionTopic::FindOption(const char *name)
{
  class Option *option;
  //
  // Scan thru the list of options under this topic and return the setting
  // if found.
  for(option = OptionList.First();option;option = option->NextOf()) {
    if (option->Matches(name)) {
      return option;
    }
  }
  return NULL;
}
///

/// OptionTopic::SaveTopic
// Save the state of the options under this topic to the configuration file.
void OptionTopic::SaveTopic(FILE *to)
{
  class Option *option;

  fprintf(to,"#\n#%s specific settings:\n",Title);

  for(option = OptionList.First(); option; option = option->NextOf()) {
    option->SaveOption(to);
  }
  
  fprintf(to,"#\n");

}
///

