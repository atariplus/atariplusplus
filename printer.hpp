/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: printer.hpp,v 1.16 2014/03/16 14:54:08 thor Exp $
 **
 ** In this module: Support for printer output.
 **********************************************************************************/

#ifndef PRINTER_HPP
#define PRINTER_HPP

/// Includes
#include "types.hpp"
#include "serialdevice.hpp"
#include "vbiaction.hpp"
#include "stdio.hpp"
///

/// Class Printer
// This class emulates output to the printer by the SIO emulation layer,
// or by a CIO patched in handler from a separate program. For that,
// it pushes output into the printing command after a specific
// timeout.
class Printer : public SerialDevice, public VBIAction {
  //
  // The following is a list of lines we buffered for printing
  struct PrintNode {
    struct PrintNode *Next;  // singly linked list
    UBYTE            *Data;  // data within this node
    size_t            Size;  // number of bytes
    //
    PrintNode(void)
      : Next(NULL), Data(NULL), Size(0)
    { };
    //
    ~PrintNode(void)
    {
      delete[] Data;
    }
  }                  *PrintBuffer,*LastPrintNode;
  //
  // We also need a timer that runs out as soon as we need to flush this
  // buffer.
  class Timer        *FlushTimer;
  //
  // The output file if it is open.
  FILE               *OutputFile;
  //
  // size of the current line we expect from SIO
  int                 LineSize;
  //
  //
  // Preferences
  bool                TransposeEOL;  // translate EOL->LF?
  bool                PrinterOn;     // printer on/off flag
  bool                PrintToFile;   // print to files instead into a command?
  char               *PrintCommand;  // the shell command used for printing
  char               *PrintFileName; // the file we print into
  bool                AppendToFile;  // if set, append at the end of the output file
  LONG                FlushDelay;    // delay in seconds when the printing starts
  //
  // Private methods:
  void CleanQueue(void); // clean the printer queue
  bool PrintQueue(void); // print out the printer queue
  // Restart the timer event for the printer queue flush.
  void RestartTimer(void);
  //
  // The following method must be called periodically to
  // flush the printer buffer once a while:
  void PeriodicPrinter(void);
  //
  // Implement the VBI method of the VBIAction interface
  virtual void VBI(class Timer *,bool quick,bool)
  {
    if (quick == false) {
      PeriodicPrinter();
    }
  }
  //
public:
  // Constructor and destructor
  Printer(class Machine *mach);
  ~Printer(void);
  //
  // The following methods come from the SerialDevice interface
  // and have to be implemented in order to make this work:
  // Check whether this device accepts the indicated command
  // as valid command, and return the command type of it.
  virtual SIO::CommandType CheckCommandFrame(const UBYTE *CommandFrame,int &datasize,
					     UWORD speed);
  //
  // Read bytes from the device into the system. Returns the number of
  // bytes read.
  virtual UBYTE ReadBuffer(const UBYTE *CommandFrame,UBYTE *buffer,
			   int &datasize,UWORD &delay,UWORD &speed);
  //  
  // Write the indicated data buffer out to the target device.
  // Return 'C' if this worked fine, 'E' on error.
  virtual UBYTE WriteBuffer(const UBYTE *CommandFrame,const UBYTE *buffer,
			    int &datasize,UWORD &delay,UWORD speed);
  //
  // Execute a status-only command that does not read or write any data except
  // the data that came over AUX1 and AUX2
  virtual UBYTE ReadStatus(const UBYTE *CommandFrame,UWORD &delay,UWORD &speed);
  //
  // Other methods imported by the SerialDevice class:
  //
  // ColdStart and WarmStart
  virtual void ColdStart(void);
  virtual void WarmStart(void);
  //
  // Methods inherited from the Configurable class.
  //
  // Argument parser stuff: Parse off command line args
  virtual void ParseArgs(class ArgParser *args);
  //
  // Status display
  virtual void DisplayStatus(class Monitor *mon);
  //
  // Print a character array over the printer, possibly
  // substituting EOLs by EOFs. This is here to
  // make life a bit simpler for the CIO emulation
  // layer on top.
  bool PrintCharacters(UBYTE *buffer,int size);
  //
  // Turn the printer on or off.
  void SwitchPower(bool onoff);
  //
};
///

///
#endif
