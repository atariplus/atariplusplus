/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: titlemenu.hpp,v 1.11 2015/05/21 18:52:43 thor Exp $
 **
 ** In this module: Definition of the class describing the short menu in the
 ** title bar
 **********************************************************************************/

#ifndef TITLEMENU_HPP
#define TITLEMENU_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "argparser.hpp"
#include "optioncollector.hpp"
///

/// Forwards
class Machine;
class MenuSuperItem;
class MenuRootItem;
class MenuItem;
class BufferPort;
class AtariDisplay;
class Gadget;
class FileRequester;
///

/// Class TitleMenu
// This class describes the quick and short menu found in the
// or near the title bar of the window the emulator runs in.
// Otherwise, it uses the same neat tricks as the (full) menu,
// namely by implementing the argument parser.
class TitleMenu : private OptionCollector {
  //
public:
  // Action identifiers for requesting specific activity.
  enum ActionId {
    TA_Prefs  = 1, // re-install the preferences, got changed.
    TA_Load,       // load preferences from disk
    TA_Save,       // save preferences to disk
    TA_LoadState,  // load machine state from disk
    TA_SaveState,  // save machine state to disk
    TA_WarmStart,  // warm start the machine
    TA_ColdStart,  // cold start the machine
    TA_Menu,       // enter the full menu
    TA_Monitor,    // enter the built-in monitor
    TA_Exit,       // leave the menu
    TA_Quit        // leave the emulator
  };
private:
  //
  // Derived from the topic, a menu topic that also contains
  // the super menu for this topic.
  class MenuTopic : public OptionTopic {
    //
    // Pointer to the root menu representing this topic *if*
    // we open a sub item here. Note that the menu topic does 
    // not control the life time of this; the root menu does.
    class MenuSuperItem *Root;
    //
    // The current super item new items become sub-items of
    class MenuSuperItem *CurrentSuper;
    //    
    // Create the right-hand menu of all options for the given
    // topic. Since this guy here isn't gadget driven, this is
    // just a dummy.
    virtual void CreateOptionGadgets(List<class Gadget> &)
    {
      Throw(NotImplemented,"OptionTopic::CreateOptionGadgets","can't create gadgets for the menu");
    }
    //
    // Event handling: Perform interpretation of "menu pick" events and collect
    // the result from the menu operation.
    virtual bool HandleEvent(struct Event &)
    { 
      Throw(NotImplemented,"OptionTopic::HandleEvent","no event handler");
      return false;
    }
    //
  public:
    MenuTopic(const char *title);
    ~MenuTopic(void);
    //
    // Open a new subitem within this topic
    void OpenSubItem(class MenuRootItem *root,const char *title);
    // Close a sub-item.
    void CloseSubItem(void);    
    //
    // Attach a new option to a given topic. This is overloaded here
    // to create sub-menus on the way.
    virtual void AddOption(class Option *option);
    //
  };
  //
  // The display we render into
  class AtariDisplay  *Display;
  //
  // The root menu
  class MenuRootItem  *RootMenu;
  //
  // The graphics ground we render into.
  class BufferPort    *BufferPort;
  //
  // The last button state we got from the display
  // This is kept here to distinguish between button clicks
  // and simple mouse movements.
  bool                 LastButton;
  //
  // Pointer to a file requester for general purpose.
  class FileRequester *Requester;
  //
  // The last used preferences file name.
  char                *LastPrefsName;
  //
  // The last used state file name.
  char                *LastStateName;
  //
  // The following is actually only useful if we want to define a 
  // hierarchical menu. If called, a (super) item of the given name is
  // created and all following argument parser calls create the sub-items
  // of this super-item.
  virtual void OpenSubItem(const char *title);
  //  
  // Pairs with the above and closes a sub-item
  virtual void CloseSubItem(void);
  //  
  // Use all the configurables and collect all the topics we can get hands of.
  // This builds all the menu structures for the user interface.
  void CollectTopics(void);
  //
  // A virtual callback that must deliver an option topic. This
  // is here to allow a derived class to enhance the OptionTopic by
  // something smarter if required. This method is called to 
  // generate new topics on argument parsing.
  virtual class OptionTopic *BuildTopic(const char *title)
  {
    return new class MenuTopic(title);
  }
  //
  // Display the menu bar on the screen.
  void DisplayMenu(void);
  // Remove the menu bar again
  void RemoveMenu(void);
  //
  // Accept the changes of the option and try to
  // re-install them back into the argument parsing
  // entities.
  void AcceptOptionChange(void);
  //
  // Collect an event from the display class
  // and forward this event to the menu event
  // machine. Deliver a high-level event identifier
  // back.
  int FeedEvent(struct Event &event);
  //
  // Perform the preferences loading activity. Open a requester,
  // ask the user for the file name, and load the settings
  void LoadPrefs(void);
  // Save the prefs to a file name. Works otherwise as above.
  void SavePrefs(void);
  //
  // Load the machine state from a file. Opens a requester to
  // ask the user where to load from, then performs the saving.
  void LoadState(void);
  // The same again for saving the machine state to a user
  // selected file.
  void SaveState(void);
  //
public:
  TitleMenu(class Machine *mach);
  virtual ~TitleMenu(void);
  //
  // Open the menu here.
  void EnterMenu(void);
  //
};
///

///
#endif
