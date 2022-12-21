/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menu.cpp,v 1.70 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Definition of a graphical frontend with the build-in graphics
 **********************************************************************************/

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "menu.hpp"
#include "timer.hpp"
#include "display.hpp"
#include "machine.hpp"
#include "textgadget.hpp"
#include "topicgadget.hpp"
#include "buttongadget.hpp"
#include "filegadget.hpp"
#include "separatorgadget.hpp"
#include "verticalgroup.hpp"
#include "errorrequester.hpp"
#include "exceptions.hpp"
///

/// Menu::Menu
// Constructor of the menu. This does currently not very much
Menu::Menu(class Machine *mach)
  : OptionCollector(mach), EventFeeder(NULL), TopicGadget(NULL),
    ControlTopic(NULL), RPort(NULL), LastTopic(NULL), TopicProp(0), OptionProp(0),
    LoadFileName(new char[strlen(".atari++.conf")+1]), SaveFileName(new char[strlen(".atari++.conf")+1]),
    LoadStateName(new char[strlen("atari++.state")+1]), SaveStateName(new char[strlen("atari++.state")+1])
{
  strcpy(LoadFileName,".atari++.conf");
  strcpy(SaveFileName,".atari++.conf");
  strcpy(LoadStateName,"atari++.state");
  strcpy(SaveStateName,"atari++.state");
}
///

/// Menu::~Menu
// Destructor. This also disposes the RPort and the topic list
Menu::~Menu(void)
{
  // Delete load and save filenames
  delete[] LoadFileName;
  delete[] SaveFileName;
  delete[] LoadStateName;
  delete[] SaveStateName;
  //
  delete EventFeeder;
  DeleteGList();
  //
  // Delete the RPort as well should we still carry this around
  delete RPort;
  delete[] LastTopic;
}
///

/// Menu::CollectTopics
// Use all the configurables and collect all the topics we can get hands of.
// This builds all the menu structures for the user interface.
void Menu::CollectTopics(void)
{
  // The ControlTopic also in the list of topics
  // and will get disposed by the option collector
  // on its way.
  ControlTopic = NULL; 
  if (TopicGadget) {
    delete TopicGadget;
    TopicGadget = NULL;
  }
  //
  // Now call the super method. Will dispose the
  // old topics on its way
  OptionCollector::CollectTopics();
  //    
  // Build the control topic of this menu here.
  ControlTopic = new class ControlTopic(RPort,LoadFileName,SaveFileName,
					LoadStateName,SaveStateName);
  // Install the topmost menu control topic
  Topics.AddHead(ControlTopic);
}
///

/// Menu::DeleteGList
// Delete all gadgets from the gadget list
void Menu::DeleteGList(void)
{
  class Gadget *gadget;
  // Delete all gadgets. The gadgets unlink themselves (unlike topics)
  while((gadget = GList.First())) {
    delete gadget;
  }
}
///

/// Menu::RefreshGUI
// Refresh the GUI after a possible warning operation
// by re-drawing everything.
void Menu::RefreshGUI(void)
{
  class Gadget *gadget;  
  RPort->SetPen(8);
  RPort->FillRaster(); // grey background.  
  // get the first gadget and iterate until its over.
  for(gadget = GList.First();gadget;gadget = gadget->NextOf()) {
    gadget->Refresh();
  } 
  Machine->Display()->ShowPointer(true);  
  RPort->Refresh();
}
///

/// Menu::CreateMenu
void Menu::CreateMenu(void)
{
  DeleteGList();
  // Create the top level gadget group and install gadgets into it.
  TopicGadget = new class TopicGadget(GList,RPort,112,&Topics);
  //
  // If we have a topic name, scroll to the indicated topic.
  TopicGadget->ScrollToName(LastTopic,TopicProp,OptionProp);
}
///

/// Menu::RestartGUI
// Relaunch the GUI by building the GUI engine
void Menu::RestartGUI(void)
{ 
#if CHECK_LEVEL > 0
  if (EventFeeder) {
    Throw(ObjectExists,"Menu::RestartGUI",
	  "The GUI has been build up already");
  }
#endif  
  if (RPort == NULL) {
    // We need to build a new render port.
    RPort = new class RenderPort;
  }
  // Get the linkage to the keyboard class
  Keyboard = Machine->Keyboard();
  //  
  CollectTopics();
  RPort->Link(Machine);
  RPort->SetPen(8);
  RPort->FillRaster(); // grey background.  
  CreateMenu();
  RefreshGUI();

  EventFeeder = new class EventFeeder(Machine->Display(),Machine->Keyboard(),
				      Machine->Joystick(0),
				      &GList,RPort);  
  Machine->Display()->ShowPointer(true);
  Machine->Display()->EnforceFullRefresh();
  RPort->Refresh();
}
///

/// Menu::DisposeGUI
// Clear all members that are required by the GUI
void Menu::DisposeGUI(void)
{
  Machine->Display()->EnforceFullRefresh();
  Machine->Display()->ShowPointer(false);
  if (TopicGadget)
    TopicGadget->ActiveTopicName(LastTopic,TopicProp,OptionProp);
  delete EventFeeder;
  EventFeeder = NULL;
  RPort->Link(NULL);
}
///

/// Menu::SaveOptions
// Save options to a named file
void Menu::SaveOptions(const char *filename)
{	
  char *newfilename;
  //
  // Call the super method to do the dirty work for us.
  OptionCollector::SaveOptions(filename);
  //
  // If we are here, no error has been created on saving the options
  newfilename = new char[strlen(filename) + 1];
  strcpy(newfilename,filename);
  delete[] SaveFileName;
  SaveFileName = newfilename;
  ControlTopic->SetSaveFile(SaveFileName);
}
///

/// Menu::LoadOptions
// Load new options from a file, install them
// into the menu.
void Menu::LoadOptions(const char *filename)
{
  // Dispose the GUI first, a lot of stuff is gonna change.
  DisposeGUI();
  // Call the super method
  try {
    char *newfilename;
    OptionCollector::LoadOptions(filename);
    // If successful, enter the new filename into the
    // gadget to indicate that things worked out fine.
    newfilename  = new char[strlen(filename) + 1];
    strcpy(newfilename,filename);
    delete[] LoadFileName;
    LoadFileName = newfilename;
    ControlTopic->SetLoadFile(LoadFileName);
  } catch(const AtariException &ex) {
    if (ex.TypeOf() != AtariException::Ex_BadPrefs && ex.TypeOf() != AtariException::Ex_IoErr) {
      // These two above are accepted "as handled". All others are serious enough
      // and passed out.
      throw;
    }
  }
  // Re-build the topic gadget.
  RestartGUI();
  TopicGadget->ReInstallOptions();
}
///

/// Menu::SaveState
// Save the current machine state to a file
void Menu::SaveState(const char *filename)
{
  // all this is handled in the machine already.
  try {
    char *newname;
    OptionCollector::SaveState(filename);
    //
    // Saving worked. Allocate a new text and assign it
    newname = new char[strlen(filename) + 1];
    strcpy(newname,filename);
    delete[] SaveStateName;
    SaveStateName = newname;
    ControlTopic->SetSaveStateFile(SaveStateName);
  } catch(...) {
    ControlTopic->SetSaveStateFile(SaveStateName);
  }
}
///

/// Menu::LoadState
// Load the current machine state from a file
void Menu::LoadState(const char *filename)
{
  try {
    char *newname;
    OptionCollector::LoadState(filename);
    //
    // Saving worked. Allocate a new text and assign it
    newname = new char[strlen(filename) + 1];
    strcpy(newname,filename);
    delete[] LoadStateName;
    LoadStateName = newname;
    ControlTopic->SetLoadStateFile(LoadStateName);
  } catch(...) {
    ControlTopic->SetLoadStateFile(LoadStateName);
  } 
  RefreshGUI();
}
///

/// Menu::AcceptOptionChange
// Accept an option change that might have been generated
// by the user menu
void Menu::AcceptOptionChange(void)
{     
  bool mustcoldstart = false;
  ArgParser::ArgumentChange changeflag;
  // An option may have changed now. If so, move the gadget setting back
  // into the option and check whether something got adjusted here.
  // This may cause an exception because one of the preferences is bad.
  // We need to capture this here.
  try {
    InstallTopics();
  } catch (const AtariException &ex) {
    if (/*ex.TypeOf() != AtariException::Ex_BadPrefs && */ /*ex.TypeOf() != AtariException::Ex_IoErr*/ true) {
      class OptionExceptionPrinter Printer(Machine);
      //
      if (ex.TypeOf() == AtariException::Ex_BadPrefs) {
	InstallDefaults();
	InstallTopics();
      }
      // Print the warning over this printer.
      ex.PrintException(Printer);
    }
    // Re-install the defaults again to fix the error, hopefully.
    InstallDefaults();
    // If it still throws, bail out!
    InstallTopics();
    // Re-build the topic gadget.
    TopicGadget->ReInstallOptions();
  }
  // Re-install the defaults of the machine and update our change flag.
  // Some options from the command line since the option layout may have
  // changed dramatically.
  changeflag = ReparseState();
  if (changeflag != ArgParser::NoChange) {
    if (changeflag == ArgParser::ColdStart) {
      mustcoldstart = true;
    }
    DisposeGUI();
    try {
      // Re-parse the global args again. This might cause a bad-prefs
      // exception which we silently ignore here.
      SignalBigChange(Machine->ParseArgs(NULL));
    } catch(const class AtariException &ex) {
      if (ex.TypeOf() != AtariException::Ex_BadPrefs && ex.TypeOf() != AtariException::Ex_IoErr)
	throw;
    }
    // Install ours on top again.
    InstallTopics();
    //
    // Disabled the GUI? No way!
    if (Machine->hasGUI() == false) {
      //Throw(NotImplemented,"Menu::AcceptOptionChange","the full menu requires a display");
      throw AsyncEvent(AsyncEvent::Ev_EnterMenu);
    }
    //
    // Re-read the flag, reset it back.
    changeflag = ReparseState();
    RestartGUI();
  }
  // Ok, check whether we need to coldstart the system here.  
  while (mustcoldstart) {
    DisposeGUI();
    Machine->ColdStart();       
    mustcoldstart = false; 
    // Reset this flag and go over
    changeflag = ReparseState();
    if (changeflag  == ArgParser::ColdStart) {
      mustcoldstart = true;
    }    
    RestartGUI();
  }
  // The prefs parser could have run a warning requester, requiring a gadget
  // refresh here.
  RefreshGUI();
}
///

/// Menu::EnterMenu
// Start the menu.
void Menu::EnterMenu(void)
{
  class Timer eventtimer;
  bool quit = false;
  struct Event event;
  unsigned char angle = 0;
  //
  try {
    RestartGUI();
  } catch(...) {
    DisposeGUI();
    throw;
  }
  // Enclose the following into a try-catch block such that
  // we release the graphics before we exit.
  do {
    try {
      // On the first time, if we do not yet have a topic, draw the title here.
      eventtimer.StartTimer(0,25*1000); // use a 25 msec cycle here.
      // This main loop seems indeed strange and is not at all "event driven"
      // in the classical sense. The problem here is, though, that the display
      // frontend is so utterly primitive that we cannot depend on any kind of
      // events it may or may not generate. Hence, we need to use a "polling
      // type" front end here.
      do {
	int change;
	if (EventFeeder == NULL) {
	  RestartGUI();
	}
	change = EventFeeder->PickedOption(event);
	switch (change) {
	case MA_Prefs:
	  AcceptOptionChange();
	  break;
	  // Load options from disk?
	case MA_Load:
	  LoadOptions(((class FileGadget *)event.Object)->GetStatus());
	  break;
	  // Save options back to disk?
	case MA_Save:
	  SaveOptions(((class FileGadget *)event.Object)->GetStatus());
	  break;
	case MA_LoadState:
	  LoadState(((class FileGadget *)event.Object)->GetStatus());
	  break;
	case MA_SaveState:
	  SaveState(((class FileGadget *)event.Object)->GetStatus());
	  break;
	  // Run a warm start?
	case MA_WarmStart:
	  // This shouldn't do any harm to the menu, but for good measure...
	  DisposeGUI();
	  Machine->WarmStart();
	  RestartGUI();
	  break;
	case MA_ColdStart:
	  DisposeGUI();
	  Machine->ColdStart();
	  RestartGUI();
	  break;
	case MA_Monitor:
	  // Remember to start the monitor as soon as we're out here.
	  Machine->LaunchMonitor() = true;
	  quit = true;
	  break;
	case MA_Exit:
	  // Exit the menu and continue emulation.
	  quit = true;
	  break;
	case MA_Quit:
	  // Quit the emulator completely.
	  Machine->Quit() = true;
	  break;
	case MA_Comeback:
	  RPort->Refresh();
	  continue;
	}
	if (TopicGadget->HaveActiveTopic() == false) {
	  DrawTitle(RPort,128,48,angle);
	  angle++;
	}
	RPort->Refresh();
	eventtimer.WaitForEvent(); // Now wait 25msecs
	eventtimer.TriggerNextEvent();
      } while(!Machine->Quit() && !quit);
      //
      // Default activity now: Quit the outside
      // loop as well.
      quit = true;
    } catch(const class AtariException &ex) {
      int action;
      // Uhoh, a user error, still something wrong
      // with the prefs?
      // Check whether the user prefers to quit anyhow.
      // In that case, nothing's to do, really.
      if (Machine->Quit()) {
	action = ErrorRequester::ERQ_Cancel;
      } else {
	// Otherwise, handle the error thru the standard error
	// requester.
	action = Machine->PutError(ex);
      }
      //
      switch(action) {
      case ErrorRequester::ERQ_Monitor:
	Machine->LaunchMonitor() = true;
	// Falls through.
      case ErrorRequester::ERQ_Menu:
      case ErrorRequester::ERQ_Retry:
	quit = false;
	// Check whether we need to restart the GUI
	// Do so if required.
	try {
	  if (EventFeeder == NULL) {
	    RestartGUI();
	  } else {
	    RefreshGUI();
	  }
	} catch(...) {
	  DisposeGUI();
	  throw;
	}
	break;
      default:
	DisposeGUI();
	throw;
      }
    } catch(const AsyncEvent &av) {
      if (av.TypeOf() == AsyncEvent::Ev_EnterMenu) {
	if (Machine->hasGUI() == false) {
	  quit = true;
	} else if (EventFeeder) {
	  // We are already in the menu, but Rebuild it here.
	  RefreshGUI();
	}
      } else {
	DisposeGUI();
	throw;
      }
    } catch(...) {
      // Before we quit: Cleanup the menu and
      // release the graphics.
      DisposeGUI();
      throw;
    }
    // Until really done. The error handler might
    // throw us back.
  } while(!quit);
  //
  DisposeGUI();      
  //
  // Possibly perform a coldstart if required.
  if (ReparseState() == ArgParser::ColdStart) {
    Machine->ColdStart();
  }
}
///

/// Menu::DrawTitle
// Draw a nice title screen into the free area
void Menu::DrawTitle(class RenderPort *rp,LONG dx,LONG dy,int angle)
{
  int line;
  int dash;
  int si,co;
  int l,r;
  LONG x,y;
  UBYTE fp,sp;
  static const int lines = 72;
  static const int thick = 16;
  static const unsigned char atarilogo[72][6] = {
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {34,42,44,60,62,70},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {32,42,44,60,62,72},
    {30,42,44,60,62,74},
    {30,42,44,60,62,74},
    {30,42,44,60,62,74},
    {30,40,44,60,64,74},
    {30,40,44,60,64,74},
    {28,40,44,60,64,76},
    {28,40,44,60,64,76},
    {28,40,44,60,64,76},
    {28,40,44,60,64,76},
    {26,40,44,60,64,78},
    {26,38,44,60,66,78},
    {26,38,44,60,66,78},
    {24,38,44,60,66,80},
    {24,38,44,60,66,80},
    {24,38,44,60,66,80},
    {22,36,44,60,68,82},
    {22,36,44,60,68,82},
    {20,36,44,60,68,84},
    {20,36,44,60,68,84},
    {18,34,44,60,70,86},
    {18,34,44,60,70,86},
    {16,34,44,60,70,88},
    {14,32,44,60,72,90},
    {12,32,44,60,72,92},
    {10,32,44,60,72,94},
    { 6,30,44,60,74,98},
    {0,30,44,60,74,104},
    {0,28,44,60,76,104},
    {0,28,44,60,76,104},
    {0,26,44,60,78,104},
    {0,26,44,60,78,104},
    {0,24,44,60,80,104},
    {0,22,44,60,82,104},
    {0,20,44,60,84,104},
    {0,18,44,60,86,104},
    {0,16,44,60,88,104},
    {0,12,44,60,92,104},
    {0, 8,44,60,96,104}
  };
  //
  // The IRIX compiler uses unsigned characters by default. We don't.
  static const signed char costable[256] = 
    {127,127,127,127,126,126,126,125,125,124,123,122,122,121,120,118,117,116,115,113,112,110,109,107,106,104,102,
     100,98,96,94,92,90,87,85,83,80,78,75,73,70,68,65,62,60,57,54,51,48,45,42,39,37,34,30,27,24,21,18,15,12,9,6,3,
     0,-3,-6,-9,-12,-15,-18,-21,-24,-27,-30,-34,-37,-39,-42,-45,-48,-51,-54,-57,-60,-62,-65,-68,-70,-73,-75,-78,-80,
     -83,-85,-87,-90,-92,-94,-96,-98,-100,-102,-104,-106,-107,-109,-110,-112,-113,-115,-116,-117,-118,-120,-121,-122,
     -122,-123,-124,-125,-125,-126,-126,-126,-127,-127,-127,-127,-127,-127,-127,-126,-126,-126,-125,-125,-124,-123,
     -122,-122,-121,-120,-118,-117,-116,-115,-113,-112,-110,-109,-107,-106,-104,-102,-100,-98,-96,-94,-92,-90,-87,
     -85,-83,-80,-78,-75,-73,-70,-68,-65,-62,-60,-57,-54,-51,-48,-45,-42,-39,-37,-34,-30,-27,-24,-21,-18,-15,-12,-9,
     -6,-3,0,3,6,9,12,15,18,21,24,27,30,34,37,39,42,45,48,51,54,57,60,62,65,68,70,73,75,78,80,83,85,87,90,92,94,96,
     98,100,102,104,106,107,109,110,112,113,115,116,117,118,120,121,122,122,123,124,125,125,126,126,126,127,127,127};
  //
  //
  co = costable[angle];
  si = costable[(angle-64) & 0xff];
  if ((si^co) >= 0) {
    l  = thick * abs(si) / 128;
    x  = dx + (l>>1);
    r  = 0;
  } else {
    l  = 0;
    r  = thick * abs(si) / 128;
    x  = dx - (r>>1);
  }
  co = abs(co);
  si = abs(si);
  fp = UBYTE(0x10 + 8 + 8 * co / 128);
  y  = dy;
  rp->SetPen(0x0f);
  for(line=0;line<lines;line++) {
    const unsigned char *p;
    bool on    = false;
    int x1     = dx - thick;
    int x2;
    p          = atarilogo[line];
    for(dash=0;dash<7;dash++) {
      int limit,xe;
      int   hm;
      if (dash < 6) {
	x2 = (((*p - 52L) * co / 64) + 104) + x;
      } else {
	x2 = (104<<1) + dx + thick;
      }
      hm   = ((abs(*p - 52L) >> 3) * si) / 96;
      hm  += 4;
      if (hm < 0) {
	hm = 0;
      } else if (hm > 15) {
	hm = 15;
      }
      sp   = UBYTE(0x20 | hm);
      // Check for extra "thickness" lines we need to draw.
      if (on) {
	// This line is "on", hence we are at the left edge of a visual line
	// that extends from x1 to x2. We may have to extend it to
	// the right beyond x2.
	xe = x2 + r;
	if (r > 0 && dash < 5) {	  
	  limit = (((p[1] - 52L) * co / 64) + 104) + x;
	  if (xe > limit) xe = limit;
	}
	rp->SetPen(fp);
	rp->Position(x1,y);
	rp->DrawHorizontal(x2-x1);
	rp->Position(x1,y+1);
	rp->DrawHorizontal(x2-x1);
	rp->SetPen(sp);
	rp->Position(x2,y);
	rp->DrawHorizontal(xe-x2);
	rp->Position(x2,y+1);
	rp->DrawHorizontal(xe-x2);
	x2 = xe;
      } else {
	// This line is "off", hence we are at the left of a line that starts
	// at x2. We may have to extend this line to the left. Compute how much
	// we have to extend it.
	xe = x2 - l;
	if (l > 0) {
	  if (dash > 0) {
	    limit = (((p[-1] - 52l) * co / 64) + 104) + x;
	    if (xe < limit) xe = limit;
	  } 
	  if (dash >= 5) {
	    xe = x2;
	  }
	}
	rp->SetPen(0x08);
	rp->Position(x1,y);
	rp->DrawHorizontal(xe-x1);
	rp->Position(x1,y+1);
	rp->DrawHorizontal(xe-x1);
	rp->SetPen(sp);
	rp->Position(xe,y);
	rp->DrawHorizontal(x2-xe);
	rp->Position(xe,y+1);
	rp->DrawHorizontal(x2-xe);
      }
      p++;
      on = !on;
      x1 = x2;
    }
    y += 2;
  }
  //
  // Write the title text to the screen.
  for(y = 0;y < 2;y++) {
    rp->SetPen(UBYTE(4 - (y<<2)));
    rp->Position(dx + LONG((104*2 - strlen("Atari++ Emulator " PACKAGE_VERSION)*8) >> 1) + y, dy + 16 + y);
    rp->Text("Atari++ Emulator " PACKAGE_VERSION);
    rp->Position(dx + LONG((104*2 - strlen("by THOR Software")*8) >> 1) + y,dy + 32 + y);
    rp->Text("by THOR Software");
  }
}
///

/// Menu::GadgetTopic::GadgetTopic
Menu::GadgetTopic::GadgetTopic(class RenderPort *rp,const char *title)
  : OptionTopic(title), RPort(rp)
{
}
///

/// Menu::GadgetTopic::~GadgetTopic
Menu::GadgetTopic::~GadgetTopic(void)
{  
}
///

/// Menu::GadgetTopic::CreateOptionGadgets
// Create the right-hand menu of all options for the given
// topic.
void Menu::GadgetTopic::CreateOptionGadgets(List<class Gadget> &GList)
{
  LONG topedge  = 0;
  LONG leftedge = 112;
  LONG width    = RPort->WidthOf() - leftedge;
  class VerticalGroup *topgroup;
  class Option *option;
  class Gadget *gadget;
  //
  // Create the vertical group containing all subgadgets.
  topgroup = new class VerticalGroup(GList,RPort,leftedge,0,width,RPort->HeightOf());
  width   -= 12;
  //
  // Add the title as a text gadget to this group.
  new class TextGadget(*topgroup,RPort,leftedge,topedge,width,12,Title);
  topedge += 12;
  //
  for(option = OptionList.First();option;option = option->NextOf()) {
    gadget = option->BuildOptionGadget(RPort,*topgroup,leftedge,topedge,width);
    if (gadget) {
      // Install the option this belongs to.
      gadget->UserPointerOf() = option;
      topedge = gadget->TopEdgeOf() + gadget->HeightOf();
    }
  }
  topgroup->Refresh();
}
///

/// Menu::GadgetTopic::HandleEvent
// Handle an even that is related/created by this topic. Return a boolean
// whose meaning/interpretation is up to the caller.
// This implementation here calls the corresponding callback of the option
// carried by the option.
bool Menu::GadgetTopic::HandleEvent(struct Event &ev)
{
  if (ev.Type == Event::GadgetUp) {
    class Gadget *gadget = ev.Object;
    if (gadget) {
      class Option *option = (class Option *)(gadget->UserPointerOf());
      // Check whether this option changed its value here.
      if (option) {
	if (option->ParseGadget()) {  	
  	  ev.Type      = Event::Ctrl;
	  ev.ControlId = MA_Prefs; // re-install preferences
	  return true;
	}
      }
    }
  }
  return false;
}
///

/// Menu::ControlTopic::ControlTopic
// The control function of the global menu. This is the topmost option
// we create by hand and which is not given by the preferences.
Menu::ControlTopic::ControlTopic(class RenderPort *rp,const char *loadname,const char *savename,
				 const char *loadstatename,const char *savestatename)
  : Topic("Prefs"), RPort(rp), LoadConfigFile(loadname), SaveConfigFile(savename),
    LoadStateFile(loadstatename), SaveStateFile(savestatename)
{
}
///

/// Menu::ControlTopic::CreateOptionGadgets
// This is currently still undefined and does nothing. Later.
void Menu::ControlTopic::CreateOptionGadgets(List<class Gadget> &GList)
{
  class VerticalGroup *top;
  LONG width  = RPort->WidthOf();
  LONG height = RPort->HeightOf();
  LONG le     = 112;
  LONG te     = 0;
  
  top = new class VerticalGroup(GList,RPort,le,0,width-le,height);
  //
  width -= 112+12;
  new class TextGadget(*top,RPort,le,te,width,12,"Emulator Control");
  te    += 12;
  new class SeparatorGadget(*top,RPort,le,te,width,4);
  te    += 4;
  //    
  //
  // Create some button gadgets 
  //
  ExitGadget      = new class ButtonGadget(*top,RPort,le,te,width,12,
				      "Exit Menu and Continue");
  te    += 12;
  WarmStartGadget = new class ButtonGadget(*top,RPort,le,te,width,12,
					   "Warm Start the Emulator");
  te    += 12;
  ColdStartGadget = new class ButtonGadget(*top,RPort,le,te,width,12,
					   "Cold Start the Emulator");
  te    += 12;
#ifdef BUILD_MONITOR
  MonitorGadget   = new class ButtonGadget(*top,RPort,le,te,width,12,
					   "Enter Monitor");
  te    += 12;
#endif
  QuitGadget      = new class ButtonGadget(*top,RPort,le,te,width,12,
					   "Exit Emulator");

  te    += 12;
  new class SeparatorGadget(*top,RPort,le,te,width,12);
  te    += 12;
  new class TextGadget(*top,RPort,le,te,width,12,"Load Configuration From");
  te    += 12;
  LoadGadget = new class FileGadget(*top,RPort,le,te,width,12,LoadConfigFile,false,true,false);

  te    += 12;
  new class SeparatorGadget(*top,RPort,le,te,width,12);
  te    += 12;
  new class TextGadget(*top,RPort,le,te,width,12,"Save Configuration As");
  te    += 12;
  SaveGadget = new class FileGadget(*top,RPort,le,te,width,12,SaveConfigFile,true,true,false);
  
  te    += 12;
  new class SeparatorGadget(*top,RPort,le,te,width,12);
  te    += 12;
  new class TextGadget(*top,RPort,le,te,width,12,"Load State From");
  te    += 12;
  LoadStateGadget = new class FileGadget(*top,RPort,le,te,width,12,LoadStateFile,false,true,false);

  te    += 12;
  new class SeparatorGadget(*top,RPort,le,te,width,12);
  te    += 12;
  new class TextGadget(*top,RPort,le,te,width,12,"Save State To");
  te    += 12;
  SaveStateGadget = new class FileGadget(*top,RPort,le,te,width,12,SaveStateFile,true,true,false);

  //
  top->Refresh();
}
///

/// Menu::ControlTopic::HandleEvent
// Handle an even that is related/created by this topic. Return a boolean
// whose meaning/interpretation is up to the caller.
// This implementation here calls the corresponding callback of the option
// carried by the option.
bool Menu::ControlTopic::HandleEvent(struct Event &ev)
{
  if (ev.Type == Event::GadgetUp) {
    class Gadget *gadget;
    // Ok, a gadget has been released. Check for which one this could be
    gadget = ev.Object;
    if (gadget == ExitGadget) {
      // The user hit the exit gadget. Hence, remove the menu and go back
      // to the emulator main loop.
      ev.Type      = Event::Ctrl;
      ev.ControlId = MA_Exit;
      return true;
    } else if (gadget == WarmStartGadget) {
      // Run a warm start of the emulator.
      ev.Type      = Event::Ctrl;
      ev.ControlId = MA_WarmStart;
      return true;
    } else if (gadget == ColdStartGadget) {
      // Run a cold start of the emulator.
      ev.Type      = Event::Ctrl;
      ev.ControlId = MA_ColdStart;
      return true;
    } else if (gadget == MonitorGadget) {
      // Enter the monitor of the emulator
      ev.Type      = Event::Ctrl;
      ev.ControlId = MA_Monitor;
      return true;
    } else if (gadget == QuitGadget) {
      // Quit the emulator completely
      ev.Type      = Event::Ctrl;
      ev.ControlId = MA_Quit;
      return true;
    } else if (gadget == LoadGadget) {
      // Load the configuration from a file
      ev.Type      = Event::Ctrl;
      ev.Object    = gadget;
      ev.ControlId = MA_Load;
      return true;
    } else if (gadget == SaveGadget) {
      // Save the configuration into a file
      ev.Type      = Event::Ctrl;
      ev.Object    = gadget;
      ev.ControlId = MA_Save;
      return true;
    } else if (gadget == LoadStateGadget) {
      // Load the machine state from a file
      ev.Type      = Event::Ctrl;
      ev.Object    = gadget;
      ev.ControlId = MA_LoadState;
      return true;
    } else if (gadget == SaveStateGadget) {
      // Save the machine state into a file
      ev.Type      = Event::Ctrl;
      ev.Object    = gadget;
      ev.ControlId = MA_SaveState;
      return true;
    }
  }
  return false;
}
///

/// Menu::ControlTopic::SetLoadFile
// Set the filename the preferences should be load from. This influences the
// contents of the gadget that keeps the filename.
void Menu::ControlTopic::SetLoadFile(const char *filename)
{
  LoadConfigFile = filename;
}
///

/// Menu::ControlTopic::SetSaveFile
// Set the contents of the filename that should be saved to. This also
// updates the gadget showing the contents.
void Menu::ControlTopic::SetSaveFile(const char *filename)
{
  SaveConfigFile = filename;
}
///

/// Menu::ControlTopic::SetLoadStateFile
void Menu::ControlTopic::SetLoadStateFile(const char *filename)
{
  LoadStateFile = filename;
  LoadStateGadget->SetContents(filename);
}
///

/// Menu::ControlTopic::SetSaveStateFile
void Menu::ControlTopic::SetSaveStateFile(const char *filename)
{
  SaveStateFile = filename;
  SaveStateGadget->SetContents(filename);
}
///
