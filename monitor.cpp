/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: monitor.cpp,v 1.111 2022/12/20 18:01:33 thor Exp $
 **
 ** In this module: Definition of the built-in monitor
 **********************************************************************************/

/// Includes
#include "machine.hpp"
#include "monitor.hpp"
#include "adrspace.hpp"
#include "cpu.hpp"
#include "mmu.hpp"
#include "antic.hpp"
#include "timer.hpp"
#include "display.hpp"
#include "new.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "curses.hpp"
#include "list.hpp"
#include "instruction.hpp"

#include <stdarg.h>
#include <ctype.h>
///

/// Defines
// The OS/2 implementation of ncurses doesn't seem handle the switch from
// the SDL window well.
// So for now, we deactivate it.
#if defined(USE_CURSES) && defined(OS2)
#undef USE_CURSES
#endif
///

/// Statics
#ifndef HAS_MEMBER_INIT
const int Monitor::History::MaxHistorySize = 32;
#endif
///

/// Monitor::Monitor
// Constructor of the monitor. This is a tad longer since we need to build
// all the commands of the monitor here.
Monitor::Monitor(class Machine *mach)
  : machine(mach), cpu(mach->CPU()), MMU(mach->MMU()),
    cpuspace(MMU->CPURAM()), anticspace(MMU->AnticRAM()), currentadr(MMU->CPURAM()),
    debugspace(MMU->DebugRAM()),
    tracefile(NULL), symboltable(NULL), curses(NULL), cmdline(NULL), 
    abort(false), fetchtrace(false),
    cmdchain(NULL),
    Envi(this,"ENVI","V","environment settings"),
    Splt(this,"SPLT","/","split off display"),
    Regs(this,"REGS","R","display the CPU register contents"),
    SetR(this,"SETR","S","set CPU register contents"),
    Stat(this,"STAT","T","chip status commands"),
    Next(this,"NEXT","N","step over subroutine calls"),
    Step(this,"STEP","Z","single step thru code"),
    GoPG(this,"GOPG","G","(re-)start the emulation"),
    Exit(this,"EXIT","X","exit the emulator"),
    RSet(this,"RSET","P","reset the emulator"),     
    UnAs(this,"UNAS","U","[expr]         : disassembly memory contents"),
    Dlst(this,"DLST","A","[expr]         : disassembly antic display list"),
    BrkP(this,"BRKP","B","[expr]         : set and clear breakpoints"),
    Eval(this,"EVAL","=","[expr]         : evaluate expression"),
    Find(this,"FIND","F","[expr]         : find pattern"),
    Move(this,"MOVE","M","from to size   : move memory contents"),
    Fill(this,"FILL","L","addr size      : fill memory with pattern"),
    Edit(this,"EDIT","E","[expr]         : edit memory contents"),
    Dump(this,"DUMP","D","[expr]         : display memory contents"),
    SkTb(this,"SKTB","K","[expr]         : stack traceback"),
    Disk(this,"DISK","I","file addr size : read or write memory to a file"),
    Prof(this,"PROF","O","profile code"),
    // help must go last into the chain to be the first getting displayed.
    Help(this,"HELP","?","display this text")
{ }
///

/// Monitor::~Monitor
Monitor::~Monitor(void)
{
  ClearSymbolTable();
  if (tracefile)
    fclose(tracefile);
  delete[] cmdline;
}
///

/// Monitor::ClearSymbolTable
// Remove all known labels from the symbol table.
void Monitor::ClearSymbolTable(void)
{
  struct Symbol *s;

  while(symboltable) {
    s = symboltable;
    symboltable = symboltable->next;
    delete s;
  }
}
///

/// Monitor::ParseSymbolTable
// This parses a ld65 debug file which contains
// all the symbols and labels of an assembled and linked ca65
// binary.
bool Monitor::ParseSymbolTable(const char *filename)
{
  FILE *symbols;
  char line[256];
  bool result = false;

  errno   = 0;
  symbols = fopen(filename,"r");
  if (symbols) {
    try {
      errno = 0;
      while(!feof(symbols)) {
	if (fgets(line,sizeof(line),symbols)) {
	  if (strncmp(line,"sym ",4) == 0 || strncmp(line,"sym\t",4) == 0) {
	    char *linebuffer;
	    struct Symbol s;
	    // This is a symbol line. These are the only ones we care about.
	    linebuffer = &line[4];
	    while(*linebuffer == ' ' || *linebuffer == '\t')
	      linebuffer++;
	    if (s.ParseLabel(linebuffer)) {
	      struct Symbol *add;
	      result = true;
	      if ((add = const_cast<struct Symbol *>(Symbol::FindSymbol(symboltable,s.name,0,s.type,s.size)))) {
		// Label already exists, replace its value.
		add->value  = s.value;
	      } else {
		add         = new struct Symbol(s);
		add->next   = symboltable;
		symboltable = add;
	      }
	    }
	  }
	} else if (errno) {
	  Print("Error %s reading the symbol file %s\n",strerror(errno),filename);
	}
      }
      fclose(symbols);
    } catch(...) {
      ClearSymbolTable();
      fclose(symbols);
      Print("Cannot allocate symbol table, no memory left.\n");
    }
  } else {
    Print("Error %s opening the symbol file %s\n",strerror(errno),filename);
  }

  return result;
}
///

/// Monitor::Symbol::ParseLabel
// Parse a label from a parsed-off ld65 debug output line.
// return true on success.
bool Monitor::Symbol::ParseLabel(char *line)
{
  char buf[32];
  size_t offs = 0;
  bool havetype = false;
  bool havesize = false;
  bool havevalue = false;

  while(*line == ' ' || *line == '\t')
    line++;
  //
  // Next is the label name in quotes.
  if (*line != '\"')
    return false;
  line++;
  while(*line != '"') {
    if (*line == '\\') {
      line++;
      if (*line == '"') {
	if (offs < sizeof(name)-1)
	  name[offs++] = '\"';
      } else if (*line == '\\') {
	if (offs < sizeof(name)-1)
	  name[offs++] = '\\';
      } else return false;
    } else {
      if (offs < sizeof(name)-1)
	name[offs++] = *line;
      line++;
    }
  }
  if (offs == 0)
    return false;
  // Name is now complete.
  name[offs] = 0;
  //
  // Names that start with a dot are local labels or assembler
  // generated labels. They do not become part of the database.
  if (name[0] == '.')
    return false;
  //
  // Skip over the quote.
  line++;
  //
  // Now parse off the remaining parameters.
  do {
    while(*line == ' ' || *line == '\t')
      line++;
    if (*line != ',') {
      if (*line == '\n' && havetype && havesize && havevalue)
	break;
      return false;
    }
    // Skip over the comma.
    line++;
    while(*line == ' ' || *line == '\t')
      line++;
    offs = 0;
    while(*line && *line != '=') {
      if (offs > sizeof(buf)-1)
	return false;
      buf[offs++] = *line++;
    }
    buf[offs] = 0;
    line++; // skip over the equates sign
    if (!strcmp(buf,"value")) {
      char *endptr = line;
      long v = strtol(line,&endptr,0);
      if (endptr == line)
	return false;
      if (v > 0xffff || v < 0)
	return false;
      value = UWORD(v);
      line  = endptr;
      if (havevalue)
	return false;
      havevalue = true;
    } else if (!strcmp(buf,"addrsize")) {
      if (!strncmp(line,"absolute",8)) {
	size  = Absolute;
	line += 8;
      } else if (!strncmp(line,"zeropage",8)) {
	size  = ZeroPage;
	line += 8;
      } else return false;
      if (havesize)
	return false;
      havesize = true;
    } else if (!strcmp(buf,"type")) {
      if (!strncmp(line,"equate",6)) {
	type  = Equate;
	line += 6;
      } else if (!strncmp(line,"label",5)) {
	type  = Label;
	line += 5;
      } else return false;
      if (havetype)
	return false;
      havetype = true;
    }
  } while(true);
  //
  return true;
}
///

/// Monitor::Symbol::FindSymbol
// Find a label by its address, size and type.
const struct Monitor::Symbol *Monitor::Symbol::FindSymbol(const struct Symbol *list,UWORD address,Type t,Size s)
{
  const struct Symbol *bestmatch = NULL;
  int score     = 0;
  int bestscore = 0;
  
  for(;list;list = list->next) {
    if (list->value == address) {
      score = 0;
      switch(t) {
      case Equate:
	if (list->type == Equate)
	  score += 10;
	else
	  continue;
	break;
      case Label:
	if (list->type == Label)
	  score += 10;
	else
	  continue;
	break;
      case Any:
	score += 5;
	break;
      case PreferLabel:
	if (list->type == Label)
	  score += 5;
	score += 2;
	break;
      case PreferEquate:
	if (list->type == Equate)
	  score += 5;
	score += 2;
	break;
      }
      switch(s) {
      case ZeroPage:
	if (list->size == ZeroPage)
	  score += 10;
	else
	  continue;
	break;
      case Absolute:
	if (list->size == Absolute)
	  score += 10;
	else
	  continue;
	break;
      case All:
	score += 5;
	break;
      case PreferZeroPage:
	if (list->size == ZeroPage)
	  score += 5;
	score += 2;
	break;
      case PreferAbsolute:
	if (list->size == Absolute)
	  score += 5;
	score += 2;
	break;
      }
      if (score >= bestscore) {
	bestscore = score;
	bestmatch = list;
      }
    }
  }
  
  return bestmatch;
}
///

/// Monitor::Symbol::FindSymbol
// Find a symbol by its name, size and type.
const struct Monitor::Symbol *Monitor::Symbol::FindSymbol(const struct Symbol *list,const char *name,UWORD pc,Type t,Size s)
{
  int bestdist = 0xffff;
  const struct Symbol *bestmatch = NULL;
  size_t l = strlen(name) + 1;

  if (l > Symbol::MaxLabelSize)
    l = Symbol::MaxLabelSize;

  while(list) {
    if ((t == Any || list->type == t) && (s == All || list->size == s)) {
      
      if (!strncasecmp(name,list->name,l)) {
	int dist = int(list->value) - int(pc);
	if (dist < 0)
	  dist = -dist;
	if (dist < bestdist) {
	  bestdist  = dist;
	  bestmatch = list;
	}
      }
    }
    list = list->next;
  }

  return bestmatch;
}
///

/// Monitor::CommandChain
// Add a command to the command chain
struct Monitor::Command* &Monitor::CommandChain(void)
{
  return cmdchain;
}
///

/// Monitor::Expression Evaluation miscellaneous
bool Monitor::EvaluateExpression(char *s,LONG &val)
{
  char *start = s;

  try {
    val = EvaluateLogical(s);
    if (*s != 0)
      throw NumericException(this,"Error: %s is an invalid expression.\n",start);
    return true;
  } catch (const NumericException &) {
    return false;
  }
}

LONG Monitor::EvaluateLogical(char *&s)
{
  LONG v1,v2;

  v1 = EvaluateCompare(s);
  do {
    if (!memcmp(s,"&&",2)) {
      s += 2;
      v2 = EvaluateCompare(s);
      v1 = v1 && v2;
      continue;
    } else if (!memcmp(s,"||",2)) {
      s += 2;
      v2 = EvaluateCompare(s);
      v1 = v1 || v2;
      continue;
    } else break;
  } while (true);
  return v1;
}

LONG Monitor::EvaluateCompare(char *&s)
{
  LONG v1,v2;

  v1 = EvaluatePlusMinus(s);
  if (!memcmp(s,"==",2)) {
    s += 2;
    v2 = EvaluatePlusMinus(s);
    return (v1 == v2);
  } else if (!memcmp(s,"!=",2)) {
    s += 2;
    v2 = EvaluatePlusMinus(s);
    return (v1 != v2);
  } else if (!memcmp(s,"<>",2)) {
    s += 2;
    v2 = EvaluatePlusMinus(s);
    return (v1 != v2);
  } else if (!memcmp(s,">=",2)) {
    s += 2;
    v2 = EvaluatePlusMinus(s);
    return (v1 >= v2);
  } else if (!memcmp(s,"<=",2)) {
    s += 2;
    v2 = EvaluatePlusMinus(s);
    return (v1 <= v2);
  } else if (*s == '=') {
    s++;
    v2 = EvaluatePlusMinus(s);
    return (v1 == v2);
  } else if (*s == '>') {
    s++;
    v2 = EvaluatePlusMinus(s);
    return (v1 > v2);
  } else if (*s == '<') {
    s++;
    v2 = EvaluatePlusMinus(s);
    return (v1 < v2);
  }
  return v1;
}

LONG Monitor::EvaluatePlusMinus(char *&s)
{
  LONG v1,v2;

  v1 = EvaluateBinary(s);
  do {
    if (*s == '+') {
      s++;
      v2 = EvaluateBinary(s);
      v1 = v1 + v2;
      continue;
    } else if (*s == '-') {
      s++;
      v2 = EvaluateBinary(s);
      v1 = v1 - v2;
      continue;
    } else break;
  } while(true);
  return v1;
}
  
LONG Monitor::EvaluateBinary(char *&s)
{
  LONG v1,v2;

  v1 = EvaluateMultiplyDivideMod(s);
  do {
    if (*s == '&') {
      s++;
      v2 = EvaluateMultiplyDivideMod(s);
      v1 = v1 & v2;
      continue;
    } else if (*s == '|') {
      s++;
      v2 = EvaluateMultiplyDivideMod(s);
      v1 = v1 | v2;
      continue;
    } else if (*s == '^') {
      s++;
      v2 = EvaluateMultiplyDivideMod(s);
      v1 = v1 ^ v2;
      continue;
    } else break;
  } while(true);
  return v1;
}

LONG Monitor::EvaluateMultiplyDivideMod(char *&s)
{
  LONG v1,v2;
  
  v1 = EvaluateShift(s);
  do {
    if (*s == '*') {
      s++;
      v2 = EvaluateShift(s);
      v1 = v1 * v2;
      continue;
    } else if (*s == '/') {
      s++;
      v2 = EvaluateShift(s);
      if (v2 == 0) 
	throw NumericException(this,"Error: Attempted division by zero\n");
      v1 = v1 / v2;
      continue;
    } else if (*s == '%') {
      s++;
      v2 = EvaluateShift(s);
      if (v2 == 0)
	throw NumericException(this,"Error: Attempted modulo by zero\n");
      v1 = v1 % v2;
      continue;
    } else break;
  } while(true);
  return v1;
}

LONG Monitor::EvaluateShift(char *&s)
{
  LONG v1,v2;

  v1 = EvaluateNumeric(s);
  do {
    if (!memcmp(s,">>",2)) {
      s += 2;
      v2 = EvaluateNumeric(s);
      v1 = v1 >> v2;
      continue;
    } else if (!memcmp(s,"<<",2)) {
      s += 2;
      v2 = EvaluateNumeric(s);
      v1 = v1 << v2;
      continue;
    } else break;
  } while(true);
  return v1;
}

LONG Monitor::EvaluateNumeric(char *&s)
{
  LONG v1;
  
  if (*s == '-') {
    s++;
    v1 = EvaluateNumeric(s);
    return (-v1);
  } else if (*s == '~') {
    s++;
    v1 = EvaluateNumeric(s);
    return ~v1;
  } else if (*s == '(') {
    s++;
    v1 = EvaluateLogical(s);
    if (*s == 0) 
      throw NumericException(this,"Error: Missing ')'\n");
    if (*s != ')') 
      throw NumericException(this,"Error: Expected ')' but found %c\n",*s);
    s++;
    return v1;
  } else if (*s == '[') {
    s++;
    v1 = EvaluateLogical(s);
    if (*s == 0)
      throw NumericException(this,"Error: Missing ']'\n");
    if (*s != ']')
      throw NumericException(this,"Error: Expected ']' but found %c\n",*s);
    s++;
    if (!memcmp(s,".w",2)) {
      s += 2;
      v1 = currentadr->ReadWord(UWORD(v1));
    } else if (!memcmp(s,".b",2)) {
      s += 2;
      v1 = currentadr->ReadByte(UWORD(v1));
    } else {
      v1 = currentadr->ReadByte(UWORD(v1));
    }
    return v1;
  } else if (!memcmp(s,"pc",2)) {
    s += 2;
    return cpu->PC();
  } else if (*s == 'x' && (!isalnum(s[1]))) {
    s++;
    return cpu->X();
  } else if (*s == 'y' && (!isalnum(s[1]))) {
    s++;
    return cpu->Y();
  } else if (*s == 'p' && (!isalnum(s[1]))) {
    s++;
    return cpu->P();
  } else if (*s == 's' && (!isalnum(s[1]))) {
    s++;
    return cpu->S();
  } else if (*s == 'a'  && (!isalnum(s[1]))) {
    s++;
    return cpu->A();
  } else if (*s == '#') {
    char *end;
    s++;
    v1 = strtol(s,&end,0);
    if (errno == ERANGE)
      throw NumericException(this,"Error: %s is out of range\n",s);
    if (s == end)
      throw NumericException(this,"Error: Invalid token %s\n",s);
    s  = end;
    return v1;
  } else if (*s == '$') {
    char *end;
    s++;
    v1 = strtol(s,&end,16);
    if (errno == ERANGE)
      throw NumericException(this,"Error: %s is out of range\n",s);
    if (s == end)
      throw NumericException(this,"Error: Invalid token %s\n",s);
    s  = end;
    return v1;
  } else if (isalpha(*s)) {
    char labelname[64]; 
    const struct Symbol *symbol;
    char *sptr = s;
    char *lptr = labelname;
    while(lptr < labelname + sizeof(labelname) - 1) {
      *lptr = *sptr;
      lptr++,sptr++;
      if (!isalnum(*sptr)) 
	break;
    }
    *lptr  = 0;
    symbol = Symbol::FindSymbol(symboltable,labelname,cpu->PC(),Symbol::Any,Symbol::All);
    if (symbol) {
      s = sptr;
      return symbol->value;
    }
  }
  // Final try: Another type of value.
  {
    char *end;
    v1 = strtol(s,&end,16);
    if (errno == ERANGE)
      throw NumericException(this,"Error: %s is out of range\n",s);
    if (s == end)
      throw NumericException(this,"Error: Invalid token %s\n",s);
    s  = end;
    return v1;
  }
}

Monitor::NumericException::NumericException(class Monitor *mon,const char *errfmt,...)
{
  va_list args;
  
  va_start(args,errfmt);
  mon->VPrint(errfmt,args);
  va_end(args);
}
///

/// Monitor::HistoryLine::HistoryLine
Monitor::HistoryLine::HistoryLine(const char *line)
  : Line(new char[strlen(line)+1])
{
  strcpy(Line,line);
}
///

/// Monitor::History::History
// Constructor of the history: Just constructs the
// list.
Monitor::History::History(void)
  : HistorySize(0), ActiveLine(NULL)
{
}
///

/// Monitor::History::AddLine
// Attach a new line to the history, possibly reducing
// its size.
void Monitor::History::AddLine(const char *line)
{      
  // Now add this line to the history if there is any useful input
  if (*line) {
    AddTail(new struct HistoryLine(line));
    HistorySize++;
    if (HistorySize > MaxHistorySize) {
      struct HistoryLine *hl;
      // Shrink the history if it grows too LONG.
      // Note that the node removes itself from the history.
      hl = First();
      // This unlinks itself from the list
      delete hl;
      // delete First();
      HistorySize--;
    }
  }
  ActiveLine = NULL;
}
///

/// Monitor::History::EarlierLine
// Get the next earlier line in the history (implement "key up")
// and copy it into a string that must be LONG enough.
void Monitor::History::EarlierLine(char *to)
{
  // Go to the previous history line and insert it.
  if (ActiveLine) {
    ActiveLine = ActiveLine->PrevOf();
  } else {
    ActiveLine = Last();
  }
  if (ActiveLine) {
    strcpy(to,ActiveLine->Line);
  } else {
    *to = 0;
  }
}
///

/// Monitor::History::LaterLine
// Get the next later line in the history (implement "key down")
// and copy it into a string buffer.
void Monitor::History::LaterLine(char *to)
{
  // Go to the next history line and insert it.
  if (ActiveLine) {
    ActiveLine = ActiveLine->NextOf();
  } else {
    ActiveLine = First();
  }
  if (ActiveLine) {
    strcpy(to,ActiveLine->Line);
  } else {
    *to = 0;
  }
}
///

/// Monitor::ReadLine
// Read a line buffered from stdin. This uses the GNU readline for
// for simplicity.
char *Monitor::ReadLine(const char *prompt)
{
  int bufpos,lnsize;
  bool done;
  static const int maxwidth = 80;
  //
  // 
  if (cmdline == NULL) {
    cmdline = new char[maxwidth+1];
  }    
  // Switch the monitor to foreground so we can see what we type
  if (machine->hasGUI())
    machine->Display()->SwitchScreen(false);
  //
  bufpos = 0;
  lnsize = 0;
  done   = false;
#ifndef USE_CURSES
  {
    char *input = cmdline;
    size_t end;
    
    printf("%s",prompt);
    fflush(stdout);
    machine->RefreshDisplay();
    fgets(input,maxwidth,stdin);
    end = strlen(input);
    if (end > 0 && input[--end] == '\n')
      input[end] = '\0';
  }
#else
  int x,y,c;
  bool changed = false;
  // I wish I could use here a plain
  // addstr(prompt);
  // but I can't. On some architectures (Solaris)
  // the addstr prototype is broken and requires a non-const character pointer.
  Print("%s",prompt);
  getyx((WINDOW *)curses->window,y,x);
  do {
    refresh();
    c = getch();
    // Update the display now by running a VBI cycle of the machine
    // in "paused" mode.
    machine->RefreshDisplay();
    //Print("%x\n",c);
    //
    switch(c) {
    case ERR:
      break;
    case 0x0a:
    case 0x0d:
    case KEY_ENTER:
      cmdline[lnsize] = 0;
      // addstr("\n"); See comment above 
      move(y,0);
      Print("%s%s\n",prompt,cmdline);
      refresh();
      done = true;
      // Now add this line to the history if there is any useful input
      if (changed)
	History.AddLine(cmdline);
      break;
    case 0x08: // PDCurses doesn't define/use KEY_BACKSPACE correctly here... BUMMER!
    case 0x7f: // and some ncurses implementations don't send the right code either... BUMMER!
    case KEY_BACKSPACE:
      changed = true;
      if (bufpos > 0) {
	memmove(cmdline+bufpos-1,cmdline+bufpos,lnsize-bufpos);
	bufpos--;
	lnsize--;
	x--;
	move(y,x);
	delch();
      }
      break;
    case 0x17: // Delete last word
    case KEY_BTAB:
      changed = true;
      c = 0;
      while(bufpos > c && isspace(cmdline[bufpos-1-c]))
	c++;
      while(bufpos > c && !isspace(cmdline[bufpos-1-c]))
	c++;
     while(bufpos > c && isspace(cmdline[bufpos-1-c]))
	c++;
     if (c > 0) {
	memmove(cmdline+bufpos-c,cmdline+bufpos,lnsize-bufpos);
	bufpos -= c;
	lnsize -= c;
	x      -= c;
	move(y,x);
	while(c) {
	  delch();
	  c--;
	}
      }
      break;
    case 0x05:
    case KEY_END:
      x += lnsize - bufpos;
      bufpos = lnsize;
      move(y,x);
      break;
    case 0x01:
    case KEY_HOME:
    case KEY_BEG:
      x -= bufpos;
      bufpos = 0;
      move(y,x);
      break;
    case KEY_LEFT:
      if (bufpos > 0) {
	bufpos--;
	x--;
	move(y,x);
      }
      break;
    case KEY_RIGHT:
      if (bufpos < lnsize) {
	bufpos++;
	x++;
	move(y,x);
      }
      break;
    case KEY_UP:
    case KEY_DOWN:
      // History management functions: Keyboard up and down
      // arrows move thru the history buffer.
      if (changed) {
	cmdline[lnsize] = 0;
	History.AddLine(cmdline);
      }
      if (c == KEY_UP) {
	// Go to the previous history line and insert it.
	History.EarlierLine(cmdline);
	if (changed)
	  History.EarlierLine(cmdline); // not the same input again.
      } else {
	// Go to the next history line and insert it.
	History.LaterLine(cmdline);
      }
      insdelln(-1);
      move(y,0);
      lnsize       = strlen(cmdline);
      bufpos       = lnsize;
      Print("%s%s",prompt,cmdline);
      getyx((WINDOW *)curses->window,y,x);
      changed      = false;
      break;
    case KEY_DL:
    case KEY_CLEAR: 
      changed = true;
      x     -= bufpos;
      bufpos = 0;
      lnsize = 0;
      move(y,x);
      break;
    case KEY_DC:
      changed = true;
      if (bufpos < lnsize) {
	memmove(cmdline+bufpos,cmdline+bufpos+1,lnsize-bufpos-1);
	lnsize--;
	delch();
      }
      break;
    case KEY_F(11):
      strcpy(cmdline,"Z");
      return cmdline;
    case KEY_F(10):
      strcpy(cmdline,"N");
      return cmdline;
    case KEY_F(5):
      strcpy(cmdline,"G");
      return cmdline;
    case KEY_F(6):
      strcpy(cmdline,"G.U");
      return cmdline;
    case KEY_F(7):
      Print("\n");
      strcpy(cmdline,"U PC");
      return cmdline;
    default:
      if (c < 0x100 && isprint(c)) {
	if (lnsize < maxwidth) { 
	  changed = true;
	  memmove(cmdline+bufpos+1,cmdline+bufpos,maxwidth-bufpos-1);
	  cmdline[bufpos] = (char)c;
	  insch(c);
	  x++;
	  bufpos++;
	  lnsize++;
	  move(y,x);	  
	}
      }
    }
  } while(!done);
  refresh();
#endif
  //
  return cmdline;
}
///

/// Monitor::NextToken
// Return the next token from the line just read, advance the token
char *Monitor::NextToken(bool fullline)
{
  char *parse,*token;

  if (strtokstart) {
    // start a new parsing process
    parse       = strtokstart;
    strtokstart = NULL;
  } else {
    parse       = strtoktmp;
  }
  if (parse == NULL)
    return NULL;
  // Skip leading blanks
  if (fullline) {
    while(*parse == '\n')
      parse++;
  } else {
    while(isspace(*parse))
      parse++;
  }
  // This is the start of the current token if we are not at the end of the string anyhow.
  if (*parse) {
    token = parse;
  } else {
    token = NULL;
  }
  // Now run over non-space characters and replace them by lower case characters.
  if (fullline) {
    while(*parse && (*parse != '\n')) {
      *parse = (char)tolower(*parse);
      parse++;
    }  
  } else {
    while(*parse && (!isspace(*parse))) {
      *parse = (char)tolower(*parse);
      parse++;
    }
  }
  if (*parse) {
    // End the current token by NUL
    *parse    = '\0';
    strtoktmp = parse+1; // continue from this point on
  } else {
    strtoktmp = NULL;
  }
  return token;
}
///

/// Monitor::Print
// Print a line over the output. This is currently just the console
void Monitor::Print(const char *fmt,...) const
{
  va_list args;

  va_start(args,fmt);
  VPrint(fmt,args);

  va_end(args);
}
///

/// Monitor::VPrint
// Print a line over the output. This is currently just the console
void Monitor::VPrint(const char *fmt,va_list args) const
{
  char outbuffer[2048];
  vsnprintf(outbuffer,2047,fmt,args);
#if CHECK_LEVEL > 0
  if (curses == NULL)
    Throw(ObjectDoesntExist,"Monitor::VPrint","Output curses not established");
#endif
#ifdef USE_CURSES
  addstr(outbuffer);
  refresh();
#else
  printf("%s",outbuffer);
  fflush(stdout);
#endif
}
///

/// Monitor::WaitKey
// Wait for the user to print any key to continue.
void Monitor::WaitKey(void)
{
  Print("<Press RETURN to continue>\n");
#ifdef USE_CURSES
  int c;
  do {
    c = getch();
  } while(c != KEY_ENTER && c != 0x0a && c != 0x0d && c != ' ');
#else
  fgetc(stdin);
#endif
}
///

/// Monitor::PrintCPUStatus
// Print the CPU registers in human-readable form
// this does less than a CPU->DisplayStatus
void Monitor::PrintCPUStatus(void) const
{
  char pstring[9];

  CPUFlags(pstring);
  
  Print("PC: $%04x  A:$%02x  X:$%02x  Y:$%02x  S:$%02x  P:$%02x = %s\n",
	cpu->PC(),cpu->A(),cpu->X(),cpu->Y(),cpu->S(),cpu->P(),pstring);
}
///

/// Monitor::CPUFlags
// Fill a buffer with the CPU flags in readable form
void Monitor::CPUFlags(char pstring[9]) const
{  
  UBYTE flags = cpu->P();

  pstring[0] = (flags & CPU::N_Mask)?('N'):('-');
  pstring[1] = (flags & CPU::V_Mask)?('V'):('-');
  pstring[2] = '.';
  pstring[3] = (flags & CPU::B_Mask)?('B'):('-');
  pstring[4] = (flags & CPU::D_Mask)?('D'):('-');
  pstring[5] = (flags & CPU::I_Mask)?('I'):('-');
  pstring[6] = (flags & CPU::Z_Mask)?('Z'):('-');
  pstring[7] = (flags & CPU::C_Mask)?('C'):('-');
  pstring[8] = 0;
}
///

/// Monitor::Command::Command
Monitor::Command::Command(class Monitor *mon,
			  const char *lng,const char *shr,const char *helper,char x)
  : monitor(mon), longname(lng), shortname(shr), helptext(helper), 
    here(0), lastext(x)
{
  // link us into the command chain
  next = mon->CommandChain();
  mon->CommandChain() = this;
}
///

/// Monitor::Command::~Command
Monitor::Command::~Command(void)
{ }
///

/// Monitor::Command::GetAddress
// Get the next hex token or the "here" pointer
bool Monitor::Command::GetAddress(UWORD &adr)
{
  bool valid = false;
  char *token;
  LONG value = here;

  token = NextToken();
  if (token) {
    valid = monitor->EvaluateExpression(token,value);
    adr   = UWORD(value);
  } else {
    adr   = here;
    valid = true;
  }
  return valid;
}
///

/// Monitor::Command::GetDefault
bool Monitor::Command::GetDefault(int &setting,int def,int min,int max)
{
  bool valid = false;
  char *token,*last;
  LONG value;
  //
  token = NextToken();
  if (token) {
    value = strtol(token,&last,16);
    if (*last == 0) {
      if (value >= min && value <= max) {
	valid   = true;
	setting = value;
      } else {
	Print(ATARIPP_LD " is out of range, must be >= %d and <= %d.\n",value,min,max);
      }
    } else {
      Print("%s is not a valid number.\n",token);
    }
  } else {
    valid   = true;
    setting = def;
  }

  return valid;
}
///

/// Monitor::Command::InverseOn
// Enable inverse video using curses
void Monitor::Command::InverseOn(void)
{
#ifdef USE_CURSES
  attron(A_BOLD);
#endif
}
///

/// Monitor::Command::InverseOff
// Disable inverse video using curses
void Monitor::Command::InverseOff(void)
{
#ifdef USE_CURSES
  attroff(A_BOLD);
#endif
}
///

/// Monitor::Command::AnticToATASCII
// Convert the ANTIC internal representation to ASCII printable
UBYTE Monitor::Command::AnticToATASCII(UBYTE c)
{
  UBYTE out = 0;

  if (c & 0x80) {
    out |= 0x80;
    c   &= 0x7f;
  }
  switch(c & 0x60) {
  case 0x00:
    out |= UBYTE(0x20 | (c & 0x1f));
    break;
  case 0x20:
    out |= UBYTE(0x40 | (c & 0x1f));
    break;
  case 0x40:
    out |= UBYTE(0x00 | (c & 0x1f));
    break;
  case 0x60:
    out |= UBYTE(0x60 | (c & 0x1f));
    break;
  }
  return out;
}
///

/// Monitor::Command::ASCIIToAntic
// Convert an ASCII character to its ANTIC screen representation
UBYTE Monitor::Command::ASCIIToAntic(UBYTE c)
{
  UBYTE out = 0;

  switch(c & 0x60) {
  case 0x00:
    out |= 0x40 | (c & 0x1f);
    break;
  case 0x20:
    out |= 0x00 | (c & 0x1f);
    break;
  case 0x40:    
    out |= 0x20 | (c & 0x1f);
    break;
  case 0x60:
    out |= 0x60 | (c & 0x1f);
    break;
  }
  return out;
}
///

/// Monitor::Command::ReadDataLine
// Read a line of data in hex,decimal,antic or ascii and
// place it in a buffer.
// Return the number of digits placed in the buffer, return zero
// for error or for the end of the line.
int Monitor::Command::ReadDataLine(UBYTE *buffer,const char *prompt,char mode,bool inverse)
{   
  char *input;
  int count = 0;
  int base  = 16;
  
  // Now ask the readline buffer to get the data
  input = monitor->ReadLine(prompt);
  if (input == NULL || *input == '\0') {
    // No data, return a zero to indicate the EOF
    return 0;
  }
  // Now check for the input mode picked by the user
  //
  switch(mode) {
  case 'A':
    while(*input) {
      *buffer++ = UBYTE(*input++ | ((inverse)?(0x80):(0x00)));
      count++;
    }
    return count;
  case 'S':
    while(*input) {
      *buffer++ = UBYTE(ASCIIToAntic(*input++) | ((inverse)?(0x80):(0x00)));
      count++;
    }
    return count;
  case 'D':
    base = 10;
    // Base defaults to 16 otherwise.
    // Intentionally falls through.
  case 'X':
    while(*input) {
      LONG value;
      char *last;
      value = strtol(input,&last,base);
      if (isspace(*last) || *last == '\0') {
	if (value >= 0x00 && value <= 0xff) {
	  *buffer++ = UBYTE(value);
	  count++;
	} else {
	  if (base == 10) {
	    Print("Input " ATARIPP_LD " is not a valid byte.\n",value);
	  } else {
	    Print("Input " ATARIPP_LX " is not a valid byte.\n",(ULONG)(value));
	  }
	  return 0;
	}
      } else {
	Print("Input %s is invalid.\n",input);
	return 0;
      }
      if (*last == '\0') {
	break;
      } else {
	input = last+1;
      }
    }
    return count;
  default:
    ExtInvalid();
    return 0;
  }
}
///

/// Monitor::Splt::Splt
Monitor::Splt::Splt(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'S'), 
    splitbuffer(NULL), splittmp(NULL), splitlines(0)
{ }
///

/// Monitor::Splt::Apply
// record a command to split off on top of the screen
void Monitor::Splt::Apply(char e)
{
#ifdef USE_CURSES
  char *token;
  
  switch(e) {
  case '?':
    Print("SPLT.C     : Remove the split-off command from the screen\n"
	  "SPLT.S cmd : Define a command to split off on top of the screen\n"
	  );
    return;
  case 'C':
    if (!LastArg())
      return;
    delete[] splitbuffer;
    delete[] splittmp;
    splitbuffer = NULL;
    splittmp    = NULL;
    break;
  case 'S':
    delete[] splitbuffer;
    delete[] splittmp;
    splitbuffer = NULL;
    splittmp    = NULL;
    //
    token = NextToken(true);
    if (token) {
      int i,len = strlen(token);
      //
      splitbuffer = new char[len + 1];
      splittmp    = new char[len + 1];
      for(i=0;i<len;i++) {
	splitbuffer[i] = (token[i] == ':')?('\n'):(token[i]);
      }
      splitbuffer[i]   = '\0';
    }
    break;
  default:
    ExtInvalid();   
  }
#else
  Print("SPLT       : unsupported, since curses output is not compiled in\n"
	);
#endif
}
///

/// Monitor::Splt::UpdateSplit
// Update the upper split display by invoking one or more commands there.
void Monitor::Splt::UpdateSplit(void)
{
#ifdef USE_CURSES
  // Remove the scroll region completely
  ClearScrollRegion();
  if (splitbuffer) {
    InitScrollRegion();
    strcpy(splittmp,splitbuffer);
    monitor->ParseCmd(splittmp);
    CompleteScrollRegion();
  } 
  refresh();
#endif
}
///

/// Monitor::Splt::ClearScrollRegion
// set the scroll region to all of the window, erase an old scrolling
// region contents.
void Monitor::Splt::ClearScrollRegion(void)
{  
#ifdef USE_CURSES
  int i,x,y,maxx,maxy; 
  getyx((WINDOW *)monitor->curses->window,y,x);
  getmaxyx((WINDOW *)monitor->curses->window,maxy,maxx);
  move(0,0);
  for(i=0;i<splitlines;i++) {
    deleteln();
  }
  for(i=0;i<splitlines;i++) {
    insertln();
  }
  setscrreg(0,maxy);
  splitlines = 0;
  move(y,x);
#endif
}
///

/// Monitor::InitScrollRegion
// Start launching a new scrolling region. Prepare to write output into
// the split-off screen part
void Monitor::Splt::InitScrollRegion(void)
{  
#ifdef USE_CURSES
  getyx((WINDOW *)monitor->curses->window,tmpy,tmpx);
  move(0,0);
#endif
}
///

/// Monitor::CompleteScrollRegion
// Finalize the scrolling region by installing the scrolling margins
void Monitor::Splt::CompleteScrollRegion(void)
{
#ifdef USE_CURSES
  int x,y,maxx,maxy;
  getyx((WINDOW *)monitor->curses->window,y,x);  
  getmaxyx((WINDOW *)monitor->curses->window,maxy,maxx);
  move(y,x);
  //attron(A_REVERSE);
  hline('-',maxx-1);  
  //attroff(A_REVERSE);
  splitlines = y+1;
  setscrreg(splitlines,maxy);
  if (tmpy < splitlines)
    tmpy = splitlines;
  move(tmpy,tmpx);
#endif
}
///

/// Monitor::Eval::Eval
Monitor::Eval::Eval(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'\0')
{ }
///

/// Monitor::Eval::Apply
// Display memory commands
void Monitor::Eval::Apply(char e)
{
  char *token;
  LONG value;
  bool valid;

  switch(e) {
  case '?':
    Print("EVAL does not take any extensions.\n");
    return;
  default:
    token = NextToken();
    if (token) {
      valid = monitor->EvaluateExpression(token,value);
      if (valid) {
	Print("%s = 0x" ATARIPP_LX " = " ATARIPP_LD "\n",token,value,value);
      }
      return;
    }
  }
}
///

/// Monitor::Dump::Dump
Monitor::Dump::Dump(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'A'), lines(16)
{ }
///

/// Monitor::Dump::PrintATASCII
void Monitor::Dump::PrintATASCII(UBYTE c)
{
  if (isprint(c & 0x7f)) {
    if (c & 0x80) {
      InverseOn();
    }
    Print("%c",c & 0x7f);
    if (c & 0x80) {
      InverseOff();
    }
  } else {
    Print(".");
  }
}
///

/// Monitor::Dump::Apply
// Display memory commands
void Monitor::Dump::Apply(char e)
{
  int i;

  switch(e) {
  case '?':
    Print("Dump subcommands:\n"
	  "DUMP.A [addr]  : dump memory as bytes and ATASCII\n"
	  "DUMP.S [addr]  : dump memory as bytes and ANTIC screen codes\n"
	  "DUMP.V [lines] : set number of lines to dump\n"
	  );
    return;
  case 'A':
  case 'S':
    if (GetAddress(here)) {
      if (!LastArg())
	return;
      for(i=0;i<lines;i++) {
	int j;
	UBYTE d,c[8];
	
	Print("$%04x: ",here);
	for(j=0;j<8;j++) {
	  d = c[j] = Adr()->ReadByte(here++);
	  Print("%02x ",d);
	}
	for(j=0;j<8;j++) {
	  d = c[j];
	  if (e == 'S') {
	    d = AnticToATASCII(d);
	  }
	  PrintATASCII(d);
	}
	Print("\n");
      }
    }
    break;
  case 'V':
    GetDefault(lines,16,1,32);
    break;  
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::UnAs::UnAs
Monitor::UnAs::UnAs(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'L'), lines(16)
{ }
///

/// Monitor::UnAs::DisassembleLine
// Disassemble a single instruction into a buffer,
// advance the disassembly position, return it.
ADR Monitor::UnAs::DisassembleLine(class AdrSpace *adr,ADR where,char *line)
{
  int op;
  ADR pc = where;
  class CPU *cpu = monitor->cpu;
  const char *name;
  UBYTE inst;
  char buf[48];
  char pcbuf[48];
  Instruction::OperandType type;
  const struct Symbol *pctarget;
  const struct Symbol *target;
  
  if (where >= 0x10000) {
    *line = 0;
    return where;
  }
  // Format of the output:
  // xxxx             ab cd ef  IIII noooo
  // namenamenamenam:                namenamenamenamn,namenamenamenam    
  // 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
  inst    = adr->ReadByte(where++); 
  {
    const struct Instruction &dis = cpu->Disassemble(inst);
    name    = dis.Name;
    type    = dis.AddressingMode;
  }
  switch(type) {
  case Instruction::NoArgs:
    sprintf(buf,"%-4s",name);
    break;
  case Instruction::Immediate:
    if (where < 0x10000) {
      op     = adr->ReadByte(where++);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,op,Symbol::Equate,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s #%.16s",name,target->name);
      } else {
	sprintf(buf,"%-4s #$%02x",name,op);
      }
    } else {
      sprintf(buf,"%-4s #$XX",name);
    }
    break;
  case Instruction::Accu:
    sprintf(buf,"%-4s A",name);
    break;
  case Instruction::ZPage:
    if (where < 0x10000) {
      op     = adr->ReadByte(where++);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,op,Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s  %.16s",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%02x",name,op);
      }
    } else {
      sprintf(buf,"%-4s  $XX",name);
    }
    break;
  case Instruction::ZPage_X:
    if (where < 0x10000) {
      op  = adr->ReadByte(where++);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,UBYTE(op + cpu->X()),Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s  %.16s,X",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%02x,X",name,op);
      }
    } else {
      sprintf(buf,"%-4s  $XX,X",name);
    }
    break;
  case Instruction::ZPage_Y:
    if (where < 0x10000) {
      op  = adr->ReadByte(where++); 
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,UBYTE(op + cpu->Y()),Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s  %.16s,Y",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%02x,Y",name,op);
      }
    } else {
      sprintf(buf,"%-4s  $XX,Y",name);
    }
    break;
  case Instruction::Indirect:
    if (where < 0x10000) {
      op  = adr->ReadWord(where);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,op,Symbol::PreferEquate,Symbol::PreferAbsolute);
      if (target) {
	sprintf(buf,"%-4s  (%.16s)",name,target->name);
      } else {
	sprintf(buf,"%-4s  ($%04x)",name,op);
      }
      where += 2;
    } else {
      sprintf(buf,"%-4s ($XXXX)",name);
    }
    break;
  case Instruction::Indirect_X:
    if (where < 0x10000) {
      op  = adr->ReadByte(where++);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,UBYTE(op + cpu->X()),Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s  (%.16s,X)",name,target->name);
      } else {
	sprintf(buf,"%-4s  ($%02x,X)",name,op);
      }
    } else {
      sprintf(buf,"%-4s ($XX,X)",name);
    }
    break;
  case Instruction::Indirect_Y:
    if (where < 0x10000) {
      op  = adr->ReadByte(where++);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,op,Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s  (%.16s),Y",name,target->name);
      } else {
	sprintf(buf,"%-4s  ($%02x),Y",name,op);
      }
    } else {
      sprintf(buf,"%-4s ($XX),Y",name);
    }
    break;
  case Instruction::Indirect_Z:
    if (where < 0x10000) {
      op  = adr->ReadByte(where++);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,op,Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	sprintf(buf,"%-4s  (%.16s)",name,target->name);
      } else {
	sprintf(buf,"%-4s  ($%02x)",name,op);
      }
    } else {
      sprintf(buf,"%-4s ($XX)",name);
    }
    break;
  case Instruction::Absolute:
    if (where < 0xffff) {
      op  = adr->ReadWord(where);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,op,Symbol::PreferLabel,Symbol::PreferAbsolute);
      if (target) {
	sprintf(buf,"%-4s  %.16s",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%04x",name,op);
      }
      where += 2;
    } else {
      sprintf(buf,"%-4s  $XXXX",name);
    }      
    break;
  case Instruction::Absolute_X:
    if (where < 0xffff) {
      op  = adr->ReadWord(where);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,UWORD(op + cpu->X()),Symbol::PreferLabel,Symbol::PreferAbsolute);
      if (target) {
	sprintf(buf,"%-4s  %.16s,X",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%04x,X",name,op);
      }
      where += 2;
    } else {
      sprintf(buf,"%-4s  $XXXX,X",name);
    }
    break;
  case Instruction::Absolute_Y:
    if (where < 0xffff) {
      op  = adr->ReadWord(where);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,UWORD(op + cpu->Y()),Symbol::PreferLabel,Symbol::PreferAbsolute);
      if (target) {
	sprintf(buf,"%-4s  %.16s,Y",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%04x,Y",name,op);
      }
      where += 2;
    } else {
      sprintf(buf,"%-4s  $XXXX,Y",name);
    }
    break;
  case Instruction::AbsIndirect_X:
    if (where < 0xffff) {
      op  = adr->ReadWord(where);
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,UBYTE(op + cpu->X()),Symbol::PreferLabel,Symbol::PreferAbsolute);
      if (target) {
	sprintf(buf,"%-4s  (%.16s,X)",name,target->name);
      } else {
	sprintf(buf,"%-4s  ($%04x,X)",name,op);
      }
      where += 2;
    } else {
      sprintf(buf,"%-4s ($XXXX,X)",name);
    }
    break;
  case Instruction::Disp:
    // Note that this displacement is signed
    if (where < 0x10000) {
      op  = (BYTE)(adr->ReadByte(where++));
      target = Monitor::Symbol::FindSymbol(monitor->symboltable,where+op,Symbol::PreferLabel,Symbol::PreferAbsolute);
      if (target) {
	sprintf(buf,"%-4s  %.16s",name,target->name);
      } else {
	sprintf(buf,"%-4s  $%04x",name,where+op);
      }
    } else {
      sprintf(buf,"%-4s  $XXXX",name);
    }
    break;
  case Instruction::ZPage_Disp:
    if (where < 0xffff) {
      UBYTE zp = adr->ReadByte(where++);
      op       = (BYTE)adr->ReadByte(where++);
      pctarget = Monitor::Symbol::FindSymbol(monitor->symboltable,where+op,Symbol::PreferLabel,Symbol::PreferAbsolute);
      target   = Monitor::Symbol::FindSymbol(monitor->symboltable,zp      ,Symbol::PreferLabel,Symbol::ZeroPage);
      if (target) {
	if (pctarget) {
	  sprintf(buf,"%-4s  %.16s,%.16s",name,target->name,pctarget->name);
	} else {
	  sprintf(buf,"%-4s  %.16s,$%04x",name,target->name,where+op);
	}
      } else {
	if (pctarget) {
	  sprintf(buf,"%-4s  $%02x,%.16s",name,zp,pctarget->name);
	} else {
	  sprintf(buf,"%-4s  $%02x,$%04x",name,zp,where+op);
	}
      }
    } else {
      sprintf(buf,"%-4s  $XX,$XXXX"  ,name);
    }
    break;
  }
  //
  // Now disassemble the remaining line dependent on the number of arguments
  // we pulled off (one to three).
  pctarget = Monitor::Symbol::FindSymbol(monitor->symboltable,pc,Symbol::Any,Symbol::Absolute);
  if (pctarget) {
    sprintf(pcbuf,"$%04x:%.16s",pc,pctarget->name);
  } else {
    sprintf(pcbuf,"$%04x:",pc);
  }
  switch(where - pc) {
  case 1:
    sprintf(line,"%-22s %02x        %s",pcbuf,inst,buf);
    break;
  case 2:
    sprintf(line,"%-22s %02x %02x     %s",pcbuf,inst,adr->ReadByte(pc+1),buf);
    break;
  case 3:
    sprintf(line,"%-22s %02x %02x %02x  %s",pcbuf,inst,adr->ReadByte(pc+1),
	    adr->ReadByte(pc+2),buf);
    break;
  }
  //
  // Check whether we are at the CPU PC position. If so, print
  // a star behind the address.
  if (pc == monitor->cpu->PC()) {
    line[32] = '*';
  }
  // Check whether we have a break point at this position.
  if (monitor->cpu->IfBreakPoint(pc)) {
    line[31] = 'B';
  }

  return where;
}
///

/// Monitor::UnAs::Apply
void Monitor::UnAs::Apply(char e)
{
  int i;
  char buffer[80];
  
  switch(e) {
  case '?':
    Print("UNAS.L [addr]  : unassemble at address.\n"
	  "UNAS.V [lines] : set number of disassembly lines.\n");
    return;
  case 'V':
    GetDefault(lines,16,1,32);
    LastArg();
    return;
  case 'L':
    if (GetAddress(here)) {
      if (LastArg()) {
	for(i=0;i<lines;i++) {
	  here = (UWORD)DisassembleLine(Adr(),here,buffer);
	  Print("%s\n",buffer);
	}
      }
    }  
    return;
  default:
    ExtInvalid();   
    return;
  }
}
///

/// Monitor::UnAs::InstructionSize
// Get the size in bytes the given instruction spawns
int Monitor::UnAs::InstructionSize(UBYTE inst)
{  
  Instruction::OperandType type;
  int size = 1; // Just the operand itself.

  type = monitor->cpu->Disassemble(inst).AddressingMode;
  switch(type) {
  case Instruction::NoArgs:
  case Instruction::Accu:
    break;
  case Instruction::Immediate:
  case Instruction::ZPage:
  case Instruction::ZPage_X:
  case Instruction::ZPage_Y:
  case Instruction::Indirect_X:
  case Instruction::Indirect_Y:
  case Instruction::Indirect_Z:
  case Instruction::Disp:
    size++;
    break;
  case Instruction::Indirect:
  case Instruction::Absolute:
  case Instruction::Absolute_X:
  case Instruction::Absolute_Y:
  case Instruction::AbsIndirect_X:
  case Instruction::ZPage_Disp:
    size += 2;
    break;
  }
  return size;
}
///

/// Monitor::Regs::Regs
Monitor::Regs::Regs(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,0)
{ }
///

/// Monitor::Regs::Apply
// Display the register map of the CPU
void Monitor::Regs::Apply(char e)
{
  switch(e) {
  case '?':
    Print("REGS does not take any extensions or arguments.\n");
    return;
  default:
    if (LastArg()) {
      monitor->PrintCPUStatus();
    }
    return;
  }
}
///

/// Monitor::Step::Step
Monitor::Step::Step(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'I'), lineaddresses(NULL), addressedlines(0)
{ }
///

/// Monitor::Step::OpenDisplay
// Create the optimized tracder display
bool Monitor::Step::OpenDisplay(void)
{
#ifdef USE_CURSES
  // Enter the smart tracing mode if it is available.
  if (lineaddresses == NULL) {
    int x,y,maxx,maxy;
    int c;
    bool abort = false;
    WINDOW *window = (WINDOW *)monitor->curses->window;
    //
    Print("Entering trace mode, key assignments are:\n"
	  "Z,F11:  Single Step\n"
	  "N,F10:  Step Over\n"
	  "U,F6 :  Finish Call\n"
	  "G,F5 :  Start Program\n"
	  "B    :  Set Breakpoint at PC\n"
	  "        All other keys abort\n\n"
	  "Press any key to continue\n");
    //
    do {
      c = getch();
      switch(c) {
      case ERR:
	break;
      default:
	move(0,0);
	clear();
	monitor->Splt.UpdateSplit();
	getyx(window,y,x);  
	getmaxyx(window,maxy,maxx);
	addressedlines = maxy - y;
	if (addressedlines > 0) {
	  topyline      = y;
	  lineaddresses = new ADR[addressedlines];
	  //
	  // Refresh the contents of the tracer window.
	  Refresh(monitor->cpuspace,monitor->cpu->PC());
	}
	abort = true;
	break;
      }
    } while(!abort);
  }
#endif
  //
  // Everything done, and curses available? Bail out.
  if (lineaddresses)
    return true;
  return false;
}
///

/// Monitor::Step::Refresh
// Refresh the contents of the tracer output
void Monitor::Step::Refresh(class AdrSpace *adr,ADR pc)
{
#ifdef USE_CURSES
  if (lineaddresses) {
    char buffer[80];
    int y = topyline;
    int lines;
    clear();
    monitor->Splt.UpdateSplit();
    for(lines = 0;lines < addressedlines;lines++,y++) {
      lineaddresses[lines] = pc;
      pc = monitor->UnAs.DisassembleLine(adr,pc,buffer);
      move(y,0);
      if (lines == 0)
	InverseOn();
      addstr(buffer);
      if (lines == 0)
	InverseOff();
    }
    refresh();
  }
#endif
}
///

/// Monitor::Step::RefreshLine
// Refresh a single line at the given PC/address space
// in the step output window. Possibly rebuild the full
// screen if applicable
bool Monitor::Step::RefreshLine(class AdrSpace *adr,ADR pc,bool showpc)
{
#ifdef USE_CURSES
  // Check whether we have line buffers.
  if (lineaddresses) {
    char buffer[80];
    int y = topyline;
    int lines;
    // Check whether we find the PC inside here. If not, run a complete
    // refresh from the given line on.
    for(lines = 0;lines < addressedlines;lines++,y++) {
      if (lineaddresses[lines] == pc) {
	monitor->UnAs.DisassembleLine(adr,pc,buffer);
	if (!showpc)
	  buffer[32] = ' ';
	move(y,0);
	deleteln();
	insertln();
	if (showpc) 
	  InverseOn();
	addstr(buffer);
	if (showpc)
	  InverseOff();
	refresh();
	return true;
      }
    }
    // Not found? Ok, run a complete refresh.
    Refresh(adr,pc);
    return true;
  }
#endif
  //
  // Not open.
  return false;
}
///

/// Monitor::Step::CloseDisplay
// Remove the tracer display
void Monitor::Step::CloseDisplay(void)
{
#ifdef USE_CURSES
  if (lineaddresses) {
    //
    delete[] lineaddresses;
    lineaddresses = NULL;
    clear();
    move(0,0);
    monitor->Splt.UpdateSplit();
  }
#endif
}
///

/// Monitor::Step::MainLoop
// Enter the main loop of the step/trace control
// Returns true in case the request was handled
// and the main loop is not required.
bool Monitor::Step::MainLoop(void)
{ 
#ifdef USE_CURSES
  int inst,s,c;
  bool abort = false;

  monitor->cpu->DisableStack();
  monitor->cpu->DisablePC();
  if (monitor->tracefile == NULL)
    monitor->cpu->DisableTrace();  
  monitor->fetchtrace = false;
  monitor->Splt.UpdateSplit();
  
  do { 
    if (monitor->machine->hasGUI())
      monitor->machine->Display()->SwitchScreen(false); 
    // Update the display now by running a VBI cycle of the machine
    // in "paused" mode.
    monitor->machine->RefreshDisplay();
    refresh();
    c = getch();
    switch(c) {
    case ERR:
      break;
    case KEY_F(11):
    case 'z':
    case 'Z':
    case 'y':
    case 'Y':
    case ' ':
      // Single step
      monitor->cpu->EnableTrace();
      monitor->fetchtrace = true;
      abort      = true;
      break;
    case KEY_F(10):
    case 'n':
    case 'N':
    case 't':
    case 'T':
      // Next command.
      // Check the CPU command at the current address. If this is a branch instruction,
      // we continue until we arrive at a PC larger than the current address. Otherwise,
      // we continue if the current stack contents is restored.
      inst = monitor->cpuspace->ReadByte(monitor->cpu->PC());
      if ((inst & 0x0f) == 0 && (inst & 0x10)) {
	// All branch instructions
	monitor->cpu->EnablePC();
      } else {
	monitor->cpu->EnableStack();
      }  
      monitor->fetchtrace = true;
      abort = true;
      break;
    case KEY_F(5):
    case 'g':
    case 'G':
      // Go
      abort = true;
      break;
    case 'b':
    case 'B':
      // Set breakpoint
      monitor->BrkP.ToggleBreakpoint(monitor->cpu->PC());
      RefreshLine(monitor->cpuspace,monitor->cpu->PC());
      break;
    case KEY_F(6):
    case 'u':
    case 'U':
      // Go Until
      s = monitor->cpu->S();
      // Increase the stack pointer for a moment to have the
      // stack checking breakpoint checking for a higher stack,
      // and not this one.
      if (s < 0xff) {
	monitor->cpu->S() = s+1;
	monitor->cpu->EnableStack();
	--monitor->cpu->S();
	monitor->fetchtrace = true;
      }
      abort = true;
      break;
    case 's':
    case 'S': 
      // Skip instruction
      {
	ADR pc   = monitor->cpu->PC();
	UBYTE in = monitor->cpuspace->ReadByte(pc);
	monitor->cpu->PC() += monitor->UnAs.InstructionSize(in);
	RefreshLine(monitor->cpuspace,pc,false);
	RefreshLine(monitor->cpuspace,monitor->cpu->PC()); 
	monitor->Splt.UpdateSplit();
      }
      break;
    case 'q':
    case 'Q':
    case 'x':
    case 'X':
    default:
      CloseDisplay();
      abort = true;
      return false;
    }
  } while(!abort);
  //
  // Rewrite the current line again, with the PC disabled.
  RefreshLine(monitor->cpuspace,monitor->cpu->PC(),false);
  // Make sure we don't re-enter the monitor immediately
  monitor->machine->LaunchMonitor() = false;
  monitor->machine->Display()->EnforceFullRefresh();    
  // Switch the monitor to background so we see the emulator going 
  monitor->machine->Display()->SwitchScreen(true);
  monitor->curses                   = NULL;
#endif
  return true;
}
///

/// Monitor::Step::Apply
// Single step thru the code
void Monitor::Step::Apply(char e)
{
  switch(e) {
  case '?': 
    Print("STEP.S           : single step over the next instruction\n"
	  "STEP.I           : use interactive tracing window\n");
    return;
  case 'S':
    if (LastArg()) {
      monitor->cpu->DisableStack();
      monitor->cpu->DisablePC();
      monitor->cpu->EnableTrace();
      monitor->fetchtrace = true;
      monitor->abort      = true;
    }
    break;
  case 'I':
    if (LastArg()) {
      /*
      char buffer[80];
      monitor->UnAs.DisassembleLine(monitor->currentadr,monitor->cpu->PC(),buffer);
      Print("%s\n",buffer);
      monitor->PrintCPUStatus();
      */
      if (OpenDisplay()) {
	if (MainLoop()) {
	  monitor->abort = true;
	}
      } else {
	// traditional approach
	monitor->cpu->DisableStack();
	monitor->cpu->DisablePC();
	monitor->cpu->EnableTrace();
	monitor->fetchtrace = true;
	monitor->abort      = true;
      }
    }
    return; 
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::Next::Next
Monitor::Next::Next(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,0)
{ }
///

/// Monitor::Next::Apply
// Step over subroutine calls
void Monitor::Next::Apply(char e)
{
  switch(e) {
  case '?':
    Print("NEXT does not take any extensions or arguments.\n");
    return;
  default:
    if (LastArg()) {
      UBYTE inst;
      /*
      char buffer[80];
      monitor->UnAs.DisassembleLine(monitor->currentadr,monitor->cpu->PC(),buffer);
      Print("%s\n",buffer);
      monitor->PrintCPUStatus();
      */
      //
      // Check the CPU command at the current address. If this is a branch instruction,
      // we continue until we arrive at a PC larger than the current address. Otherwise,
      // we continue if the current stack contents is restored.
      inst = monitor->cpuspace->ReadByte(monitor->cpu->PC());
      if ((inst & 0x0f) == 0 && (inst & 0x10)) {
	// All branch instructions
	monitor->cpu->EnablePC();
      } else {
	monitor->cpu->EnableStack();
      }
      monitor->fetchtrace = true;
      monitor->abort = true;
    }
    return;
  }
}
///

/// Monitor::Help::Help
Monitor::Help::Help(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,0)
{ }
///

/// Monitor::Help::Apply
// Print the help status over the line
void Monitor::Help::Apply(char)
{
  struct Command *cmd = this;
  // Print the list of all commands
  Print("Atari++ Monitor command summary:\n"
	"Type <command>.? for a list of subtopics.\n\n");
  //
  // Note that cmd is the first in the command "stack" since we
  // added it last.
  while(cmd) {
    Print("%s %s : %s\n",cmd->longname,cmd->shortname,cmd->helptext);
    cmd = cmd->next;
  }
}
///

/// Monitor::Stat::Stat
Monitor::Stat::Stat(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'S')
{ }
///

/// Monitor::Stat::Apply
// The status command implementation
void Monitor::Stat::Apply(char e)
{
  char *token;
  class Chip *chip;
  
  switch(e) {
  case '?':
    Print("STAT.L           : list all emulator components.\n"
	  "STAT.S component : display the status of the named component\n");
    return;
  case 'L':
    Print("Available emulator components:\n");
    chip = monitor->machine->ChipChain().First();
    while(chip) {
      Print("%s\n",chip->NameOf());
      chip = chip->NextOf();
    }
    return;
  case 'S':
    token = NextToken();
    chip  = monitor->machine->ChipChain().First();
    if (token) {
      while(chip) {
	if (!strcasecmp(token,chip->NameOf())) {
	  chip->DisplayStatus(monitor);
	  return;
	}
	chip = chip->NextOf();
      }
      Print("Unknown emulator component %s\n",token);
      return;
    }
    MissingArg();
    break;
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::Edit::Edit
Monitor::Edit::Edit(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'X'), inverse(false)
{ }
///

/// Monitor::Edit::Apply
// The line editor
void Monitor::Edit::Apply(char e)
{
  switch(e) {
  case '?':
    Print("Edit subcommands:\n"
	  "EDIT.X [addr]  : edit memory in hex\n"
	  "EDIT.D [addr]  : edit memory in dec\n"
	  "EDIT.A [addr]  : edit memory as bytes as ATASCII\n"
	  "EDIT.S [addr]  : edit memory as ANTIC bytes\n"
	  "EDIT.I         : toggle inverse mode on/off\n"
	  );
    break;
  case 'I':
    inverse = !inverse;
    Print("Entered characters are now interpreted as %s.\n",
	  (inverse)?("inverse"):("regular"));
    break;
  case 'D':
  case 'A':
  case 'X':
  case 'S':
    if (GetAddress(here)) {
      do {
	UBYTE *p;
	int   count;
	char  adbuffer[9];
	UBYTE buffer[128];
	//
	sprintf(adbuffer,"$%04x : ",here);
	// Now just read the data from the command service call.
	count =     ReadDataLine(buffer,adbuffer,e,inverse);
	if (count == 0)
	  break;
	// Now fill the data in.
	p = buffer;
	do {
	  Adr()->WriteByte(here++,*p++);
	} while(--count);
      } while(true);
    }
    break;
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::Fill::Fill
Monitor::Fill::Fill(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'X'), inverse(false)
{ }
///

/// Monitor::Fill::Apply
// The pattern filler
void Monitor::Fill::Apply(char e)
{
  switch(e) {
  case '?':
    Print("Fill subcommands:\n"
	  "FILL.X addr size  : fill memory in hex\n"
	  "FILL.D addr size  : fill memory in dec\n"
	  "FILL.A addr size  : fill memory as bytes as ATASCII\n"
	  "FILL.S addr size  : fill memory as ANTIC bytes\n"
	  "FILL.I            : toggle inverse mode on/off\n"
	  );
    break;
  case 'I':
    inverse = !inverse;
    Print("Entered characters are now interpreted as %s.\n",
	  (inverse)?("inverse"):("regular"));
    break;
  case 'D':
  case 'A':
  case 'X':
  case 'S':
    if (GetAddress(here)) {
      int size;
      if (GetDefault(size,1,1,65536)) {
	UBYTE *s;
	UWORD  d;
	int    c,count;
	UBYTE buffer[128];
	// Now just read the data from the command service call.
	count = ReadDataLine(buffer,"Pattern > ",e,inverse);
	if (count == 0)
	  break;
	// Fill in the address to fill to.
	d = here;
	s = buffer;
	c = count;
	do {
	  Adr()->WriteByte(d++,*s++);
	  if (--c == 0) {
	    // pattern wrap around
	    c = count;
	    s = buffer;
	  }
	} while(--size);
      }
    }
    break;
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::Move::Move
Monitor::Move::Move(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'S')
{ }
///

/// Monitor::Move::Apply
// The memory mover
void Monitor::Move::Apply(char e)
{  
  class AdrSpace *src,*dst;
  UWORD from,to;
  int size;
  
  switch(e) {
  case '?':
    Print("Move subcommands:\n"
	  "MOVE.S from to size : simple memory move\n"
	  "MOVE.C from to size : move to CPU space\n"
	  "MOVE.A from to size : move to ANTIC space\n"
	  );
    break;
  case 'S':
  case 'C':
  case 'A':
    if (GetAddress(here)) {
      from = here;
      to   = here;
      if (GetAddress(to)) {
	if (GetDefault(size,1,1,65535)) {
	  dst = src = Adr();
	  switch(e) {
	  case 'C':
	    dst = monitor->cpuspace;
	    break;
	  case 'A':
	    dst = monitor->anticspace;
	  }
	  // Now check whether we must move upwards or downwards.
	  if (from < to) {
	    // Must move downwards
	    from += size;
	    to   += size;
	    do {
	      to--;
	      from--;
	      dst->WriteByte(to,src->ReadByte(from));
	    } while(--size);
	  } else {
	    // Must move upwards
	    do {
	      dst->WriteByte(to,src->ReadByte(from));
	      to++;
	      from++;
	    } while(--size);
	  }
	}
      }
    }
    break;
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::Find::Find
Monitor::Find::Find(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'X'), inverse(false), lines(10)
{ }
///

/// Monitor::Find::Apply
// The pattern searcher
void Monitor::Find::Apply(char e)
{
  switch(e) {
  case '?':
    Print("Find subcommands:\n"
	  "FIND.X [expr] : find hex pattern in memory\n"
	  "FIND.D [expr] : find decimal pattern in memory\n"
	  "FIND.A [expr] : find ASCII text in memory\n"
	  "FIND.S [expr] : find screen text in memory\n"
	  "FIND.I        : toggle inverse mode on/off\n"
	  "FIND.V  expr  : set number of matches to show\n"
	  );
    break;
  case 'V':
    GetDefault(lines,10,1,256);
    break;
  case 'I':
    inverse = !inverse;
    Print("Entered characters are now interpreted as %s.\n",
	  (inverse)?("inverse"):("regular"));
    break;
  case 'D':
  case 'A':
  case 'X':
  case 'S':
    if (GetAddress(here)) {
      int count,msize;
      UBYTE buffer[128],mask[128];
      UWORD s;
      int lc = lines;
      // Now just read the data from the command service call.
      count = ReadDataLine(buffer,"Pattern > ",e,inverse);
      if (count == 0)
	break;
      msize = ReadDataLine(mask,"Hex Mask> ",'X',false);
      if (msize == 0) {
	// No mask, fill mask with 0xff
	memset(mask,0xff,count);
      } else if (msize != count) {
	Print("Pattern and mask size do not match.\n");
	return;
      }
      s = here;
      do {
	UWORD hc = s;
	UBYTE *p = buffer,*m = mask;
	int    c = count;
	// Compare at the given address 
	do {
	  if ((Adr()->ReadByte(hc) ^ *p) & *m)
	    break; // does not match;
	  // Compare the next byte.
	  p++,m++,hc++;
	} while(--c);
	// If c is now zero, then we have a match.
	if (c == 0) {
	  Print("Match found at 0x%04x\n",s);
	  if (--lc <= 0) {
	    // correct here pointer for next go.
	    here = s+1;
	    break;
	  }
	}
	s++;
      } while(s != here);
    }
    break;
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::SetR::SetR
Monitor::SetR::SetR(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,0)
{ }
///

/// Monitor::SetR::Apply
// Define the register map of the CPU
void Monitor::SetR::Apply(char e)
{
  char *setstr;
  char *valstr;
  LONG value;
  
  switch(e) {
  case '?':
    Print("SETR <register>=value\n");
    return;
  default:
    setstr = NextToken();
    if (setstr == NULL) {
      MissingArg();
      return;
    }
    if (!LastArg())
      return;
    valstr = strchr(setstr,'=');
    if (!valstr) {
      Print("Missing = sign for register definition.\n");
      return;
    }
    *valstr++ = '\0';
    value     = 0;
    if (monitor->EvaluateExpression(valstr,value)) {
      if (value<0x0000 || value > 0xffff) {
	Print("Register value " ATARIPP_LX " out of range\n",(ULONG)value);
	return;
      }
      if (!strcasecmp(setstr,"A")) {
	if (value > 0xff) {
	  Print("Register value " ATARIPP_LX " out of range\n",(ULONG)value);
	  return;
	}
	monitor->cpu->A() = UBYTE(value);
      } else if (!strcasecmp(setstr,"X")) {
	if (value > 0xff) {
	  Print("Register value " ATARIPP_LX " out of range\n",(ULONG)value);
	  return;
	}
	monitor->cpu->X() = UBYTE(value);
      } else if (!strcasecmp(setstr,"Y")) {
	if (value > 0xff) {
	  Print("Register value " ATARIPP_LX " out of range\n",(ULONG)value);
	  return;
	}
	monitor->cpu->Y() = UBYTE(value);
      } else if (!strcasecmp(setstr,"S")) {
	if (value > 0xff) {
	  Print("Register value " ATARIPP_LX " out of range\n",(ULONG)value);
	  return;
	}
	monitor->cpu->S() = UBYTE(value);
      } else if (!strcasecmp(setstr,"P")) {
	if (value > 0xff) {
	  Print("Register value " ATARIPP_LX " out of range\n",(ULONG)value);
	  return;
	}
	monitor->cpu->P() = UBYTE(value);
      } else if (!strcasecmp(setstr,"PC")) {
	monitor->cpu->PC() = UWORD(value);
      } else {
	Print("Invalid CPU register %s.\n",setstr);
	return;
      }
    }
  }
}
///

/// Monitor::SkTb::SkTb
Monitor::SkTb::SkTb(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,0)
{ }
///

/// Monitor::SkTb::Apply
void Monitor::SkTb::Apply(char e)
{  
  UWORD addr,back;

  switch(e) {
  case '?':
    Print("SKTB does not take any extensions or arguments.\n");
    return;
  default: 
    here = monitor->cpu->S() + 0x101;
    if (GetAddress(addr)) {
      while(addr <= 0x1fe) {
	back  = monitor->cpuspace->ReadWord(addr);
	back -= 2;
	if (monitor->cpuspace->ReadByte(back) == 0x20) { // Is a JSR
	  Print("0x%04x: call from 0x%04x\n",addr,back);
	  addr += 2;
	} else {
	  addr++;
	}
      }
    }
    return;
  }
}
///

/// Monitor::Dlst::Dlst
Monitor::Dlst::Dlst(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'L'), lines(16)
{ }
///

/// Monitor::Dlst::DisassembleLine
// Disassemble a single instruction into a buffer,
// advance the disassembly position, return it.
ADR Monitor::Dlst::DisassembleLine(class AdrSpace *adr,ADR where,char *line)
{
  UBYTE inst;
  bool dli     = false;
  bool hscroll = false;
  bool vscroll = false;
  bool load    = false;
  bool waitvbr = false;
  char cmdname[20],prehex[20];
  char options[33];

  inst    = adr->ReadByte(where);
  if (inst & 0x80) {
    dli = true;
  }
  if ((inst & 0x0f) == 0x00) {
    // blank line displays
    sprintf(cmdname,"Blank #%1x",((inst & 0x70) >> 4) + 1);
  } else if ((inst & 0x0f) == 0x01) {
    if (inst & 0x40) {
      waitvbr = true;
    }
    sprintf(cmdname,"Jump   ");
    load = true;
  } else {
    if (inst & 0x10) {
      hscroll = true;
    }
    if (inst & 0x20) {
      vscroll = true;
    } 
    if (inst & 0x40) {
      load    = true;
    }
    switch(inst & 0x0f) {
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
      sprintf(cmdname,"Text  #%1x",inst & 0x0f);
      break;
    case 0x08:
    case 0x0a:
    case 0x0d:
    case 0x0e:
      sprintf(cmdname,"Color #%1x",inst & 0x0f);
      break;
    case 0x09:
    case 0x0b:
    case 0x0c:
    case 0x0f:
      sprintf(cmdname,"Mono  #%1x",inst & 0x0f);
      break;
    }
  }

  if (load) {
    UBYTE ins,lo,hi;
    ins = adr->ReadByte(where);
    lo  = adr->ReadByte(where+1);
    hi  = adr->ReadByte(where+2);
    
    sprintf(prehex,"$%04x: %02x %02x %02x  ",where,ins,lo,hi);
  } else {
    sprintf(prehex,"$%04x: %02x        "    ,where,adr->ReadByte(where));
  }
  
  strcpy(options,"");
  if (waitvbr) 
    strcat(options," WaitVBR");
  if (hscroll)
    strcat(options," HScroll");
  if (vscroll)
    strcat(options," VScroll");
  if (dli)
    strcat(options," DLI");
  
  if (load) {
    snprintf(line,80,"%s %s @$%04x %s",prehex,cmdname,adr->ReadWord(where+1),options);
    where += 3;
  } else {
    snprintf(line,80,"%s %s %s",prehex,cmdname,options);
    where++;
  }

  return where;
}
///

/// Monitor::Dlst::Apply
// Disassemble an Antic display list contents
void Monitor::Dlst::Apply(char e)
{
  char buffer[80];
  
  switch (e) {
  case '?':
    Print("DLST.L [addr] : disassemble the display list at address\n"
	  "DLST.S        : show the Antic status\n"
	  "DLST.V        : set the number of lines\n");
    return;
  case 'V':
    GetDefault(lines,16,1,32);
    LastArg();
    return;
  case 'L':
    if (GetAddress(here)) {
      if (LastArg()) {
	int i;
	for(i=0;i<lines;i++) {
	  here = DisassembleLine(monitor->anticspace,here,buffer);
	  Print("%s\n",buffer);
	}
      }
    }
    return;
  case 'S':
    if (LastArg()) {
      monitor->machine->Antic()->DisplayStatus(monitor);
    }
    return;
  default:
    ExtInvalid();
  }
}
///

/// Monitor::Envi::Envi
Monitor::Envi::Envi(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'A')
{ }
///

/// Monitor::Envi::Apply
// Various monitor settings
void Monitor::Envi::Apply(char e)
{
  const char *token;
  
  switch(e) {
  case '?':
    Print("ENVI.A : toggle between CPU and ANTIC address space\n");
    Print("ENVI.L [filename] : set tracing output file\n");
    Print("ENVI.S [filename] : load ld65 debug symbols from file\n");
    Print("ENVI.C : clear symbol table\n");
    break;
  case 'A':
    if (monitor->currentadr == monitor->cpuspace) {
      monitor->currentadr = monitor->anticspace;
      Print("Current address space is ANTIC.\n");
    } else {
      monitor->currentadr = monitor->cpuspace;
      Print("Current address space is CPU.\n");
    }
    break;
  case 'C':
    monitor->ClearSymbolTable();
    Print("Symbol table removed.\n");
    break;
  case 'S':
    token = NextToken();
    if (token) {
      if (monitor->ParseSymbolTable(token)) {
	Print("Symbols from %s added to the symbol table.\n",token);
      } else {
	Print("No symbols found in %s.\n",token);
      }
    }
    break;
  case 'L':
    if (monitor->tracefile) {
      fclose(monitor->tracefile);
      monitor->tracefile = NULL;
      if (!monitor->fetchtrace)
	monitor->cpu->DisableTrace();
    }
    token = NextToken();
    if (token) {
      monitor->tracefile = fopen(token,"w");
      if (monitor->tracefile) {
	monitor->cpu->EnableTrace();
	Print("Tracing enabled, trace output written to %s.\n",token);
      }
    } else {
      Print("Tracing disabled.\n");
    }
    break;
  default:
    ExtInvalid();
  }
}
///

/// Monitor::BrkP::BrkP
Monitor::BrkP::BrkP(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'S')
{ }
///

/// Monitor::BrkP::ToggleBreakpoint
// Toggle the break point at the given address on/off
void Monitor::BrkP::ToggleBreakpoint(ADR here)
{
  int i;

  for(i=0;i<NumBrk;i++) {
    if (BreakPoints[i].id >= 0 && BreakPoints[i].address == here) {
      monitor->cpu->ClearBreakPoint(BreakPoints[i].id);
      BreakPoints[i].id = -1;
      return;
    }
  }

  // Here, nothing found at the address, set a breakpoint
  for(i=0;i<NumBrk;i++) {
    if (BreakPoints[i].id == -1) {
      BreakPoints[i].id      = monitor->cpu->SetBreakPoint(here);
      if (BreakPoints[i].id >= 0) {
	monitor->cpu->EnableBreakPoint(BreakPoints[i].id);
	BreakPoints[i].address = here;
	BreakPoints[i].enabled = true;
	return;
      }
    }
  }
}
///

/// Monitor::BrkP::Apply
// Set and clear breakpoints, general breakpoint logic
void Monitor::BrkP::Apply(char e)
{
  int i;
  class CPU *cpu = monitor->cpu;
  class MMU *MMU = monitor->MMU;
  
  switch(e) {
  case '?':
    Print("BRKP.S [addr] : set breakpoint at address\n"
	  "BRKP.W [addr] : set write only watchpoint at address\n"
	  "BRKP.V [addr] : set read/write watchpoint at address\n"
	  "BRKP.C [addr] : clear breakpoint at address\n"
	  "BRKP.D [addr] : disable breakpoint at address\n"
	  "BRKP.E [addr] : enable breakpoint at address\n"
	  "BRKP.A        : clear all breakpoints\n"
	  "BRKP.L        : list all breakpoints\n");
    return;
  case 'S':
    if (GetAddress(here)) {
      if (LastArg()) {
	for(i=0;i<NumBrk;i++) {
	  if (BreakPoints[i].id >= 0 && BreakPoints[i].address == here) {
	    Print("Already breakpoint at address : $%04x\n",here);
	    return;
	  }
	}
	for(i=0;i<NumBrk;i++) {
	  if (BreakPoints[i].id == -1) {
	    BreakPoints[i].id      = cpu->SetBreakPoint(here);
	    if (BreakPoints[i].id >= 0) {
	      cpu->EnableBreakPoint(BreakPoints[i].id);
	      BreakPoints[i].address = here;
	      BreakPoints[i].enabled = true;
	      Print("Installed breakpoint at address : $%04x\n",here);
	      return;
	    }
	  }
	}
	Print("All breakpoint slots occupied.\n");
      }
    }
    break;  
  case 'W':
  case 'V':
    if (GetAddress(here)) {
      if (LastArg()) {
	for(i=0;i<NumBrk;i++) {
	  if (WatchPoints[i].id >= 0 && WatchPoints[i].address == here) {
	    Print("Already watchpoint at address : $%04x\n",here);
	    return;
	  }
	}
	for(i=0;i<NumBrk;i++) {
	  if (WatchPoints[i].id == -1) {
	    WatchPoints[i].id = MMU->DebugRAM()->SetWatchPoint(here,e == 'V');
	    if (WatchPoints[i].id >= 0) {
	      cpu->EnableWatchPoints();
	      WatchPoints[i].address = here;
	      WatchPoints[i].enabled = true;
	      WatchPoints[i].read    = (e == 'V');
	      Print("Installed watchpoint at address : $%04x\n",here);
	      return;
	    }
	  }
	}
	Print("All watchpoint slots occupied.\n");
      }
    }
    break;
  case 'C':
    if (GetAddress(here)) {
      if (LastArg()) {
	for(i=0;i<NumBrk;i++) {
	  if (BreakPoints[i].id >= 0 && BreakPoints[i].address == here) {
	    cpu->ClearBreakPoint(BreakPoints[i].id);
	    BreakPoints[i].id = -1;
	    Print("Removed breakpoint at address : $%04x\n",here);
	    return;
	  }
	  if (WatchPoints[i].id >= 0 && WatchPoints[i].address == here) {
	    if (WatchPoints[i].enabled)
	      MMU->DebugRAM()->RemoveWatchPointByIndex(WatchPoints[i].id);
	    WatchPoints[i].id = -1;
	    Print("Removed watchpoint at address : $%04x\n",here);
	    if (MMU->DebugRAM()->WatchesEnabled()) {
	      cpu->DisableWatchPoints();
	    }
	    return;
	  }
	}
	Print("No breakpoint or watchpoint at address : $%04x\n",here);
      }
    }
    break;
  case 'D':
    if (GetAddress(here)) {
      if (LastArg()) {
	for(i=0;i<NumBrk;i++) {
	  if (BreakPoints[i].id >= 0 && BreakPoints[i].address == here) {
	    cpu->DisableBreakPoint(BreakPoints[i].id);
	    BreakPoints[i].enabled = false;
	    Print("Disabled breakpoint at address : $%04x\n",here);
	    return;
	  }
	}
	for(i=0;i<NumBrk;i++) {
	  if (WatchPoints[i].id >= 0 && WatchPoints[i].address == here) {
	    MMU->DebugRAM()->RemoveWatchPointByIndex(WatchPoints[i].id);
	    WatchPoints[i].enabled = false;
	    if (MMU->DebugRAM()->WatchesEnabled()) {
	      cpu->DisableWatchPoints();
	    }
	    Print("Disabled watchpoint at address : $%04x\n",here);
	    return;
	  }
	}
	Print("No breakpoint or watchpoint at address : $%04x\n",here);
      }
    }
    break;
  case 'E':
    if (GetAddress(here)) {
      if (LastArg()) {
	for(i=0;i<NumBrk;i++) {
	  if (BreakPoints[i].id >= 0 && BreakPoints[i].address == here) {
	    cpu->EnableBreakPoint(BreakPoints[i].id);
	    BreakPoints[i].enabled = true;
	    Print("Enabled breakpoint at address : $%04x\n",here);
	    return;
	  }
	}	
	for(i=0;i<NumBrk;i++) {
	  if (WatchPoints[i].id >= 0 && WatchPoints[i].address == here) {
	    WatchPoints[i].id = MMU->DebugRAM()->SetWatchPoint(here,WatchPoints[i].read);
	    if (WatchPoints[i].id >= 0) {
	      cpu->EnableWatchPoints();
	      WatchPoints[i].address = here;
	      WatchPoints[i].enabled = true;
	      Print("Enabled watchpoint at address : $%04x\n",here);
	      return;
	    } else {
	      WatchPoints[i].id = -1;
	      return;
	    }
	  }
	}
	Print("No breakpoint or watchpoint at address : $%04x\n",here);
      }
    }
    break;
  case 'A':
    if (LastArg()) {
      for(i=0;i<NumBrk;i++) {
	if (BreakPoints[i].id >= 0) {
	  cpu->ClearBreakPoint(BreakPoints[i].id);
	  BreakPoints[i].id = -1;
	} 
      }
      for(i=0;i<NumBrk;i++) {
	if (WatchPoints[i].id >= 0) {
	  MMU->DebugRAM()->RemoveWatchPointByIndex(WatchPoints[i].id);
	  WatchPoints[i].id = -1;
	}
      }
      cpu->DisableWatchPoints();
      Print("All breakpoints removed.\n");
    }
    break;
  case 'L':
    if (LastArg()) {
      for(i=0;i<NumBrk;i++) {
	if (BreakPoints[i].id >= 0) {
	  Print("Breakpoint at $%04x (%s)\n",
		BreakPoints[i].address,
		(BreakPoints[i].enabled)?("enabled"):("disabled"));
	}
      }
      for(i=0;i<NumBrk;i++) {
	if (WatchPoints[i].id >= 0) {
	  Print("Watchpoint(%s) at $%04x (%s)\n",
		(WatchPoints[i].read)?("read/write"):("write only"),
		WatchPoints[i].address,
		(WatchPoints[i].enabled)?("enabled"):("disabled"));
	}
      }
    }
    break;
  default:
    ExtInvalid();
  }
}
///

/// Monitor::RSet::RSet
Monitor::RSet::RSet(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'W')
{ }
///

/// Monitor::RSet::Apply
// Reset system command
void Monitor::RSet::Apply(char e)
{
  switch(e) {
  case '?':
    Print("RSET.W : initiate a warm start reset.\n"
	  "RSET.C : initiate a cold start reset.\n"
	  "RSET.I : pull the Antic NMI reset line.\n"
	  );
    return;
  case 'W':
    Print("Warm starting the system....\n");
    // Switch the monitor to background so we see the emulator going
    monitor->machine->Display()->SwitchScreen(true);
    WARMSTART;
    return;
  case 'C':
    Print("Cold starting the system....\n");
    //
    // The following has to be done asynchronously as we
    // would otherwise pull the rug under the ANTIC emulation by
    // resetting the display front end.
    //
    // Switch the monitor to background so we see the emulator going
    monitor->machine->Display()->SwitchScreen(true);
    COLDSTART;
    return;
  case 'I':
    Print("Signalling a RESET to the ANTIC NMI input...\n");
    // This just pulls the NMI signal at ANTIC. As there is no detection
    // in the ROMs of the XLs and XEs, it does little for these
    // models.
    monitor->machine->Antic()->ResetNMI();
    return;
  default:
    ExtInvalid();
  }
}
///

/// Monitor::GoPG::GoPG
Monitor::GoPG::GoPG(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'P')
{ }
///

/// Monitor::GoPG::Apply
// Leave the monitor, exit to the CPU again.
void Monitor::GoPG::Apply(char e)
{
  switch(e) {
  case '?':
    Print("GOPG.P : restart the program at current PC.\n"
	  "GOPG.U : run until the stack pointer increases.\n"
	  "GOPG.M : enter setup menu.\n");
    return;
  case 'P':
    Print("Rerunning the emulator from $%04x\n",monitor->cpu->PC());
    monitor->abort = true;
    return;
  case 'U':
    {
      UBYTE s = monitor->cpu->S();
      // Increase the stack pointer for a moment to have the
      // stack checking breakpoint checking for a higher stack,
      // and not this one.
      if (s < 0xff) {
	monitor->cpu->S() = s+1;
	monitor->cpu->EnableStack();
	monitor->fetchtrace = true;
	--monitor->cpu->S();
      }
    }
    monitor->abort = true;
    return;
  case 'M':
    throw AsyncEvent(AsyncEvent::Ev_EnterMenu);
  }
}
///

/// Monitor::Exit::Exit
Monitor::Exit::Exit(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,0)
{ }
///

/// Monitor::Exit::Apply
// Leave the monitor and the emulation system completely
void Monitor::Exit::Apply(char e)
{
  switch(e) {
  case '?':
    Print("EXIT does not take any extensions\n");
    return;
  default:
    Print("Leaving Atari++ ....\n");
    EXIT;
  }
}
///

/// Monitor::Disk::Disk
Monitor::Disk::Disk(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'L')
{ }
///

/// Monitor::Disk::Apply
// Loading and saving of memory blocks
void Monitor::Disk::Apply(char e)
{
  class AdrSpace *adr;
  char *filename;
  UWORD from;
  int size;

  switch(e) {
  case '?':
    Print("Disk subcommands:\n"
	  "DISK.L file addr      : load raw memory block from disk\n"
	  "DISK.S file addr size : save raw memory block to disk\n"
	  );
    break;
  case 'L':
    filename = NextToken();
    if (filename) {
      if (GetAddress(here)) {
	FILE *f = fopen(filename,"rb"); 
	if (f) {
	  adr  = Adr();
	  from = here;
	  do {
	    int c = fgetc(f);
	    if (c < 0)
	      break;
	    adr->WriteByte(from,c);
	    from++;
	  } while(from);
	  fclose(f);
	} else {
	  Print("I/O error : %s\n",strerror(errno));
	}
      }
    } else {
      Print("file name argument missing.\n");
    }
    break;
  case 'S':
    filename = NextToken();
    if (filename) {
      if (GetAddress(here)) {
	if (GetDefault(size,1,1,65535)) {
	  FILE *f = fopen(filename,"wb"); 
	  if (f) {
	    adr  = Adr();
	    from = here;
	    do {
	      fputc(adr->ReadByte(from),f);
	      from++;
	    } while(from && --size);
	    fclose(f);
	  } else {
	    Print("I/O error : %s\n",strerror(errno));
	  }
	}
      }
    } else {
      Print("file name argument missing.\n");
    }
    break;
  default:
    ExtInvalid();   
  }
}
///

/// Monitor::Prof::Prof
// Build the profiler.
Monitor::Prof::Prof(class Monitor *mon,const char *lng,const char *shr,const char *helper)
  : Command(mon,lng,shr,helper,'L')
{ }
///

/// Monitor::Prof::Apply
// The monitor implementation.
void Monitor::Prof::Apply(char e)
{
  switch(e) {
  case '?':
    Print("Profiler subcommands:\n"
	  "PROF.S : start profiling\n"
	  "PROF.X : stop profiling\n"
	  "PROF.L : list profile data\n"
	  "PROF.C : list cumulative profiling data\n");
    break;
  case 'S':
    if (monitor->cpu->ProfilingCountersOf()) {
      Print("Profiler is already running.\n");
    } else {
      monitor->cpu->StartProfiling();
      Print("Profiling enabled.\n");
    }
    break;
  case 'X':
    if (monitor->cpu->ProfilingCountersOf()) {
      monitor->cpu->StopProfiling();
      Print("Profiler stopped.\n");
    } else {
      Print("Profiler is not running.\n");
    }
    break;
  case 'L':
  case 'C':
    {
      struct Entry {
	struct Entry *next;
	ULONG count;
	ADR   pc;
      } *entries         = NULL,*last = NULL;
      const ULONG *cntrs = (e == 'L')?
	(monitor->cpu->ProfilingCountersOf()):
	(monitor->cpu->CumulativeProfilingCountersOf());
      int lines;
      int height  = 32;
      ADR pc      = 0;
      UQUAD total = 0;
#ifdef USE_CURSES
      WINDOW *window = (WINDOW *)monitor->curses->window;
      getmaxyx(window,height,lines);
#endif
      if (cntrs == NULL) {
	Print("Profiler is currently not running. Please start the profiler first with\n"
	      "PROF.S, run the program, then use PROF.L again to show collected data.\n");
	break;
      }
      try {
	do {
	  if (cntrs[pc]) {
	    // Ok, so this address was used after all. Check whether its count is the
	    // same as that of the last entry (i.e. the PC above). If so, do not collect
	    // because it is likely the same function.
	    if (last == NULL || last->count != cntrs[pc]) {
	      struct Entry **i;
	      struct Entry *e = new struct Entry;
	      e->count = cntrs[pc];
	      e->pc    = pc;
	      total   += cntrs[pc];
	      // Run a stupid insertion sort to sort entries by size...
	      i = &entries;
	      while(*i && (*i)->count >= e->count)
		i = &((*i)->next);
	      // insert before i
	      e->next = *i;
	      *i      = e;
	      last    = e;
	    }
	  }
	  pc++;
	} while(pc != 0xffff);
	if (e == 'C')
	  total = cntrs[pc];
	//
	// Now print the profiling entries.
	last  = entries;
	lines = 0;
	while(last) {
	  const struct Symbol *target;
	  target = Monitor::Symbol::FindSymbol(monitor->symboltable,last->pc,Symbol::Label,Symbol::PreferAbsolute);
	  //
	  // Print the label name if we have it, or the label.
	  if (target) {
	    Print("%-22s %10lu (%.3f%%)\n",target->name,(unsigned long)(last->count),
		  100.0*last->count/double(total));
	  } else {
	    Print("%4x                   %10lu (%.3f%%)\n",(unsigned int)last->pc,
		  (unsigned long)(last->count),
		  100.0*last->count/double(total));
	  }
	  lines++;
	  if (lines >= (height >> 1)) {
	    const char *prompt = "*** Press RETURN to continue or Q to abort ***";
	    int in = 0;
#ifndef USE_CURSES
	    char input[64];
	    
	    printf("%s",prompt);
	    fflush(stdout);
	    monitor->machine->RefreshDisplay();
	    fgets(input,sizeof(input),stdin);
	    in = input[0];
#else
	    monitor->Print("%s",prompt);
	    do {
	      in = getch();
	    } while(in == ERR);
	    monitor->Print("\n");
#endif	   
	    if (in == 'q' || in == 'Q')
	      break;
	    lines = 0;
#ifdef USE_CURSES
	    {
	      int x,y;
	      getyx(window,y,x);
	      wmove(window,y-1,0);
	      wdeleteln(window);
	    }
#endif
	  }
	  last = last->next;
	}
	while((last = entries)) {
	  entries = entries->next;
	  delete last;
	}
      } catch(...) {
	while((last = entries)) {
	  entries = entries->next;
	  delete last;
	}
	throw;
      }
    }
    break;
  }
}
///

/// Monitor::ParseCmd
// Get the current token from the command line, interpret as an argument
// and run the corresponding entry
void Monitor::ParseCmd(char *cmdline)
{
  struct Command *cmd;
  char *token;
  char *ext,e;
  char *next;
  //
  do {
    // Scan for the end of the token if there is one.
    next = strchr(cmdline,'\n');
    if (next) {
      // Cut here, and continue next time behind the newline
      *next = '\0';
      next++;
    }
    // Initialize strtok
    strtokstart = cmdline;
    token = NextToken();
    if (token) {
      // Get the command extension
      ext   = strchr(token,'.');
      // If there is no command extesion, set the extension character to NUL
      if (ext) {
	e    = ext[1];
	*ext = '\0';
      } else {
	e    = '\0';
      }
      cmd    = cmdchain;
      //
      // Now go thru the command chain and look for the command we have here.
      while(cmd) {
	if ((!strcasecmp(token,cmd->longname)) || (!strcasecmp(token,cmd->shortname))) {
	  if (e == 0) {
	    e = cmd->lastext;
	  } else {
	    e = toupper(e);
	    if (e != '?') {
	      cmd->lastext = e;
	    }
	  }
	  cmd->Apply(e);
	  break;
	}
	cmd = cmd->next;
      }
      if (cmd == NULL) {
	Print("Unknown command %s.\n",token);
	break;
      }
    }
    // Repeat until all commands have been eaten up.
  } while((cmdline = next));
}
///

/// Monitor::PrintStatus
void Monitor::PrintStatus(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  VPrint(fmt,args);
  va_end(args);
}
///

/// Monitor::MainLoop
// This is the main event loop of the monitor. It is completely
// command line driven (for now)
void Monitor::MainLoop(bool title)
{
  char *token;

#if CHECK_LEVEL > 0
  // Someone must have build the curses output here
  if (curses == NULL) {
    Throw(ObjectDoesntExist,"Monitor::MainLoop","curses output not established");
  }
#endif
  abort = false; // do not leave it now
  if (title)
    Print("Entering Atari++ built-in monitor system.\n"
	  "Use HELP to get a list of commands,\n"
	  "use GOPG to restart the emulator and\n"
	  "use EXIT to stop the emulator.\n\n");
#ifdef USE_CURSES
  if (title)
    Print("Shortcuts:\n"
	  "F5:  Continue program                 (GOPG)\n"
	  "F6:  Continue up to end of subroutine (GOPG.U)\n"
	  "F7:  Disassemble at PC                (UNAS PC)\n"
	  "F10: Step Over                        (NEXT)\n"
	  "F11: Step                             (STEP)\n\n");
#endif
  do {
    Splt.UpdateSplit();
    token = ReadLine("Monitor > ");
    if (token) {
      if (*token) {
	ParseCmd(token);
      }
    } else {
      // ReadLine failed, bail out fast
      abort = true;
    }
  } while(!abort);
  //
  // Make sure we don't re-enter the monitor immediately
  machine->LaunchMonitor() = false;
  machine->Display()->EnforceFullRefresh();    
  // Switch the monitor to background so we see the emulator going
  machine->Display()->SwitchScreen(true);
  curses                   = NULL;
}
///

/// Monitor::UnknownESC
// Enter the monitor because we found an unhandle-able ESC code
void Monitor::UnknownESC(UBYTE code)
{
#ifdef MUST_OPEN_CONSOLE
  machine->Display()->SwitchScreen(false);
  OpenConsole();
#endif
  CursesWindow win;
  curses = &win;
  
  Print("\n\n*** found unknown ESCape code #$%02x at $%04x\n"
	"entering the monitor. You should possibly reset the\n"
	"emulator with the RSET command.\n",
	code,cpu->PC()-2); // the CPU advanced the PC already
  MainLoop();
#ifdef MUST_OPEN_CONSOLE
  if (!fetchtrace) {
	machine->Display()->SwitchScreen(true);
	CloseConsole();
  }
#endif
}
///

/// Monitor::Crash
// Enter the monitor because we found an unstable and unreliable opcode
// that cannot be emulated savely
void Monitor::Crash(UBYTE code)
{
#ifdef MUST_OPEN_CONSOLE
  machine->Display()->SwitchScreen(false);
  OpenConsole();
#endif
  CursesWindow win;
  curses = &win;
  
  Print("\n\n*** found unreliable opcode #$%02x at $%04x\n"
	"entering the monitor.  You should possibly reset the\n"
	"emulator with the RSET command.\n",
	code,cpu->PC());
  MainLoop();
#ifdef MUST_OPEN_CONSOLE
  if (!fetchtrace) {
	machine->Display()->SwitchScreen(true);
	CloseConsole();
  }
#endif
}
///

/// Monitor::Jam
// Enter the monitor because we found a JAM opcode that is not
// an ESC opcode
void Monitor::Jam(UBYTE code)
{
#ifdef MUST_OPEN_CONSOLE
  machine->Display()->SwitchScreen(false);
  OpenConsole();
#endif
  CursesWindow win;
  curses = &win;
  
  Print("\n\n*** found HALT opcode #$%02x at $%04x\n"
	"entering the monitor.  You should possibly reset the\n"
	"emulator with the RSET command.\n",
	code,cpu->PC());
  MainLoop();
#ifdef MUST_OPEN_CONSOLE
  if (!fetchtrace) {
	machine->Display()->SwitchScreen(true);
	CloseConsole();
  }
#endif
}
///

/// Monitor::EnterMonitor
void Monitor::EnterMonitor(void)
{
#ifdef MUST_OPEN_CONSOLE
  machine->Display()->SwitchScreen(false);
  OpenConsole();
#endif
  CursesWindow win;
  curses = &win;
  
  Step.CloseDisplay();
  Print("\nEntering monitor\n");
  MainLoop();
#ifdef MUST_OPEN_CONSOLE
  if (!fetchtrace) {
	machine->Display()->SwitchScreen(true);
	CloseConsole();
  }
#endif
}
///

/// Monitor::CapturedBreakPoint
// Enter the monitor because we detected a breakpoint. Arguments
// are the breakpoint number and the PC
void Monitor::CapturedBreakPoint(int,ADR pc)
{
#ifdef MUST_OPEN_CONSOLE
  machine->Display()->SwitchScreen(false);
  OpenConsole();
#endif
  CursesWindow win;
  curses = &win;

  if (Step.RefreshLine(cpuspace,cpu->PC())) {
    if (!Step.MainLoop())
      MainLoop();
  } else {
    Print("\nBreakpoint hit at $%04x.\n",pc);
    MainLoop();
  }
  curses = NULL;
#ifdef MUST_OPEN_CONSOLE
  if (!fetchtrace) {
	machine->Display()->SwitchScreen(true);
	CloseConsole();
  }
#endif
}
///

/// Monitor::CapturedWatchPoint
// Enter the monitor because we detected a watchpoint. Arguments
// are the watchpoint number and the address
void Monitor::CapturedWatchPoint(int,ADR mem)
{
#ifdef MUST_OPEN_CONSOLE
  machine->Display()->SwitchScreen(false);
  OpenConsole();
#endif
  CursesWindow win;
  curses = &win;

  if (Step.RefreshLine(cpuspace,cpu->PC())) {
    if (!Step.MainLoop())
      MainLoop();
  } else {
    Print("\nWatchpoint hit at $%04x.\n",mem);
    MainLoop();
  }
  curses = NULL;
#ifdef MUST_OPEN_CONSOLE
  if (!fetchtrace) {
	machine->Display()->SwitchScreen(true);
	CloseConsole();
  }
#endif
}
///

/// Monitor::CapturedTrace
// Enter the monitor because of software tracing. Argument is the
// current PC
void Monitor::CapturedTrace(ADR)
{
  //
  // If tracing is enabled, write the traced output
  // to the trace file.
  if (tracefile) {
    char buffer[80];
    char pstring[9];

    UnAs.DisassembleLine(cpuspace,cpu->PC(),buffer);
    CPUFlags(pstring); 
    fprintf(tracefile,"%-32s;A:%02x X:%02x Y:%02x S:%02x P:%02x=%s XPos:%3d YPos:%3d\n",buffer,
	    cpu->A(),cpu->X(),cpu->Y(),cpu->S(),cpu->P(),pstring,cpu->CurrentXPos(),
	    machine->Antic()->CurrentYPos());
  }
  
  if (fetchtrace) {
#ifdef MUST_OPEN_CONSOLE
    machine->Display()->SwitchScreen(false);
    OpenConsole();
#endif
    struct CursesWindow win;
    char buffer[80];
    
    curses = &win;
    
    if (tracefile == NULL)
      cpu->DisableTrace();
    fetchtrace = false;
    cpu->DisableStack(); 
    cpu->DisablePC();
    if (Step.RefreshLine(cpuspace,cpu->PC())) {
      if (!Step.MainLoop())
	MainLoop();
    } else {
      UnAs.DisassembleLine(cpuspace,cpu->PC(),buffer);
      Print("%s\n",buffer);
      PrintCPUStatus();
      MainLoop(false);
    }
    curses = NULL;
#ifdef MUST_OPEN_CONSOLE
	if (!fetchtrace) {
		machine->Display()->SwitchScreen(true);
		CloseConsole();
	}
#endif
  }
}
///

/// Monitor::Command::Print
void Monitor::Command::Print(const char *fmt,...)
{  
  va_list args;
  va_start(args,fmt);
  monitor->VPrint(fmt,args);
  va_end(args);
}
///

/// Monitor::CursesWindow::~CursesWindow
// Leave the monitor screen alone and disable the curses output here
Monitor::CursesWindow::~CursesWindow(void)
{
#ifdef USE_CURSES
  if (window) {  
    nocbreak();
    echo();
    endwin();
    window = NULL;
  }
#endif
}
///

/// Monitor::CursesWindow::CursesWindow
// Init monitor output
Monitor::CursesWindow::CursesWindow(void)
{
#ifdef USE_CURSES 
  window = initscr();
  clearok((WINDOW *)window,true); 
  curs_set(1);
  nl();
  noecho();
  cbreak();
  refresh();
  keypad(((WINDOW *)window),true);
  scrollok((WINDOW*)window,true);
  idlok((WINDOW*)window,true);
  halfdelay(5); // enforce refresh every half second
#endif
}
///
