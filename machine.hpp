/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: machine.hpp,v 1.77 2015/12/11 16:27:36 thor Exp $
 **
 ** In this module: Machine/Architecture specific settings
 **********************************************************************************/

#ifndef MACHINE_HPP
#define MACHINE_HPP

/// Includes
#include "types.h"
#include "types.hpp"
#include "list.hpp"
#include "argparser.hpp"
#include "configurable.hpp"
#include "exceptions.hpp"
#include "vbiaction.hpp"
#include "hbiaction.hpp"
#include "cycleaction.hpp"
#include <stdarg.h>
///

/// Type definitions for the machine type
enum Machine_Type {
  Mach_None,      // should not appear
  Mach_Atari800,  // Atari400 or Atari800
  Mach_Atari1200, // Atari1200XL
  Mach_AtariXL,   // Atari600XL or Atari800XL
  Mach_AtariXE,   // Atari130XE
  Mach_5200       // Atari5200
};
///

/// Forward references
// Various chip classes that are linked in here
class Chip;
class CPU;
class GTIA;
class ParPort;
class Pokey;
class YConnector;
class PIA;
class Antic;
class CartCtrl;
class MMU;
class GameController;
class Keyboard;
class SIO;
class Sound;
class AtariDisplay;
class Monitor;
class RAM;
class OsROM;
class BasicROM;
class CartROM;
class Atari;
class GamePort;
class Timer;
class PatchProvider;
class DeviceAdapter;
class Patch;
class IRQSource;
class AnalogJoystick;
class DigitalJoystick;
class SDLAnalog;
class SDLDigital;
class Menu;
class Saveable;
class SDL_Port;
class Printer;
class InterfaceBox;
class Tape;
class WarningRequester;
class ErrorRequester;
class AtariSIOPort;
class TitleMenu;
class KeyboardStick;
///

/// Class Machine
// The only purpose of this class is to keep
// the machine type of the machine we are using.
// Unfortunately, we cannot config this by the 
// configchain itself.
class Machine {
  //
  // Chain of all configurable modules in here.
  List<Configurable>    configChain;
  // Chain of all chips
  List<Chip>            chipChain;
  // Chain of all VBI activities
  List<VBIAction>       vbiChain;
  // Chain of all HBI activities
  List<HBIAction>       hbiChain;
  // Chain of all CPU activities
  List<CycleAction>     cycleChain;
  // Chain of all gameports. These classes are the input generators
  // for game port devices like paddles, joysticks or lightpens
  List<GamePort>        gamePortChain;
  // Chain of all installed patches so we know what to run if we
  // hit an escape code
  List<PatchProvider>   patchProviderChain;
  // Chain of all possible input sources for the CPU IRQ line
  List<IRQSource>       irqChain;
  // Chain of all snapshot-able objects
  List<Saveable>        snapshotChain;
  //
  // Type of the machine here
  Machine_Type          machtype;
  // Type of the frontend
  enum FrontEnd_Type {
    Front_X11,   // classical X11 frontend
    Front_SDL,   // simple direct media frontend
    Front_Curses,// character based output
    Front_None   // no front-end
  }                     fronttype;
  // Type of the sound method
  enum Sound_Type {
    Sound_HQ,     // high quality Oss sound
    Sound_Oss,    // Oss sound generation thru /dev/dsp
    Sound_Wav,    // sound recoring thru wav
    Sound_SDL,    // Sound output thru SDL
    Sound_Alsa,   // Sound output by Alsa
    Sound_DirectX // Sound output over - urgs - M$ DirectX
  }                     soundtype;
  //
  // SDL frontend interface class goes here if we have SDL.
  class SDL_Port        *sdlport;
  //
  // Pointers to all custom chips here
  class CPU             *cpu;
  class GTIA            *gtia;
  class Pokey           *pokey;
  class Pokey           *leftpokey;   // in case we have two
  class YConnector      *pokeybridge; // need this to combine the two.
  class PIA             *pia;
  class Antic           *antic;
  class CartCtrl        *cartctrl;
  class RAM             *ram;
  class OsROM           *osrom;
  class BasicROM        *basicrom;
  class CartROM         *cartrom;
  class MMU             *mmu;
  class Monitor         *monitor;
  class Menu            *menu;
  class TitleMenu       *quickmenu;
  class Atari           *atari;
  //
  // Pointers to all input classes
  class Keyboard        *keyboard;
  class GameController  *joysticks[4];
  class GameController  *paddles[8];
  class GameController  *lightpen;
  class SIO             *sio;
  class Sound           *sound;
  class AtariDisplay    *display;
  class AtariDisplay    *xepdisplay;
  class Printer         *printer;
  class Tape            *tape;
  class InterfaceBox    *serial;
  class AtariSIOPort    *sioport;
  class AnalogJoystick  *analogjoysticks[8];
  class DigitalJoystick *digitaljoysticks[8];
  class SDLAnalog       *sdlanalog[8];
  class SDLDigital      *sdldigital[8];
  class KeyboardStick   *keypadstick;
  //
  // Global argument values (from the command line and the configuration files)
  class ArgParser       *globalargs;
  //
  // The warning requester that collects warnings (and other problem cases)
  class WarningRequester *warninglog;
  class ErrorRequester   *errorlog;
  //
  // The next free Escape code
  UBYTE                  escCode;
  //
  // The following is set to true when exiting the emulator.
  bool                   quit;
  bool                   reset;
  bool                   coldstart;
  bool                   pause;
  bool                   launchmonitor;
  bool                   launchmenu;
  bool                   monitoroncrash;
  bool                   acceptlicence;
  bool                   stereopokey;
  bool                   enablexep;
  bool                   nogfx;
  bool                   noerrors;
  bool                   nowarnings;
  //
  // Parse the configuration options for the overall menu here.
  void ParseConfig(class ArgParser *args);
  //
public:
  Machine(void);
  ~Machine(void);
  //
  // Allocate all components of the machine given the global
  // arguments. We *must not* yet parse them here, but keep them
  // for later reference.
  void BuildMachine(class ArgParser *args);
  //
  // Link a configurable element into the machine
  List<Configurable> &ConfigChain(void)
  {
    return configChain;
  }
  //
  // Link a chip into the machine
  List<Chip> &ChipChain(void)
  {
    return chipChain;
  }
  //
  // Link a VBI into the machine
  List<VBIAction> &VBIChain(void)
  {
    return vbiChain;
  }
  //
  // Link a HBI into the machine
  List<HBIAction> &HBIChain(void)
  {
    return hbiChain;
  }
  //
  // Return the list of all CPU cycle specific operations.
  List<CycleAction> &CycleChain(void)
  {
    return cycleChain;
  }
  //
  // Link into the game port chain
  List<GamePort> &GamePortChain(void)
  {
    return gamePortChain;
  }
  //
  // Link into the list of classes that provide patches
  List<PatchProvider> &PatchList(void)
  {
    return patchProviderChain;
  }
  //
  // Link into the list of available IRQ sources
  List<IRQSource> &IRQChain(void)
  {
    return irqChain;
  }
  //
  // Link into the list of available snapshoting classes
  List<Saveable> &SaveableChain(void)
  {
    return snapshotChain;
  }
  //
  // Return the machine type
  Machine_Type MachType(void) const 
  {
    return machtype;
  }
  //
  // Parse off arguments: Must be called not before
  // the config chain is complete;
  // This also configures all the remaining chips.
  // You may pass NULL in here meaning "install the command line
  // default arguments". The result code is the "reparse" flag
  // of the argument parser.
  ArgParser::ArgumentChange ParseArgs(class ArgParser *args);
  //
  // Write all available state definitions into a named file
  // throw in case of an error
  void WriteStates(const char *file);
  //
  // Read the state machine from a file, throw in case of an error.
  void ReadStates(const char *file);
  //
  // This is called as soon as the emulator crashes by
  // an unknown or unstable opcode
  void Crash(UBYTE opcode);
  //
  // This is called as soon as the emulator executed
  // a JAM (crash and never return) opcode
  void Jam(UBYTE opcode);
  //
  // Enter the monitor by the front gate
  void EnterMonitor(void);
  //
  // Run the quick menu in the title bar
  void QuickMenu(void);
  //
  // Enter the user menu. Only the atari class should do this.
  void EnterMenu(void);
  //
  // Handle a ^C event in whatever way it is appropriate
  void SigBreak(void);
  //
  // Return a pointer to the SDL port in case we have one, i.e.
  // SDL is available.
  class SDL_Port *SDLPort(void) const
  {
    return sdlport;
  }
  //
  // Return the Atari SIO Port for connecting external 
  // "true atari" devices to this emulator
  class AtariSIOPort *SioPort(void) const
  {
    return sioport;
  }
  //
  // Return pointers to all custom chips here
  // Pointers to all custom chips here
  class GTIA *GTIA(void) const
  {
    return gtia;
  }
  //
  // If we have two pokeys, return the given one
  // or NULL. Default is the default pokey.
  class Pokey *Pokey(int n = 0) const
  {
    return n?leftpokey:pokey;
  }
  // If we have two pokeys, they cannot get
  // mapped into memory directly, but are rather
  // connected by a "bridge" that distinguishes
  // which access goes to which pokey. This call
  // returns for the MMU the page that should go
  // into the pokey area.
  class Page *PokeyPage(void) const;
  //
  class PIA *PIA(void) const
  {
    return pia;
  }
  //
  class Antic *Antic(void) const
  {
    return antic;
  }
  //
  class CartCtrl *CartCtrl(void) const
  {
    return cartctrl;
  }
  //
  class MMU *MMU(void) const
  {
    return mmu;
  }
  //
  class CPU *CPU(void) const
  {
    return cpu;
  }
  //
  class RAM *RAM(void) const
  {
    return ram;
  }
  //
  class OsROM *OsROM(void) const
  {
    return osrom;
  }
  //
  class BasicROM *BasicROM(void) const
  {
    return basicrom;
  }
  //
  class CartROM *CartROM(void) const
  {
    return cartrom;
  }
  //
  class Atari *Atari(void) const
  {
    return atari;
  }
  //
  // Return various input classes
  class SIO *SIO(void) const
  {
    return sio;
  }
  //
  class Sound *Sound(void) const
  {
    return sound;
  }
  //
  // Return the display of the emulator. Note that this pointer may
  // change(!) within the VBI (because the user changed it by the
  // configuration menu)
  class AtariDisplay *Display(void) const
  {
    return display;
  }
  //
  // Return the display responsible for the 80 character output
  class AtariDisplay *XEPDisplay(void) const
  {
    return xepdisplay;
  }
  //
  class Keyboard *Keyboard(void) const
  {
    return keyboard;
  }
  //
  class Printer *Printer(void) const
  {
    return printer;
  }
  //
  class Tape *Tape(void) const
  {
    return tape;
  }
  //
  class InterfaceBox *InterfaceBox(void) const
  {
    return serial;
  }
  //
  class GameController *Joystick(int stick) const
  {
    return joysticks[stick];
  }
  //
  class GameController *Paddle(int pot) const
  {
    return paddles[pot];
  }
  //
  class GameController *Lightpen(void) const
  {
    return lightpen;
  }
  //
  class KeyboardStick *KeypadStick(void) const
  {
    return keypadstick;
  }
  //
  class Monitor *Monitor(void) const
  {
    return monitor;
  }
  //
  // This one is privately for the CPU: Emulate an escape code by
  // going thru all patches.  
  // It is called by the CPU code to respond to an ESCape / JAM code 
  // to execute emulator specific instructions
  void Escape(UBYTE code);
  // 
  // Allocate N escape codes, return the next available code.
  UBYTE AllocateEscape(UBYTE count);
  //
  // Machine management functions
  //
  // Coldstart all of the system
  void ColdStart(void);
  //
  // Warmstart the system
  void WarmStart(void);
  //
  // Quit the emulator, resp. get the quit status.
  bool &Quit(void)
  {
    return quit;
  }
  // Issue a keyboard click on the "warm reset" button. Depending on the model,
  // various things could happen as the Atari800 and Atari400 handle these keys
  // quite different.
  void WarmReset(void);
  //
  bool &Reset(void)
  {
    return reset;
  }
  bool &ColdReset(void)
  {
    return coldstart;
  }
  bool &Pause(void)
  {
    return pause;
  }
  bool &LaunchMonitor(void)
  {
    return launchmonitor;
  }
  bool &LaunchMenu(void)
  {
    return launchmenu;
  }
  //
  // Returns true in case a graphical interface is available.
  bool hasGUI(void) const
  {
    return !nogfx;
  }
  //
  // Print a warning message
  void PutWarning(const char *fmt,...) PRINTF_STYLE;
  // with a vararg-listing.
  void VPutWarning(const char *fmt,va_list args);
  // Display a true failure, return an indicator of type
  // ErrorRequester::ErrorAction what to do about it.
  int PutError(const class AtariException &e);
  //
  //
  // The window title to be used 
  const char *WindowTitle(void)
  {
    return PACKAGE_STRING "(c) THOR Software";
  }
  //
  // Run all emulator specific VBI activity. If "quick" is set, 
  // then we'd better finish quickly as we're already late.
  // Timer is the time base that runs out as soon as the VBI
  // is over.
  void VBI(class Timer *time,bool quick,bool p = false)
  {
    class VBIAction *vbi;
    // Set the forwarded pause flag if we're globally pausing
    // and not just emulating (as the monitor does)
    if (pause) p = true;
    
    for(vbi = vbiChain.First();vbi;vbi = vbi->NextOf()) {
      vbi->VBI(time,quick,p);
    }
  }
  //
  // Run all emulator specific HBI activity
  void HBI(void)
  {  
    class HBIAction *hbi;
  
    for(hbi = hbiChain.First();hbi;hbi = hbi->NextOf()) {
      hbi->HBI();
    }
  }
  //
  // Run the next CPU cycle. Hmm. How much does this cost?
  void Step(void)
  {
    class CycleAction *cycle;

    for(cycle = cycleChain.First();cycle;cycle = cycle->NextOf()) {
      cycle->Step();
    }
  }
  //
  // Scale the given frequency for the selected refresh rate if
  // a custom refresh rate has been selected. This is required for
  // proper audio generation.
  int ScaleFrequency(int freq);
  //
  // Check whether the user wants to accept the licence conditions.
  bool CheckLicence(void);
  //
  // Rebuild the display manually from the user interface
  void RefreshDisplay(void);
  //
  // Return true in case this is an NTSC machine.
  bool isNTSC(void) const;
};
///

///
#endif
