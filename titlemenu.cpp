/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: titlemenu.cpp,v 1.23 2015/09/13 18:33:02 thor Exp $
 **
 ** In this module: Definition of the class describing the short menu in the
 ** title bar
 **********************************************************************************/

/// Includes
#include "types.h"
#include "titlemenu.hpp"
#include "new.hpp"
#include "bufferport.hpp"
#include "machine.hpp"
#include "menusuperitem.hpp"
#include "menurootitem.hpp"
#include "menuvertitem.hpp"
#include "menuboolitem.hpp"
#include "menuactionitem.hpp"
#include "menuseparatoritem.hpp"
#include "menuselectionitem.hpp"
#include "filerequester.hpp"
#include "display.hpp"
#include "timer.hpp"
#include "exceptions.hpp"
#include <stdarg.h>
///

/// TitleMenu::TitleMenu
TitleMenu::TitleMenu(class Machine *mach)
  : OptionCollector(mach),
    Display(NULL), RootMenu(NULL), 
    BufferPort(new class BufferPort),
    Requester(new class FileRequester(mach)),
    LastPrefsName(new char[14]), LastStateName(new char[14])
{
  strcpy(LastPrefsName,".atari++.conf");
  strcpy(LastStateName,"atari++.state");
}
///

/// TitleMenu::~TitleMenu
TitleMenu::~TitleMenu(void)
{
  RemoveMenu(); // Remove the menu. This also kills the root menu
  delete BufferPort;
  delete Requester;
  delete[] LastPrefsName;
  delete[] LastStateName;
}
///

/// TitleMenu::MenuTopic::MenuTopic
// Create a new topic describing a super menu
TitleMenu::MenuTopic::MenuTopic(const char *title)
  : OptionTopic(title), Root(NULL), CurrentSuper(NULL)
{
}
///

/// TitleMenu::MenuTopic::~MenuTopic
TitleMenu::MenuTopic::~MenuTopic(void)
{
  // The menu root lifetime is not under control of this topic,
  // it is under control of the menu root.
}
///

/// TitleMenu::MenuTopic::OpenSubItem
// Open a new subitem within this topic
void TitleMenu::MenuTopic::OpenSubItem(class MenuRootItem *root,const char *title)
{
  if (CurrentSuper == NULL) {
    // We have no current super item, hence we are creating the super item
    // for this item.
#if CHECK_LEVEL > 0
    if (Root)
      Throw(ObjectExists,"TitleMenu::MenuTopic::OpenSubItem","the super item has been created already");
#endif
    Root         = new class MenuVertItem(root,title);
    // Sub-Items that get openend are then under this root.
    CurrentSuper = Root;
  } else {
    // Create a sub-item of the current super item instead.
    CurrentSuper = new class MenuVertItem(CurrentSuper,title);
  }
}
///

/// TitleMenu::MenuTopic::CloseSubItem
// Close a sub-item by moving up one hierarchy
void TitleMenu::MenuTopic::CloseSubItem(void)
{
#if CHECK_LEVEL > 0
  if (CurrentSuper == NULL)
    Throw(ObjectDoesntExist,"TitleMenu::MenuTopic::CloseSubItem","no sub-item is open to be closed");
#endif
  // Move up the hierarchy if possible.
  // FIXME: This does not return NULL for the root menu, but the root menu.
  // This is only a problem if this function is used in the wrong way.
  CurrentSuper = CurrentSuper->ParentOf();
}
///

/// TitleMenu::MenuTopic::AddOption
// Attach a new option to a given topic. This is overloaded here
// to create sub-menus on the way.
void TitleMenu::MenuTopic::AddOption(class Option *option)
{
  // First call the super method to add the abstract option
  OptionTopic::AddOption(option);
  //
  // Now check whether we have a root item and hence this
  // topic is part of the menu.
  if (CurrentSuper) {
    // Yes, it is. Hence, create a new sub-item of the current
    // super item if possible.
    option->BuildMenuItem(CurrentSuper);
  }
}
///

/// TitleMenu::OpenSubItem
// The following is actually only useful if we want to define a 
// hierarchical menu. If called, a (super) item of the given name is
// created and all following argument parser calls create the sub-items
// of this super-item.
void TitleMenu::OpenSubItem(const char *title)
{
  if (ConfigTime) {
    class MenuTopic *mt = (class MenuTopic *)Current;
    // We are currently collecting items, hence need to build
    // menus. In this case, we need to build the menu items for
    // the current topic.
    mt->OpenSubItem(RootMenu,title);
  }
}
///

/// TitleMenu::CloseSubItem
// Pairs with the above and closes a sub-item
void TitleMenu::CloseSubItem(void)
{
  if (ConfigTime) {
    class MenuTopic *mt = (class MenuTopic *)Current;
    // Forward this request to the topic as well.
    mt->CloseSubItem();
  }
}
///

/// TitleMenu::DisplayMenu
// Display the menu bar on the screen.
void TitleMenu::DisplayMenu(void)
{
#if CHECK_LEVEL > 0
  if (RootMenu == NULL)
    Throw(ObjectDoesntExist,"TitleMenu::DisplayMenu","the root menu has not yet been created");
#endif
  LastButton = false;
  BufferPort->Link(Machine); 
  Display    = Machine->Display();
  RootMenu->DisplayMenu(BufferPort);
  Display->ShowPointer(true);
}
///

/// TitleMenu::RemoveMenu
// Remove the menu bar again
void TitleMenu::RemoveMenu(void)
{
  if (RootMenu) {
    // Make the menu disappear if we have a port for it.
    if (BufferPort) {
      RootMenu->HideMenu(BufferPort);
    }
    delete RootMenu; // This disposes all children as well
    RootMenu = NULL;
  }
  if (BufferPort) {
    // Cut the connection to the display
    BufferPort->Link(NULL);
  }
  if (Display) {
    Display->ShowPointer(false);
  }
  Display = NULL;
}
///

/// TitleMenu::CollectTopics
// Use all the configurables and collect all the topics we can get hands of.
// This builds all the menu structures for the user interface.
void TitleMenu::CollectTopics(void)
{  
  class MenuVertItem *projectmenu;
  // Check whether the root menu exists already. This
  // should not happen
#if CHECK_LEVEL > 0
  if (RootMenu)
    Throw(ObjectExists,"TitleMenu::CollectItems","the root menu has been created already");
#endif
  // Now start the menu by building up the root menu.
  RootMenu = new class MenuRootItem;
  //
  // Now add items to it: First the control menu.
  projectmenu = new class MenuVertItem(RootMenu,"Project");
  new class MenuActionItem(projectmenu,"Load Prefs...",TA_Load);
  new class MenuActionItem(projectmenu,"Save Prefs...",TA_Save);
  new class MenuSeparatorItem(projectmenu);
  new class MenuActionItem(projectmenu,"Load State...",TA_LoadState);
  new class MenuActionItem(projectmenu,"Save State...",TA_SaveState);
  new class MenuSeparatorItem(projectmenu);
  new class MenuActionItem(projectmenu,"Warm Start",TA_WarmStart);
  new class MenuActionItem(projectmenu,"Cold Start",TA_ColdStart);
  new class MenuSeparatorItem(projectmenu);
  new class MenuActionItem(projectmenu,"Full Menu...",TA_Menu);
  new class MenuSeparatorItem(projectmenu);
#ifdef BUILD_MONITOR
  new class MenuActionItem(projectmenu,"Enter Monitor",TA_Monitor);
  new class MenuSeparatorItem(projectmenu);
#endif
  new class MenuActionItem(projectmenu,"Exit",TA_Quit);  
  //
  // Then forward the request to the option collector
  // doing its job fetching the remaining options from
  // the argument parser.
  OptionCollector::CollectTopics();
}
///

/// TitleMenu::FeedEvent
// Collect an event from the display class
// and forward this event to the menu event
// machine. Deliver a high-level event identifier
// back.
int TitleMenu::FeedEvent(struct Event &event)
{  
  LONG x,y;
  bool button;
  struct Event sent;
  int change = Event::Nothing;
  //
  // We only handle mouse events here. No keyboard is required
  // to use the menu.
  // Update the GUI by reading the mouse position from it.
  Display->MousePosition(x,y,button);
  // Construct an event from the mouse position and the button position.
  event.Type   = Event::Mouse;
  if (button  != LastButton) {
    if (x >= 0 && y >= 0 && x < BufferPort->WidthOf() && y < BufferPort->HeightOf()) {
      event.Type = Event::Click;
    } else {
      // Ignore button presses outside the window.
      return change;
    }
  }
  event.Button = button;
  event.X      = x;
  event.Y      = y;
  //
  // Now forward the event to the menu.
  do {
    sent = event;
    if (RootMenu->HitTest(sent,BufferPort)) {
      // Hit an item and generated a useful event.
      // Check whether we got a menu pick; if so, process
      // it by inserting the selected item from the menu
      // into the option.
      switch(sent.Type) {
      case Event::MenuPick:
	{
	  class MenuItem *item = sent.Menu;
	  class Option *option = (class Option *)(item->UserPointerOf());
	  // Check whether this menu carries a topic. If so, install the
	  // settings of the menu now into the topic.
	  if (option) {
	    if (option->ParseMenu()) {
	      // Yes, the setting did change, indeed. Generate
	      // a control event that signals that we need to
	      // re-parse the preferences.
	      change         = TitleMenu::TA_Prefs;
	      event          = sent;
	      break;
	    }
	  }
	}
	// Runs into the following to abort the menu
      case Event::MenuAbort:
	change = TitleMenu::TA_Exit;
	event  = sent;
	break;
      case Event::Request:
	// Open a file requester. Comes from a menu file item.
	{
	  class MenuItem *item = sent.Menu;
	  class OptionCollector::FileOption *option = (class OptionCollector::FileOption *)(item->UserPointerOf());
	  // Shut down the menu
	  RemoveMenu();
	  // Now run the file request by means of the file option "special call".
	  if (option->RequestFile(Requester)) {
	    // The request worked. 
	    change         = TitleMenu::TA_Prefs;
	    event          = sent;
	    break;
	  }
	}
	// Otherwise, abort the menu silently.	
	change = TitleMenu::TA_Exit;
	event  = sent;
	break;
      case Event::Ctrl:
	// Check whether we got a control event. If so, then we set the
	// action ID of the main menu.
	change = sent.ControlId;
	// Store changes back so the outside sees what we did with the event
	event  = sent;
	break;
      default:
	break;
      }
    }
  } while(sent.Resent);
  //
  // Keep the last button state to be able to detect mouse clicks.
  LastButton   = button;
  // Return the high-level event.
  return change;
}
///

/// TitleMenu::AcceptOptionChange
// Accept the changes of the option and try to
// re-install them back into the argument parsing
// entities.
void TitleMenu::AcceptOptionChange(void)
{  
  bool mustcoldstart = false;
  ArgParser::ArgumentChange changeflag;
  // An option may have changed now. Remove the menu just in case...
  RemoveMenu();
  // This may cause an exception because one of the preferences is bad.
  // We need to capture this here.
  try {
    InstallTopics();
  } catch (const AtariException &ex) {
    if (/*ex.TypeOf() != AtariException::Ex_BadPrefs && */ /*ex.TypeOf() != AtariException::Ex_IoErr*/true) {
      class OptionExceptionPrinter Printer(Machine);
      //
      if (ex.TypeOf() == AtariException::Ex_BadPrefs) {
	InstallDefaults();
	InstallTopics();
      }
      //
      // Print the warning over this printer.
      ex.PrintException(Printer);
    }
    // Re-install the defaults again to fix the error, hopefully.
    InstallDefaults();
    // If it still throws, bail out!
    InstallTopics();
  }    
  // Re-install the defaults of the machine and update our change flag.
  // Some options from the command line since the option layout may have
  // changed dramatically.
  changeflag = ReparseState();
  if (changeflag != ArgParser::NoChange) {    
    if (changeflag == ArgParser::ColdStart) {
      mustcoldstart = true;
    }
    // Install default options and add our options
    // on top. Problem is that our options might not
    // cover all available options then.
    SignalBigChange(Machine->ParseArgs(NULL));
    // Install ours on top again.
    InstallTopics();
    // Re-read the flag, reset it back.
    changeflag = ReparseState();
  }
  // Ok, check whether we need to coldstart the system here.
  while (mustcoldstart) {
    Machine->ColdStart();       
    mustcoldstart = false; 
    // Reset this flag and go over
    changeflag = ReparseState();
    if (changeflag  == ArgParser::ColdStart) {
      mustcoldstart = true;
    }    
  }
  //
  // No need to adjust the GUI, we quit anyhow.
}
///

/// TitleMenu::LoadPrefs
// Perform the preferences loading activity. Open a requester,
// ask the user for the file name, and load the settings
void TitleMenu::LoadPrefs(void)
{
  // Shut down the menu
  RemoveMenu();
  if (Requester->Request("Load Prefs From",LastPrefsName,false,true,false)) {
    // Requesting worked. Copy the file name over in case we could
    // really load from here.
    try {
      const char *filename = Requester->SelectedItem();
      // Forward this to the OptionCollector.
      OptionCollector::LoadOptions(filename);
      //
      // If we are here, things really worked fine and we can
      // copy the file name over for the next time.
      delete[] LastPrefsName;
      LastPrefsName = NULL;
      LastPrefsName = new char[strlen(filename) + 1];
      strcpy(LastPrefsName,filename);
      //
      // Possibly perform a coldstart if required.
      if (ReparseState() == ArgParser::ColdStart) {
	Machine->ColdStart();
      }
    } catch(const AtariException &ex) {
      // If we are here, a warning has been generated already.
      // We now check whether we can savely ignore the error.
      if (ex.TypeOf() != AtariException::Ex_IoErr && ex.TypeOf() != AtariException::Ex_BadPrefs) {
	// We can't. Throw up.
	throw;
      }
    }
  }
}
///

/// TitleMenu::SavePrefs
// Save the currently active options to a file. Open
// a file requester asking where to save them, then
// perform the option save by means of the OptionCollector
// super class.
void TitleMenu::SavePrefs(void)
{
  // Shut down the menu
  RemoveMenu();
  if (Requester->Request("Save Prefs To",LastPrefsName,true,true,false)) {
    // Requesting worked. Copy the file name over in case we could
    // really load from here.
    try {
      const char *filename = Requester->SelectedItem();
      // Forward this to the OptionCollector.
      OptionCollector::SaveOptions(filename);
      //
      // If we are here, things really worked fine and we can
      // copy the file name over for the next time.
      delete[] LastPrefsName;
      LastPrefsName = NULL;
      LastPrefsName = new char[strlen(filename) + 1];
      strcpy(LastPrefsName,filename);
    } catch(const AtariException &ex) {
      // If we are here, a warning has been generated already.
      // We now check whether we can savely ignore the error.
      if (ex.TypeOf() != AtariException::Ex_IoErr && ex.TypeOf() != AtariException::Ex_BadPrefs) {
	// We can't. Throw up.
	throw;
      }
    }
  }
}
///

/// TitleMenu::LoadState
// Perform the state loading activity. Open a requester,
// ask the user for the file name, and load the machine state
void TitleMenu::LoadState(void)
{
  // Shut down the menu
  RemoveMenu();
  if (Requester->Request("Load State From",LastStateName,false,true,false)) {
    // Requesting worked. Copy the file name over in case we could
    // really load from here.
    try {
      const char *filename = Requester->SelectedItem();
      // Forward this to the OptionCollector.
      OptionCollector::LoadState(filename);
      //
      // If we are here, things really worked fine and we can
      // copy the file name over for the next time.
      delete[] LastStateName;
      LastStateName = NULL;
      LastStateName = new char[strlen(filename) + 1];
      strcpy(LastStateName,filename);
    } catch(const AtariException &ex) {
      // If we are here, a warning has been generated already.
      // We now check whether we can savely ignore the error.
      if (ex.TypeOf() != AtariException::Ex_IoErr && ex.TypeOf() != AtariException::Ex_BadPrefs) {
	// We can't. Throw up.
	throw;
      }
    }
  }
}
///

/// TitleMenu::SavePrefs
// Save the current machine state to a file. Open
// a file requester asking where to save them, then
// perform the option save by means of the OptionCollector
// super class.
void TitleMenu::SaveState(void)
{
  // Shut down the menu
  RemoveMenu();
  if (Requester->Request("Save State To",LastStateName,true,true,false)) {
    // Requesting worked. Copy the file name over in case we could
    // really load from here.
    try {
      const char *filename = Requester->SelectedItem();
      // Forward this to the OptionCollector.
      OptionCollector::SaveState(filename);
      //
      // If we are here, things really worked fine and we can
      // copy the file name over for the next time.
      delete[] LastStateName;
      LastStateName = NULL;
      LastStateName = new char[strlen(filename) + 1];
      strcpy(LastStateName,filename);
    } catch(const AtariException &ex) {
      // If we are here, a warning has been generated already.
      // We now check whether we can savely ignore the error.
      if (ex.TypeOf() != AtariException::Ex_IoErr && ex.TypeOf() != AtariException::Ex_BadPrefs) {
	// We can't. Throw up.
	throw;
      }
    }
  }
}
///

/// TitleMenu::EnterMenu
// Open the menu here: This is the main entry
// point for menu handling.
void TitleMenu::EnterMenu(void)
{
  class Timer eventtimer;
  bool quit = false;
  struct Event event;
  // First step: Create the menu. Add our own items,
  // then parse all arguments to insert the menu-able items
  // into the title menu.
  CollectTopics();
  // Then display the menu on the screen.
  DisplayMenu();
  try {
    eventtimer.StartTimer(0,25*1000); // use a 25 msec cycle here.
    // This main loop seems indeed strange and is not at all "event driven"
    // in the classical sense. The problem here is, though, that the display
    // frontend is so utterly primitive that we cannot depend on any kind of
    // events it may or may not generate. Hence, we need to use a "polling
    // type" front end here.
    do {
      int change = FeedEvent(event);
      switch(change) {      
      case TA_Prefs:
	AcceptOptionChange();
	quit = true;
	break;
	// Run a warm start?
      case TA_WarmStart:
	// This shouldn't do any harm to the menu, but for good measure...
	RemoveMenu();
	Machine->WarmStart();
	quit = true;
	break;
      case TA_ColdStart:
	RemoveMenu();
	Machine->ColdStart();
	quit = true;
	break;
      case TA_Menu:
	// Remember to start the full menu
	Machine->LaunchMenu() = true;
	quit = true;
	break;
      case TA_Monitor:
	// Remember to start the monitor as soon as we're out here.
	Machine->LaunchMonitor() = true;
	quit = true;
	break;
      case TA_Exit:
	// Exit the menu and continue emulation.
	quit = true;
	break;
      case TA_Quit:
	// Quit the emulator completely.
	Machine->Quit() = true;
	break;
      case TA_Load:
	// Load preferences from a file.
	LoadPrefs();
	quit = true;
	break;
      case TA_Save:
	// Save preferences to a file
	SavePrefs();
	quit = true;
	break;
      case TA_LoadState:
	// Load machine state from a file.
	LoadState();
	quit = true;
	break;
      case TA_SaveState:
	// Save machine state to a file.
	SaveState();
	quit = true;
	break;
      default:
	break;
      }
      if (quit || Machine->Quit())
	break;
      // We must break early, the option
      // installation might have unlinked the port
      // already.
      BufferPort->Refresh();
      eventtimer.WaitForEvent(); // Now wait 25msecs
      eventtimer.TriggerNextEvent();
    } while(true);
  } catch(...) {
    // Before we quit: Cleanup the menu and
    // release the graphics.
    RemoveMenu();
    throw;
  }
  //
  RemoveMenu();
}
///
