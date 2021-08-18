/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: menu.hpp,v 1.54 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Definition of a graphical frontend with the build-in graphics
 **********************************************************************************/

#ifndef MENU_HPP
#define MENU_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "new.hpp"
#include "exceptions.hpp"
#include "list.hpp"
#include "configurable.hpp"
#include "directory.hpp"
#include "gadget.hpp"
#include "optioncollector.hpp"
///

/// Forwards
class Machine;
class AtariDisplay;
class OsROM;
class Keyboard;
class RenderPort;
///

/// Class Menu
// The menu class defines the graphical frontend for the user.
// This is a rather simple frontend that is capable of running
// with the resources of the emulator without requiring any
// additional packets.
class Menu : private OptionCollector {
  //
  class Keyboard   *Keyboard;
  //
  // Global actions we need to execute, either driver by the
  // preferences reloader or by the "control topic" directly
  // by the user. Unfortunately, we must put this into an int
  // as we cannot forward-declare an enum.
  enum MenuAction {
    MA_Nothing   = 0, // do nothing special
    MA_Comeback,      // high-frequency action required, come again immediately (keyboard)
    MA_Prefs,         // re-install preferences
    MA_WarmStart,     // warm-start the emulator
    MA_ColdStart,     // cold-start the emulator
    MA_Monitor,       // enter the system monitor
    MA_Load,          // load the preferences from
    MA_Save,          // save the preferences into
    MA_LoadState,     // load the machine state from
    MA_SaveState,     // save the machine state to
    MA_Exit,          // exit the menu
    MA_Quit           // quit the emulator
  };
  //
  // The control function of the global menu. This is the topmost topic
  // we create by hand and which is not given by the preferences.
  class ControlTopic : public Topic {
    //  
    // The render port we use for rendering into the buffer. Temporarely
    // created whenever we get a display to work in.
    class RenderPort      *RPort;
    //
    // Various gadgets we generate by hand and keep here for simple
    // comparisons.
    class Gadget     *ExitGadget;
    class Gadget     *WarmStartGadget;
    class Gadget     *ColdStartGadget;
    class Gadget     *MonitorGadget;
    class Gadget     *QuitGadget;
    class FileGadget *LoadGadget;
    class FileGadget *SaveGadget;
    class FileGadget *LoadStateGadget;
    class FileGadget *SaveStateGadget;
    //
    // Default definitions where to load from and where to load to.
    const char       *LoadConfigFile;
    const char       *SaveConfigFile;
    const char       *LoadStateFile;
    const char       *SaveStateFile;
    //
  public:
    ControlTopic(class RenderPort *rp,const char *loadname,const char *savename,
		 const char *loadstatename,const char *savestatename);
    virtual ~ControlTopic(void);
    //    
    // Create the right-hand menu of all options for the given
    // topic.
    virtual void CreateOptionGadgets(List<Gadget> &GList);
    //
    // Handle an even that is related/created by this topic. Return a boolean
    // whose meaning/interpretation is up to the caller.
    // This implementation here calls the corresponding callback of the option
    // carried by the option.
    virtual bool HandleEvent(struct Event &ev);
    //
    // Set the filename the preferences should be load from. This influences the
    // contents of the gadget that keeps the filename.
    void SetLoadFile(const char *filename);
    // Set the contents of the filename that should be saved to. This also
    // updates the gadget showing the contents.
    void SetSaveFile(const char *filename);
    // Set the file name for the state load gadget
    void SetLoadStateFile(const char *filename);
    // Ditto for the save method
    void SetSaveStateFile(const char *filename);
  };
  //
  // A topic represented by a couple of gadgets
  class GadgetTopic  : public OptionTopic {  
    // The render port we use for rendering into the buffer. Temporarely
    // created whenever we get a display to work in.
    class RenderPort      *RPort;
    //
  public:
    GadgetTopic(class RenderPort *rp,const char *title);
    virtual ~GadgetTopic(void);
    //
    // Create the right-hand menu of all options for the given
    // topic.
    virtual void CreateOptionGadgets(List<Gadget> &GList);  
    //
    // Handle an even that is related/created by this topic. Return a boolean
    // whose meaning/interpretation is up to the caller.
    // This implementation here calls the corresponding callback of the option
    // carried by the option.
    virtual bool HandleEvent(struct Event &ev);
  };
  //
  // List of all installed gadgets is kept here
  List<Gadget>           GList;
  //
  //
  // The event feeder mechanism uses this class
  class EventFeeder     *EventFeeder;
  //
  // Pointer to the topmost gadget that keeps all the topics
  class TopicGadget     *TopicGadget;
  //
  // Pointer to the topmost control topic
  class ControlTopic    *ControlTopic;
  //  
  // The render port we use for rendering into the buffer. Temporarely
  // created whenever we get a display to work in.
  class RenderPort      *RPort;
  //  
  // Name of the last topic that has been picked by the user
  // we keep it while we dispose the topic list for a machine
  // re-assignment
  char                  *LastTopic;
  //
  // Slider positions before the reset
  UWORD                  TopicProp,OptionProp;
  //
  // Load and save paths where the last preferences have been load from or
  // have been saved to.
  char                  *LoadFileName;
  char                  *SaveFileName;
  char                  *LoadStateName;
  char                  *SaveStateName;
  //
  // A virtual callback that must deliver an option topic. This
  // is here to allow a derived class to enhance the OptionTopic by
  // something smarter if required. This method is called to 
  // generate new topics on argument parsing.
  virtual class OptionTopic *BuildTopic(const char *title)
  {
    return new class GadgetTopic(RPort,title);
  }
  //
  // The following is a list of methods we implement as part of the 
  // argparser interface.
  //
  // Create all the gadgets required for the menu
  void CreateMenu(void);
  //
  // Delete all gadgets from the gadget list
  void DeleteGList(void);
  //
  // Refresh the GUI after a possible warning operation
  // by re-drawing everything.
  void RefreshGUI(void);
  //
  // Relaunch the GUI by building the GUI engine
  void RestartGUI(void);
  //
  // Clear all members that are required by the GUI
  void DisposeGUI(void);
  //  
  // Use all the configurables and collect all the topics we can get hands of.
  // This builds all the menu structures for the user interface.
  // This overloads the call of the OptionCollector
  void CollectTopics(void);
  //
  // Draw a nice title screen into the free area.
  void DrawTitle(class RenderPort *rp,LONG dx,LONG dy,int angle);
  // 
  // Save options to a named file
  void SaveOptions(const char *filename);
  //
  // Load options from a named file
  void LoadOptions(const char *filename);
  //
  // Save the machine state to a named file
  void SaveState(const char *name);
  //
  // Load the machine state from a named file
  void LoadState(const char *name);
  // 
  // Accept an option change that might have been generated
  // by the user menu
  void AcceptOptionChange(void);
  //
public:
  //
  // Create the menu. 
  Menu(class Machine *mach);
  virtual ~Menu(void);
  //
  // Launch the menu from the outside.
  void EnterMenu(void);
};
///

///
#endif
