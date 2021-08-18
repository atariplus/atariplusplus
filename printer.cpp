/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: printer.cpp,v 1.40 2021/08/16 10:31:01 thor Exp $
 **
 ** In this module: Support for printer output.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "printer.hpp"
#include "timer.hpp"
#include "argparser.hpp"
#include "monitor.hpp"
#include "string.hpp"
#include "stdlib.hpp"
#include "stdio.hpp"
#include "new.hpp"
#if HAVE_UNISTD_H && HAVE_WORKING_FORK
#define USE_PRINTER
#include "unistd.hpp"
#endif
///

/// Printer::Printer
// Setup the printer interface here.
Printer::Printer(class Machine *mach)
  : SerialDevice(mach,"Printer",'@'), VBIAction(mach),
    PrintBuffer(NULL), LastPrintNode(NULL), FlushTimer(NULL), OutputFile(NULL),
    LineSize(40), TransposeEOL(true), PrinterOn(true), PrintToFile(false),
    PrintCommand(new char[4]), PrintFileName(NULL), AppendToFile(false), FlushDelay(5)
{ 
  // Just use the usual here....
  strcpy(PrintCommand,"lpr");
#ifdef USE_PRINTER
  PrintToFile = false;
#else
  PrintToFile = true;
#endif
}
///

/// Printer::~Printer
// Cleanup the printer. Dispose the timer and the print buffer.
Printer::~Printer(void)
{
  //
  // cleanup the Printer Queue 
  CleanQueue();
  //
  if (OutputFile) {
    fclose(OutputFile);
    OutputFile = NULL;
  }
  //
  // Dispose the timer
  delete   FlushTimer;
  delete[] PrintCommand;
  delete[] PrintFileName;
}
///

/// Printer::ColdStart
// Run a printer ColdStart.
void Printer::ColdStart(void)
{
  WarmStart();
}
///

/// Printer::WarmStart
// Run a printer WarmStart
// Actually, a printer wouldn't notice a warmstart
// but for convenience, we flush the printer
// buffer here.
void Printer::WarmStart(void)
{
  CleanQueue();
}
///

/// Printer::CleanQueue
// Clean the printer queue, i.e. forget all
// its contents.
void Printer::CleanQueue(void)
{
  struct PrintNode *node,*next;
  //
  next = PrintBuffer;
  while ((node = next)) {
    next = node->Next;
    delete node;
  }
  PrintBuffer   = NULL;
  LastPrintNode = NULL;
}
///

/// Printer::PrintQueue
// Print the current printer queue with the
// selected print command
// Returns a success/failure indicator (we may ignore)
bool Printer::PrintQueue(void)
{
  int failure = 0;
  //    
  // If there is nothing to print, ignore.
  if (PrintBuffer == NULL)
    return true;
  //
  // Check whether we print into a file or into a command
  if (PrintToFile) {
    // We are printing to a file. Open the file and write
    // the queue contents into it line by line.
    if (PrintFileName && *PrintFileName) {
      OutputFile = fopen(PrintFileName,(AppendToFile)?("a"):("w")); // need to translate text/lf according to the host.
      if (OutputFile == NULL)
	failure = errno;
    }
    if (OutputFile) {	
      struct PrintNode *node;
      node = PrintBuffer;
      while(node) {
	size_t size = node->Size;
	if (fwrite(node->Data,1,size,OutputFile) != size) {
	  // A problem. Huh!
	  failure = errno;
	  break;
	}
	node = node->Next;
      }
      // Done with writing. Close the output file.
      fclose(OutputFile);
      OutputFile = NULL;
    }
  } else {
#ifdef USE_PRINTER
    int pipefiles[2];
    //
    // We build up a pipe, pipe the string into one end and run
    // lpr on the other side.
    if (pipe(pipefiles) == 0) {
      int pid;
      // fork the current process to generate the printing spooler
      pid = fork();
      if (pid == 0) {
	errno = 0;
	// Close the writing end such that we get an EOF on the
	// reading end.
	close(pipefiles[1]);
	// We're here in the child process. Okey, use the reading
	// end of the pipe = pipefile[0] as stdin for this process
	if (close(0) == 0) {
	  // stdin closing worked fine, now use dup() to connect
	  // the stdin with the pipe file. This must return 0 = stdin
	  // provided everything works fine.
	  if (dup(pipefiles[0]) == 0) {
	    // Run now execlp on the print command. Note that this
	    // is a no-return call (intentionally). We don't need
	    // any arguments here.
	    // The first argument should be the command name by "tradition"
	    execlp(PrintCommand,PrintCommand,NULL);
	  }
	}
	if (errno)
	  fprintf(stderr,"%s failed: %s\n",PrintCommand,strerror(errno));
	// We haven't opened the pipe, so we don't close it either.
	// Question is what can we do now? Let's hope Linux
	// releases all the memory for us here.
	exit(25);
      } else if (pid > 0) {
	struct PrintNode *node;
	// We are here the parent process. Write all the print data into
	// the writing side of the pipe.
	close(pipefiles[0]); // close the reading end. We write into the writing end now.
	node = PrintBuffer;
	while(node) {
	  size_t size = node->Size;
	  if (write(pipefiles[1],node->Data,size) != (ssize_t)size) {
	    // A problem. Huh!
	    failure = errno;
	    break;
	  }
	  node = node->Next;
	}
	// Done with writing.
      } else {
	// Forking failed.
	failure = errno;
      }
      // Close now both sides of the pipe. Note that the forked 
      // process got a copy of our side of the pipe. We do not
      // care about possible errors here. What could we do about them
      // either?
      close(pipefiles[1]);
    } else {
      failure = errno;
    }
#endif
    //
  }
  // In either case: Flush the buffer and do not attempt to
  // print it again.
  CleanQueue();
  //
  if (failure) {
    machine->PutWarning("Printer output failed because : %s\n",strerror(failure));
    return false;
  }
  return true;
}
///

/// Printer::RestartTimer
// Restart the timer event for the printer queue flush.
void Printer::RestartTimer(void)
{
  if (FlushTimer == NULL) {
    // Build a timer class in case we do not yet have one.
    FlushTimer = new class Timer;
  }
  //
  // Restart the timer in the given delay.
  FlushTimer->StartTimer(FlushDelay,0);
}
///

/// Printer::PeriodicPrinter
// The following method must be called periodically to
// flush the printer buffer once a while:
void Printer::PeriodicPrinter(void)
{
  // If we have a timer, check whether time is up to
  // print this out.
  if (FlushTimer) {
    if (FlushTimer->EventIsOver()) {
      PrintQueue(); // print the queue out now.
      // Do not ask again until we need it.
      delete FlushTimer;
      FlushTimer = NULL;
    }
  }
}
///

/// Printer::CheckCommandFrame
// Check a SIO command frame for a valid
// command and return the command type
SIO::CommandType Printer::CheckCommandFrame(const UBYTE *commandframe,int &datasize,UWORD speed)
{
  //
  // If the printer is not turned on, return an error here
  if (!PrinterOn || speed != SIO::Baud19200)
    return SIO::Off;
  //
  // Otherwise, check the command character
  switch(commandframe[1]) {
  case 'S':
    // Status command. This returns four bytes in size
    datasize = 4;
    return SIO::ReadCommand;
  case 'W':    
    // The size of the write buffer depends on the write mode.
    // The ATARI printer is able to write rotated characters
    // if AUX1 = 83 = 'S'(ideways) : 29 characters
    // if AUX1 =      'D'(double)  : 20 characters
    // if AUX1 =      'N'(ormal)   : 40 characters
    switch(commandframe[2]) {
    case 'N':
      datasize = 40;
      break;
    case 'S':
      datasize = 29;
      break;
    case 'D':
      datasize = 20;
      break;
    default:
      // Brzrk! If anything goes wrong, assume 40.
      datasize = 40;
      break;
    }
    // Keep this as the line size
    LineSize = datasize;
    return SIO::WriteCommand;
  }
  // Nothing else known here.
  return SIO::InvalidCommand;
}
///

/// Printer::ReadBuffer
// Fill a buffer by a read command, return the amount of
// data read back (in bytes), not counting the checksum
// byte which is computed for us by the SIO.
// Prepare the indicated command. On read, read the buffer. On
// write, just check whether the target is write-able. Returns
// the size of the buffer (= one sector). On error, return 0.
UBYTE Printer::ReadBuffer(const UBYTE *commandframe,UBYTE *buffer,int &,UWORD &,UWORD &speed)
{
 
  switch (commandframe[1]) {
  case 'S':
    // There is only the status command, and I've no idea what I should
    // return here...
    buffer[0] = 1;
    buffer[1] = 0;
    buffer[2] = 1;
    buffer[3] = 0;
    speed     = SIO::Baud19200;
    return 'C';
  }  

  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
  return 0;
}
///

/// Printer::WriteBuffer
// Execute a write command to the printer->Print some characters
// Return a SIO error indicator
UBYTE Printer::WriteBuffer(const UBYTE *commandframe,const UBYTE *buffer,
			   int &datasize,UWORD &,UWORD)
{
  const UBYTE *p = NULL;
  struct PrintNode *node;
  int size = datasize;
  
  switch (commandframe[1]) { 
  case 'W':
    // The one and only known write command
    if (size != LineSize) {
      // This cannot work!
      return 'E';
    }
    // Now check the real size: The first EOL terminates the line!
    p = (const UBYTE *)memchr(buffer,0x9b,size);
    if (p) {
      size = int(p - buffer + 1);
    }
    // Add a print node for the printer now, and a buffer.
    node       = new struct PrintNode;
    node->Data = new UBYTE[size];
    node->Size = size;
    memcpy(node->Data,buffer,size);
    //
    if (p && TransposeEOL) {
      // Here: Translate an EOL to a LF. 
      node->Data[p - buffer] = '\n';
    }
    //
    // Link it into the buffer as a FIFO
    if (LastPrintNode) {
      LastPrintNode->Next = node;
    } else {
      PrintBuffer         = node;
    }
    //
    // This is now the last node in the list
    LastPrintNode         = node;
    // And restart the printer timer.
    RestartTimer();
    // Worked!
    return 'C';
  }
  // 
  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
  // No other command is known
  return 'E';
}
///

/// Printer::ReadStatus
// Execute a status-only command that does not read or write any data except
// the data that came over AUX1 and AUX2
// As the printer knows no status-only commands, nothing happens here
// except an error. This should actually never be called as we never
// signal a status command
UBYTE Printer::ReadStatus(const UBYTE *commandframe,UWORD &,UWORD &)
{
  machine->PutWarning("Unknown command frame: %02x %02x %02x %02x\n",
		      commandframe[0],commandframe[1],commandframe[2],commandframe[3]);
    
  return 'N';
}
///

/// Printer::PrintCharacters
// Print a character array over the printer, possibly
// substituting EOLs by EOFs. This is here to
// make life a bit simpler for the CIO emulation
// layer on top.
bool Printer::PrintCharacters(UBYTE *buffer,int size)
{
  UBYTE *p;

  if (!PrinterOn)
    return false;

  if (buffer && size) {
    struct PrintNode *node;
    // Add a print node for the printer now, and a buffer.
    node       = new struct PrintNode;
    node->Data = new UBYTE[size];
    node->Size = size;
    memcpy(node->Data,buffer,size);
    // Link it into the buffer as a FIFO
    if (LastPrintNode) {
      LastPrintNode->Next = node;
    } else {
      PrintBuffer         = node;
    }
    LastPrintNode         = node;
    //
    // substitute EOL -> LF possibly.
    if (TransposeEOL) {
      while((p = (UBYTE *)memchr(node->Data,0x9b,size)))
	*p = '\n';
    }
    //
    // Now launch the printer timer.
    RestartTimer();
  }
  
  return true;
}
///

/// Printer::SwitchPower
// Turn the printer on or off.
void Printer::SwitchPower(bool onoff)
{
  if (onoff) {
    // Simply turn it on.
    PrinterOn = true;
  } else {
    // Flush the buffer and let it go.
    CleanQueue();
    delete FlushTimer;
    FlushTimer = NULL;
    PrinterOn = false;
  }
}
///

/// Printer::ParseArgs
// Argument parser stuff: Parse off command line args
void Printer::ParseArgs(class ArgParser *args)
{  
  static const struct ArgParser::SelectionVector printtypevector[] = 
    { 
#ifdef USE_PRINTER
      {"ToSpoolCommand", false},
#endif
      {"ToFile"        , true},
      {NULL            , 0}
    };
  bool printeron   = PrinterOn;
  LONG printtarget = PrintToFile;
  
  args->DefineTitle("Printer");  
  args->DefineBool("EnablePrinter","turn the printer on or off",printeron);
  args->DefineSelection("PrintTarget","define where printer output goes",printtypevector,printtarget);
  args->DefineString("PrintCommand","define the printing command",PrintCommand);
  args->DefineFile("PrintFile","define the file to print to",PrintFileName,true,true,false);
  args->DefineBool("TransposeEOL","transpose Atari EOL to linefeed",TransposeEOL);
  args->DefineLong("FlushDelay","set the printer queue flush delay",0,60,FlushDelay);
  args->DefineBool("AppendToPrintFile","append new data at end of print file",AppendToFile);
  //
  PrintToFile = (printtarget)?(true):(false);
  SwitchPower(printeron);
}
///

/// Printer::Display
// Status display
void Printer::DisplayStatus(class Monitor *mon)
{
  mon->PrintStatus("Printer Status:\n"
		   "\tPrinter output queue is: %s\n"
		   "\tPrinter is             : %s\n"
		   "\tTranspose EOL->LF      : %s\n"
		   "\tFlush delay is         : " ATARIPP_LD "sec\n"
		   "\tPrint command is       : %s\n"
		   "\tPrint target file is   : %s\n"
		   "\tPrinting into          : %s\n"
		   "\tAppend to output file  : %s\n",
		   (LastPrintNode)?("full"):("empty"),
		   (PrinterOn)?("on"):("off"),
		   (TransposeEOL)?("on"):("off"),
		   FlushDelay,
		   (PrintCommand)?(PrintCommand):(""),
		   (PrintFileName)?(PrintFileName):(""),
		   (PrintToFile)?("file"):("command"),
		   (AppendToFile)?("yes"):("no")
		   );
}
///
