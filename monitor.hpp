/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: monitor.hpp,v 1.47 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Definition of the built-in monitor
 **********************************************************************************/

#ifndef MONITOR_HPP
#define MONITOR_HPP

/// Includes
#include "types.hpp"
#include "list.hpp"
#include "stdlib.hpp"
#include "stdio.hpp"
#include "string.hpp"
#include "new.hpp"
#include <stdarg.h>
///

/// Class Monitor
// This class implements the built-in monitor for the
// Atari++ emulator.
class Monitor {
  struct Command;          // forward reference
  friend struct   Command; // we need access for here
  //
  //
  class Machine  *machine;
  class CPU      *cpu;
  class MMU      *MMU;
  class AdrSpace *cpuspace,*anticspace,*currentadr;
  class DebugAdrSpace *debugspace;
  //
  // The log file for the output tracing, if possible.
  FILE           *tracefile;
  //
  // Symbol database for symbolic debugging.
  struct Symbol {
    //
    // The next symbol.
    struct Symbol *next;
    //
    // The type of the symbol
    enum Type {
      Equate,      // a symbolic equate
      Label,       // an absolute address
      Any,         // Only for matching: do not care
      PreferLabel, // Prefer a label if exists
      PreferEquate
    }              type;
    //
    // The size of a label: 16 bit or 8 bit.
    enum Size {
      ZeroPage,   // within the z-page
      Absolute,   // the full 16-bit range.
      All,        // only for matching: do not care
      PreferZeroPage,
      PreferAbsolute
    }              size;
    //
    enum {
      MaxLabelSize = 64
    };
    //
    // The name of a label.
    char           name[MaxLabelSize];
    //
    // And the address itself.
    UWORD          value;
    //
    // Read a label from a line of a file. Returns true on
    // a successful parse.
    bool ParseLabel(char *line);
    //
    // Find a label by its address, size and type.
    static const struct Symbol *FindSymbol(const struct Symbol *list,
					   UWORD address,Type t,Size s);
    //
    // Find a label by its name. Try to find the symbol that is closest
    // to the given PC value.
    static const struct Symbol *FindSymbol(const struct Symbol *list,
					   const char *name,UWORD pc,Type t,Size s);
    //
    // Construct an empty label.
    Symbol(void)
    : next(NULL)
    { }
    //
    // Copy a label from another label.
    Symbol(const Symbol &o)
    : next(NULL), type(o.type), size(o.size), value(o.value)
    {
      memcpy(name,o.name,sizeof(name));
    }
  }              *symboltable;
  //
  //
  // curses output window
  struct CursesWindow {
    // The window for the ncurses output
    // We cannot include curses since it conflicts with other (joystick) includes
    void         *window;
    //
    CursesWindow(void);
    ~CursesWindow(void);
  }              *curses;
  //
  // ReadLine buffer.
  char           *cmdline;
  // Various char * for strtok.
  char           *strtokstart;
  char           *strtoktmp;
  //
  // monitor wide control: if this variable gets send, the monitor
  // exists back to where it came from.
  bool            abort;
  //
  // Another control flag: If set, then the trace interrupt enters
  // the monitor.
  bool            fetchtrace;
  //
  // Definition of the command history buffer. This is a doubly linked list of the following
  // nodes
  struct HistoryLine : public Node<HistoryLine> {
    // The contents of this history line.
    char *Line;
    //
  public:
    HistoryLine(const char *line);
    ~HistoryLine(void)
    {
      delete[] Line;
      Remove();
    }
  };
  //
  // And the list containing the history lines.
  struct History : public List<HistoryLine> {
    // Size of the history buffer in lines
    static const int         MaxHistorySize INIT(32);
    //
    // The number of lines buffered so far, and the current active line
    int                      HistorySize;
    // The active line of the history
    struct HistoryLine      *ActiveLine;
    //
    // Constructors and destructors: Initialization
    History(void);
    ~History(void)
    {
      struct HistoryLine *hl;
      
      while((hl = First())) {
	// History lines remove themselves from the history.
	delete hl;
      };
    }
    //
    // Some methods: Attach a new line to the history, possibly reducing
    // its size.
    void AddLine(const char *line);
    //
    // Get the next earlier line in the history (implement "key up")
    // and copy it into a string that must be LONG enough.
    void EarlierLine(char *to);
    // Get the next later line in the history (implement "key down")
    // and copy it into a string buffer.
    void LaterLine(char *to);
    //
  }      History;
  //
  // Read a single line buffered from the input into an internal buffer.
  char *ReadLine(const char *prompt);
  // Parse off the next token from the input, or NULL if there aren't
  // any new tokens. If fullline is true, then all of the line is
  // returned.
  char *NextToken(bool fullline = false);
  //
  // Print a line over the output. This is currently just the console
  void Print(const char *fmt,...) const PRINTF_STYLE;
  // Print a line over the output. This is currently just the console
  void VPrint(const char *fmt,va_list args) const;
  //  
  // Parse off the next command from the token buffer
  void ParseCmd(char *tokens);
  //
  // Print the CPU registers in human-readable form
  // this does less than a CPU->DisplayStatus
  void PrintCPUStatus(void) const;
  //
  // Fill a buffer with the CPU flags in readable form
  void CPUFlags(char pstring[9]) const;
  //
  // Clear the symbol table.
  void ClearSymbolTable(void);
  //
  // Read a symbol table from a file.
  bool ParseSymbolTable(const char *filename);
  //
  // Add a command to the command chain
  struct Command* &CommandChain(void);
  //
  // The monitor main loop
  void MainLoop(bool title = true);
  //
  // Expression evaluator methods (private)
  bool EvaluateExpression(char *s,LONG &val);
  LONG EvaluateLogical(char *&s);
  LONG EvaluateCompare(char *&s);
  LONG EvaluatePlusMinus(char *&s);
  LONG EvaluateBinary(char *&s);
  LONG EvaluateMultiplyDivideMod(char *&s);
  LONG EvaluateShift(char *&s);
  LONG EvaluateNumeric(char *&s);
  // 
  // Exception generated by the above
  struct NumericException {
  public:
    NumericException(class Monitor *mon,const char *error,...) LONG_PRINTF_STYLE(3,4);
  };
  friend struct NumericException;
  //
  // ************************************************************************
  // ** command specific structures                                        **
  // ************************************************************************
  //
  // The following defines a command
  struct Command {
    class Monitor  *monitor;
    struct Command *next;
    const char     *longname;
    const char     *shortname;
    const char     *helptext;
    UWORD           here; // last address we worked on
    char            lastext;
    //
    Command(class Monitor *mon,
	    const char *lng,const char *shr,const char *helper,char x);
    //
    virtual ~Command(void);
    //
    virtual void Apply(char e) = 0;
    //
    // Helpers
    void Print(const char *fmt,...) PRINTF_STYLE;
    //
    // Get the next token
    char *NextToken(bool fullline = false)
    {
      return monitor->NextToken(fullline);
    }
    //
    // Get the next hex token or the "here" pointer
    bool GetAddress(UWORD &adr);
    // Get a numeric argument from the next token
    bool GetDefault(int &setting,int def,int min,int max);
    //
    // Some curses support functions follow
    //
    // Enable inverse video
    void InverseOn(void);
    // Disable inverse video
    void InverseOff(void);
    //
    // Get the currently active address space of the monitor
    class AdrSpace *Adr(void)
    {
      return monitor->currentadr;
    }
    //
    // Print an error
    void MissingArg(void)
    {
      Print("Required argument missing.\n");
    }
    //
    // Check whether this is the last argument
    bool LastArg(void)
    {
      char *token;
      
      if ((token = NextToken())) {
	Print("Unexpected argument %s.\n",token);
	return false;
      }
      return true;
    }
    //
    // Print unknown extender
    void ExtInvalid(void)
    {
      Print("Illegal or unknown extender for %s.\n",longname);
    }
    //
    // Miscellaneous service functions: Conversions from/to ANTIC/ASCII representation
    static UBYTE ASCIIToAntic(UBYTE a);
    static UBYTE AnticToATASCII(UBYTE c);
    // Read a line of data in hex,decimal,antic or ascii and
    // place it in a buffer.
    // Return the number of digits placed in the buffer, return zero
    // for error or for the end of the line.
    int ReadDataLine(UBYTE *buffer,const char *prompt,char mode,bool inverse = false);
  };
  //
  // This list here chains all available commands in a singly linked list
  struct Command *cmdchain;
  //
  // ************************************************************************
  //
  // All command implementations
  //  
  //
  // The Environment settings command
  struct Envi : public Command {
    Envi(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Envi;
  friend struct Envi;
  //
  // The split command display {
  struct Splt : public Command {  
  private:
    char *splitbuffer;
    char *splittmp;
    // Number of lines split off from the main window
    int   splitlines;
    //
    // Temporary storage for the cursor position on installation of split-off
    // command
    int  tmpx,tmpy;
    //
    // set the scroll region to all of the window, erase an old scrolling
    // region contents.
    void ClearScrollRegion(void);
    // Start launching a new scrolling region. Prepare to write output into
    // the split-off screen part
    void InitScrollRegion(void);
    // Finalize the scrolling region by installing the scrolling margins
    void CompleteScrollRegion(void);
    //
  public:
    Splt(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    ~Splt(void)
    {
      delete[] splitbuffer;
      delete[] splittmp;
    }
    void Apply(char e);
    //
    // Update the screen for the split buffer contents
    void UpdateSplit(void);
  } Splt;
  friend struct Splt;
  //
  // The register display
  struct Regs : public Command {
    Regs(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Regs;
  friend struct Regs;
  //
  // The register setter
  struct SetR : public Command {
    SetR(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } SetR; 
  friend struct SetR;
  //
  // The status display
  struct Stat : public Command {
    Stat(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Stat;  
  friend struct Stat;
  //
  // The step over subroutines
  struct Next : public Command {
    Next(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Next;
  friend struct Next;
  //
  // The single stepper
  struct Step : public Command { 
  private:
    // The following array identifes display lines with
    // addresses for smart tracing
    ADR             *lineaddresses;
    //
    // Number of lines we can handle currently.
    int              addressedlines;
    //
    // Index of the first line in this array.
    int              topyline;
    //
    // Open the optimized tracer window. Returns true on success.
    bool OpenDisplay(void);
    //
  public:
    Step(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    ~Step(void)
    {
      delete[] lineaddresses;
    }
    void Apply(char e);
    //
    // Refresh the output of the tracer window.
    void Refresh(class AdrSpace *adr,ADR pc);
    //
    // Refresh a single line at the given PC/address space
    // in the step output window. Possibly rebuild the full
    // screen if applicable. Returns false in case the step buffer
    // is not open.
    bool RefreshLine(class AdrSpace *adr,ADR pc,bool showpc = true);
    //
    // Remove the tracer display
    void CloseDisplay(void);
    //
    // Enter the tracer main loop.
    // Returns true in case the request was handled
    // and the main loop is not required.
    bool MainLoop(void);
  } Step;  
  friend struct Step;
  //
  // Leave the monitor to the CPU 
  struct GoPG : public Command {
    GoPG(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } GoPG;  
  friend struct GoPG;
  //
  // Leave the emulator
  struct Exit : public Command {
    Exit(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Exit;  
  friend struct Exit;
  //
  // Reset system command
  struct RSet : public Command {
    RSet(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } RSet;  
  friend struct RSet;
  //
  // The disassembler
  struct UnAs : public Command {
    UnAs(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  private:
    int  lines;
    //
  public:
    // Get the byte size of an instruction in bytes.
    int InstructionSize(UBYTE ins);
    //
    // Disassemble a single line into a buffer, return the instruction
    ADR DisassembleLine(class AdrSpace *adr,ADR where,char *line);
    void Apply(char e);
  } UnAs;  
  friend struct UnAs;
  //
  // The dList disassembly
  struct Dlst : public Command {
    Dlst(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  private:
    int lines;
    ADR DisassembleLine(class AdrSpace *adr,ADR where,char *line);
  public:
    void Apply(char e);
  } Dlst;   
  friend struct Dlst;
  //
  // Set and clear breakpoints
  struct BrkP : public Command {  
  private:
#ifdef HAS_MEMBER_INIT
    static const int NumBrk = 8;
#else
#define              NumBrk   8
#endif
    struct BreakPoint {
      ADR address;
      int id;      // its Id, as returned from the CPU interface, -1 for free
      bool enabled;
      bool read;   // if true, the watchpoint reacts on read also
      //
      BreakPoint(void)
	: address(0), id(-1), enabled(true)
      { }
    } BreakPoints[NumBrk],WatchPoints[NumBrk];
    //
  public:
    BrkP(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    //
    void Apply(char e);
    //
    // Toggle breakpoint at the indicated address on/off.
    void ToggleBreakpoint(ADR brk);
  } BrkP; 
  friend struct BrkP;
  //
  // The expression evaluator
  struct Eval : public Command {
    Eval(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Eval;
  friend struct Eval;
  //
  // The memory searcher
  struct Find : public Command {
    Find(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  private:
    bool inverse;
    int  lines;
  public:
    //
    void Apply(char e);
  } Find;
  friend struct Find;
  //
  // The memory mover
  struct Move : public Command {
    Move(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    //
    void Apply(char e);
  } Move;
  friend struct Move;
  //
  // The memory filler
  struct Fill : public Command {
    Fill(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  private:
    bool inverse;
  public:
    void Apply(char e);
  } Fill;
  friend struct Fill;
  //
  // The memory editor
  struct Edit : public Command {
    Edit(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  private:
    bool inverse;
  public:
    void Apply(char e);
  } Edit;
  friend struct Edit;
  //
  // The memory dumper
  struct Dump : public Command {
    Dump(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  private:
    int  lines;
    void PrintATASCII(UBYTE c);
  public:
    void Apply(char e);
  } Dump;
  friend struct Dump;
  //
  // The stack traceback
  struct SkTb : public Command {
    SkTb(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  public:
    void Apply(char e);
  } SkTb;
  friend struct SkTb;
  //
  // The disk IO command
  struct Disk : public Command {
    Disk(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  public:
    void Apply(char e);
  } Disk;
  friend struct Disk;
  //
  // The profiling command.
  struct Prof : public Command {
    Prof(class Monitor *mon,const char *lng,const char *shr,const char *helper);
  public:
    void Apply(char e);
  } Prof;
  friend struct Prof;
  //
  // The online help "system"
  struct Help : public Command {
    Help(class Monitor *mon,const char *lng,const char *shr,const char *helper);
    void Apply(char e);
  } Help;
  friend struct Help;
  //
  //
public:
  //
  // NOTE: The monitor must be build after all other classes have been
  // setup just before the RamController::Initialize phase.
  Monitor(class Machine *mach);
  ~Monitor(void);
  //
  // Print a string formatted over the monitor output
  // channel.
  void PrintStatus(const char *fmt,...) PRINTF_STYLE;
  //
  // Wait for the user to print any key to continue.
  void WaitKey(void);
  //
  // Enter the monitor by the front gate, by the monitor
  // hot-key F12
  void EnterMonitor(void);
  // Enter the monitor because we found an unhandle-able ESC code
  void UnknownESC(UBYTE code);
  // Enter the monitor because we found an unstable and unreliable opcode
  // that cannot be emulated savely
  void Crash(UBYTE opcode);
  // Enter the monitor because we found a JAM opcode that is not
  // an ESC opcode
  void Jam(UBYTE opcode);
  // Enter the monitor because we detected a breakpoint. Arguments
  // are the breakpoint number and the PC
  void CapturedBreakPoint(int i,ADR pc);
  // Enter the monitor because we detected a memory watch point.
  // Arguments are the breakpoint number and the address.
  void CapturedWatchPoint(int i,ADR mem);
  // Enter the monitor because of software tracing. Argument is the
  // current PC
  void CapturedTrace(ADR pc);
};
///

///
#endif
