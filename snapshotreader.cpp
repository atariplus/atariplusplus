/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: snapshotreader.cpp,v 1.19 2015/05/21 18:52:43 thor Exp $
 **
 ** This class implements the snapshot interface for reading a snapshot
 ** back from a file.
 **********************************************************************************/

/// Includes
#include "exceptions.hpp"
#include "snapshotreader.hpp"
#include "string.hpp"
#include "stdio.hpp"
#include "new.hpp"
#include <stdarg.h>
#include <ctype.h>
///

/// SnapShotReader::SnapShotReader
SnapShotReader::SnapShotReader(void)
  : File(NULL), CurrentTopic(NULL), Collecting(false)
{
}
///

/// SnapShotReader::~SnapShotReader
SnapShotReader::~SnapShotReader(void)
{
  struct Topic *topic;
  //
  CloseFile();
  // Now dispose all topics, one after another.
  do {
    topic = Topics.RemHead();
    if (topic)  delete topic;
  } while(topic);
}
///

/// SnapShotReader::Option::Option
// Create an option base type and install the options
SnapShotReader::Option::Option(const char *name,OptionType type)
  : Name(new char[strlen(name) + 1]), Type(type)
{
  strcpy(Name,name);
}
///

/// SnapShotReader::Option::~Option
SnapShotReader::Option::~Option(void)
{
  delete[] Name;
}
///

/// SnapShotReader::BooleanOption::BooleanOption
// Create a boolean snapshot option
SnapShotReader::BooleanOption::BooleanOption(const char *name,bool value)
  : Option(name,Boolean), Value(value)
{ }
///

/// SnapShotReader::BooleanOption::~BooleanOption
SnapShotReader::BooleanOption::~BooleanOption(void)
{
}
///

/// SnapShotReader::BooleanOption::Parse
// Parse off the boolean option from the argument line
void SnapShotReader::BooleanOption::Parse(const char *value)
{
  if (!MatchesBool(value,Value))
    Throw(InvalidParameter,"SnapShotReader::BooleanOption::Parse",
	  "invalid boolean value in the snapshot file");
}
///

/// SnapShotReader::NumericOption::NumericOption
// create a numeric option
SnapShotReader::NumericOption::NumericOption(const char *name,LONG min,LONG max,LONG value)
  : Option(name,Numeric), Value(value), Min(min), Max(max)
{
}
///

/// SnapShotReader::NumericOption::~NumericOption
SnapShotReader::NumericOption::~NumericOption(void)
{
}
///

/// SnapShotReader::NumericOption::Parse
// Parse off the numeric option from the argument line
void SnapShotReader::NumericOption::Parse(const char *value)
{
  LONG tmp = Value;
  
  if (!MatchesLong(value,tmp))
    Throw(InvalidParameter,"SnapShotReader::NumericOption::Parse",
	  "invalid numeric value in the snapshot file");
  //
  // Check whether the value is in range. Ingore if it is not the
  // case: This might be left over from a previous snapshot with
  // a different configuration.
  if (tmp >= Min && tmp <= Max)
    Value = tmp;
}
///

/// SnapShotReader::StringOption::StringOption
// Create a new string option
SnapShotReader::StringOption::StringOption(const char *name,const char *value)
  : Option(name,String), Value(new char[strlen(value) + 1])
{
  strcpy(Value,value);
}
///

/// SnapShotReader::StringOption::~StringOption
SnapShotReader::StringOption::~StringOption(void)
{
  delete[] Value;
}
///

/// SnapShotReader::StringOption::Parse
// Parse off a string option from the argument value
void SnapShotReader::StringOption::Parse(const char *value)
{
  delete[] Value;
  Value = NULL;
  // Re-allocate with a new length.
  Value = new char[strlen(value) + 1];
  strcpy(Value,value);
}
///

/// SnapShotReader::FileOption::FileOption
// Create a new file option
SnapShotReader::FileOption::FileOption(const char *name,const char *value)
  : Option(name,String), Value(new char[strlen(value) + 1])
{
  strcpy(Value,value);
}
///

/// SnapShotReader::FileOption::~FileOption
SnapShotReader::FileOption::~FileOption(void)
{
  delete[] Value;
}
///

/// SnapShotReader::FileOption::Parse
// Parse off a string option from the argument value
void SnapShotReader::FileOption::Parse(const char *value)
{
  delete[] Value;
  Value = NULL;
  // Re-allocate with a new length.
  Value = new char[strlen(value) + 1];
  strcpy(Value,value);
}
///

/// SnapShotReader::SelectionOption::SelectionOption
// Make a copy of the selection vector for private
// purposes.
SnapShotReader::SelectionOption::SelectionOption(const char *name,const struct SelectionVector *selections,LONG v)
  : Option(name,Selection), Value(v), Selections(NULL)
{
  const struct SelectionVector *sv;
  struct SelectionVector *si;
  LONG cnt;
  //
  // We need to make a copy of the selection vector here. For that, count how many we need.
  sv = selections, cnt = 1;
  while(sv->Name) {
    cnt++,sv++;
  }
  try {
    Selections = new struct SelectionVector[cnt];
    // Clear all names such that we know what to dispose and what not.
    memset(Selections,0,sizeof(struct SelectionVector) * cnt);
    //
    sv = selections,si = Selections;
    while(sv->Name) {
      char *newname;
      newname = new char[strlen(sv->Name) + 1];
      strcpy(newname,sv->Name);
      si->Name  = newname;
      si->Value = sv->Value;
      si++,sv++;
    }
    // The last name is already cleared.
  } catch(...) {
    // Dispose the temporaries here.
    if ((si = Selections)) {
      while(si->Name) {
	delete[] TOCHAR(si->Name);
	si++;
      }
      delete[] Selections;
      Selections = NULL;
    }
    throw;
  } 
}
///

/// SnapShotReader::SelectionOption::~SelectionOption
SnapShotReader::SelectionOption::~SelectionOption(void)
{
  struct SelectionVector *si;
 
  // Dispose all selection vectors and the names allocated for them
  if ((si = Selections)) {
    while(si->Name) {
      delete[] TOCHAR(si->Name);
      si++;
    }
    delete[] Selections;
  }
}
///

/// SnapShotReader::SelectionOption::Parse
// Parse off a value for the selection vector
void SnapShotReader::SelectionOption::Parse(const char *value)
{
  const struct SelectionVector *sv;
  // 
  sv = Selections;
  // Go thru all selections and check whether we find a matching option.
  // If not, then ignore the value. We might find an unsupported selection
  // in case the program configuration changed in between.
  while(sv->Name) {
    if (!strcasecmp(sv->Name,value)) {
      // Found a match. Enter this as value.
      Value = sv->Value;
      break;
    }
    sv++;
  }
}
///

/// SnapShotReader::BlockOption::BlockOption
// Build a new block option
SnapShotReader::BlockOption::BlockOption(const char *name,const UBYTE *value,size_t size)
  : Option(name,Block), Value(new UBYTE[size]), Size(size)
{ 
  memcpy(Value,value,size);
}
///

/// SnapShotReader::BlockOption::~BlockOption
SnapShotReader::BlockOption::~BlockOption(void)
{
  delete[] Value;
}
///

/// SnapShotReader::BlockOption::Parse
// Parse the definition of the block in the file
void SnapShotReader::BlockOption::Parse(FILE *file)
{
  size_t cnt = Size;
  UBYTE *mem = Value;
  char line[512];
  char *p;
  int c,len;
  //
  while(cnt && !feof(file)) {
    // First check the next character. If this is a '+', then the block got aborted prematularely
    // and we better put it back.
    c = fgetc(file);
    ungetc(c,file);
    if (c == -1 || c == '+') break;
    // Read another line into the buffer.
    errno = 0;
    if (fgets(line,512,file) == NULL) {
      if (errno) {
	// A failure. Generate an IO error. This should not happen as this file is
	// machine-generated
	ThrowIo("SnapShotReader::BlockOption::Parse","failed to read a line from the snapshot file");
      }
      break;
    }
    len = (int)strlen(line);
    // Check for overflow,
    if (len >= 512) {
      Throw(OutOfRange,"SnapShotReader::BlockOption::Parse","snapshot line too LONG");
    }
    // Check whether this line is a comment line. If it is, then ignore it
    // and continue.
    // Check whether this is a comment or an empty line. If so, skip
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
      continue;
    //
    // Interpret the line contents as pairs of hex digits. Blanks are not allowed, only line feeds
    // as the last separating character of a full line.
    p = line;
    while(*p && *p != '\n' && *p != '\r') {
      int x1,x2;
      // Convert this to upper case for easier conversion
      x1 = toupper(p[0]);
      x2 = toupper(p[1]);
      if (!(isxdigit(x1) && isxdigit(x2))) {
	// found a non-hex digit in the configuration. If that happens, then the line is damaged and
	// we better return.
	Throw(InvalidParameter,"SnapShotReader::BlockOption::Parse",
	      "found invalid hex digit in the block definition input line");
      }
      // Now extract the number from the digits.
      x1   = (x1 >= 'A')?(x1 - 'A' + 10):(x1 - '0');
      x2   = (x2 >= 'A')?(x2 - 'A' + 10):(x2 - '0');
      *mem = UBYTE((x1<<4)|x2);
      mem++;
      cnt--;
      p += 2;
    }
  }
  if (cnt) {
    Throw(InvalidParameter,"SnapShotReader::BlockOptions::Parse",
	  "premature EOF while parsing a block parameter");
  }
}
///

/// SnapShotReader::Topic::Topic
// Create a new topic from a name
SnapShotReader::Topic::Topic(const char *name)
  : Name(new char[strlen(name) + 1])
{
  strcpy(Name,name);
}
///

/// SnapShotReader::Topic::~Topic
SnapShotReader::Topic::~Topic(void)
{
  struct Option *option;
  // Dispose all options within this topic
   delete[] Name;
  do {
    option = Options.RemHead();
    if (option) delete option;
  } while(option);
}
///

/// SnapShotReader::Topic::FindOption
// Find an option by name within the list of options, ignore case.
struct SnapShotReader::Option *SnapShotReader::Topic::FindOption(const char *name)
{
  struct Option *option;
  option = Options.First();
  while(option) {
    if (!strcasecmp(option->Name,name))
      return option;
    option = option->NextOf();
  }
  return NULL;
}
///

/// SnapShotReader::FindTopic
struct SnapShotReader::Topic *SnapShotReader::FindTopic(const char *name)
{
  struct Topic *topic;

  topic = Topics.First();
  while(topic) {
    if (!strcasecmp(topic->Name,name))
      return topic;
    topic = topic->NextOf();
  }
  return NULL;
}
///

/// SnapShotReader::OpenFile
// Open a snapshot file for reading
void SnapShotReader::OpenFile(const char *pathname)
{
#if CHECK_LEVEL > 0
  if (File)
    Throw(ObjectExists,"SnapShotReader::OpenFile",
	  "the snapshot file is already open");
#endif
  Collecting = true;
  File = fopen(pathname,"r");
  if (File == NULL)
    ThrowIo("SnapShotReader::OpenFile","unable to open the snapshot file");
}
///

/// SnapShotReader::CloseFile
// Dispose the file and all collected data items
void SnapShotReader::CloseFile(void)
{
  if (File) {
    fclose(File);
    File = NULL;
  }
}
///

/// SnapShotReader::Parse
// Parse the data in the file: Run this after having collected all the
// data items we need to assign the file contents to the data items.
void SnapShotReader::Parse(void)
{
  size_t len;
  char line[512];     // line buffer
  char *sep,*end,*topicname,*optionname,*value;
  struct Topic *topic;
  struct Option *option;
  //
  // Check whether we are still collecting. If not, then this method
  // should not have been called.
#if CHECK_LEVEL > 0
  if (!Collecting) {
    Throw(InvalidParameter,"SnapShotReader::Parser",
	  "the file has been parsed already");
  }
#endif
  //
  Collecting = false;
  //
  // Get the next line.
  while(!feof(File)) {
    errno = 0;
    if (fgets(line,512,File) == NULL) {
      if (errno) {
	// A failure. Generate an IO error. This should not happen as this file is
	// machine-generated
	ThrowIo("SnapShotReader::Parse","failed to read a line from the snapshot file");
      }
      break;
    }
    len = strlen(line);
    // Check for overflow,
    if (len >= 512) {
      Throw(OutOfRange,"SnapShotReader::Parse","snapshot line too LONG");
    }
    // Check whether this is a comment or an empty line. If so, skip
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
      continue;
    //
    // Check whether we are at the beginning of a new argument.
    if (line[0] != '+') {
      // We are not. Phew, something is wrong, maybe because we are in an argument
      // type we don't know about. Just ignore this and continue.
      continue;
    }
    //
    // Scan for the "::" separator of topic and option.
    sep = strchr(line,':');
    if (sep == NULL || sep[1] != ':') {
      Throw(InvalidParameter,"ShapShotReader::Parse","invalid option in snapshot file");
    }
    *sep       = '\0';
    // The name of the topic starts directly behind the '+' sign.
    // The option starts behind the "::" string.
    topicname  = line + 1;
    optionname = sep  + 2;
    end        = sep  - 1;
    // Remove leading spaces here.
    while(*topicname && isspace(*topicname))
      topicname++;
    // Remove trailing spaces
    while(end >= topicname && isspace(*end))
      *end-- = '\0';
    //
    sep = strchr(optionname,'=');
    if (sep == NULL) {
      Throw(InvalidParameter,"SnapShotReader::Parse","missing '=' sign in snapshot option");
    }
    *sep       = '\0';
    // Now scan for the '=' sign separating the option from the value.
    value      = sep + 1;
    end        = sep - 1;
    // Remove leading spaces from the optionname.
    while(*optionname && isspace(*optionname))
      optionname++;
    // Remove trailing spaces
    while(end >= optionname && isspace(*end))
      *end-- = '\0';
    //
    while(*value && isspace(*value))
      value++;
    // Now advance towards the end of the value
    end = value;
    while(*end)
      end++;
    // Now remove trailing blanks
    do {
      *end = '\0';
      end--;
    } while(end >= value && isspace(*end));
    //
    // Scan for the mentioned topic. If we don't find it, then this option does not apply for
    // the current configuration and we ignore it.
    topic  = FindTopic(topicname);
    if (topic == NULL)
      continue;
    option = topic->FindOption(optionname);
    if (option == NULL)
      continue;
    //
    // The parsing of the following line depends on the type of the option.
    switch(option->Type) {
    case Option::Boolean:
      ((struct BooleanOption *)option)->Parse(value);
      break;
    case Option::Numeric:
      ((struct NumericOption *)option)->Parse(value);
      break;
    case Option::String:
      ((struct StringOption *)option)->Parse(value);
      break;
    case Option::File:
      ((struct FileOption *)option)->Parse(value);
      break;
    case Option::Selection:
      ((struct SelectionOption *)option)->Parse(value);
      break;
    case Option::Block:
      ((struct BlockOption *)option)->Parse(File);
      break;
    }
  }
}
///

/// SnapShotReader::PrintError
// Generate a parsing error of the snapshot file with the
// indicated message.
void SnapShotReader::PrintError(const char *msg,...)
{
  va_list args;
  char buffer[256];

  va_start(args,msg);
  vsnprintf(buffer,255,msg,args);
  Throw(BadSnapShot,"SnapShotReader::PrintError",buffer);
  va_end(args);
}
///

/// SnapShotReader::DefineTitle
// Define the module this and the following arguments
// are good for.
void SnapShotReader::DefineTitle(const char *title)
{
  struct Topic *topic;
  //
  if (Collecting) {
    // We are in the collection phase: Hence, we build up the list of topics and options.
    // This means specifically that the option we're going to define must not yet exist
    // in here.
    topic = FindTopic(title);
    if (topic) {
      // Something's wrong here!
      Throw(ObjectExists,"SnapShotReader::DefineTitle",
	    "duplicate topic detected");
    }
    Topics.AddHead(topic = new struct Topic(title));
    // Note this topic for the following
    CurrentTopic = topic;
  } else {
    // Otherwise, the topic must have been recorded in the previous collection phase
    topic = FindTopic(title);
    if (topic == NULL) {
      Throw(ObjectDoesntExist,"SnapShotReader::DefineTitle",
	    "unknown topic requested for parsing");
    }
    // Note this as well.
    CurrentTopic = topic;
  }
}
///

/// SnapShotReader::DefineBool
// Define a boolean argument, resp. parse it.
void SnapShotReader::DefineBool(const char *argname,const char *,bool &var)
{
  struct Option *option;
#if CHECK_LEVEL > 0
  if (CurrentTopic == NULL) {
    Throw(ObjectDoesntExist,"SnapShotReader::DefineBool",
	  "no current topic active to install option into");
  }
#endif
  if (Collecting) {
    // Option build-up. This option must not yet exist.
    option = CurrentTopic->FindOption(argname);
    if (option) {
      Throw(ObjectExists,"SnapShotReader::DefineBool",
	    "duplicate option definition detected");
    }
    CurrentTopic->Options.AddHead(new struct BooleanOption(argname,var));
  } else {
    // Option definition: Read it from the collected issue.
    option = CurrentTopic->FindOption(argname);
    if (option == NULL) {
      Throw(ObjectExists,"SnapShotReader::DefineBool",
	    "unknown option requested in build-up phase");
    }
    if (option->Type != Option::Boolean) {
      Throw(PhaseError,"SnapShotReader::DefineBool",
	    "collected option is not boolean");
    }
    var = ((struct BooleanOption *)option)->Value;
  }
}
///

/// SnapShotReader::DefineString
// Define a string argument, resp. parse it. This function
// releases the old string and allocate a new one.
void SnapShotReader::DefineString(const char *argname,const char *,char *&var)
{
  struct Option *option;
  char *value;
#if CHECK_LEVEL > 0
  if (CurrentTopic == NULL) {
    Throw(ObjectDoesntExist,"SnapShotReader::DefineString",
	  "no current topic active to install option into");
  }
#endif
  if (Collecting) {
    // Option build-up. This option must not yet exist.
    option = CurrentTopic->FindOption(argname);
    if (option) {
      Throw(ObjectExists,"SnapShotReader::DefineString",
	    "duplicate option definition detected");
    }
    CurrentTopic->Options.AddHead(new struct StringOption(argname,var));
  } else {
    // Option definition: Read it from the collected issue.
    option = CurrentTopic->FindOption(argname);
    if (option == NULL) {
      Throw(ObjectExists,"SnapShotReader::DefineString",
	    "unknown option requested in build-up phase");
    }
    if (option->Type != Option::String) {
      Throw(PhaseError,"SnapShotReader::DefineString",
	    "collected option is not of string type");
    }
    // Re-allocate the variable and copy the new value in.
    value = ((struct StringOption *)option)->Value;
    delete[] var;
    var = NULL;
    var = new char[strlen(value) + 1];
    strcpy(var,value);
  }
}
///

/// SnapShotReader::DefineFile
// A specialized version of the above, reads file names.
void SnapShotReader::DefineFile(const char *argname,const char *,char *&var,bool,bool,bool)
{
  struct Option *option;
  char *value;
#if CHECK_LEVEL > 0
  if (CurrentTopic == NULL) {
    Throw(ObjectDoesntExist,"SnapShotReader::DefineFile",
	  "no current topic active to install option into");
  }
#endif
  if (Collecting) {
    // Option build-up. This option must not yet exist.
    option = CurrentTopic->FindOption(argname);
    if (option) {
      Throw(ObjectExists,"SnapShotReader::DefineFile",
	    "duplicate option definition detected");
    }
    CurrentTopic->Options.AddHead(new struct FileOption(argname,var));
  } else {
    // Option definition: Read it from the collected issue.
    option = CurrentTopic->FindOption(argname);
    if (option == NULL) {
      Throw(ObjectExists,"SnapShotReader::DefineFile",
	    "unknown option requested in build-up phase");
    }
    if (option->Type != Option::File) {
      Throw(PhaseError,"SnapShotReader::DefineFile",
	    "collected option is not of file type");
    }
    // Re-allocate the variable and copy the new value in.
    value = ((struct FileOption *)option)->Value;
    delete[] var;
    var = NULL;
    var = new char[strlen(value) + 1];
    strcpy(var,value);
  }
}
///

/// SnapShotReader::DefineLong
// Define a LONG argument and its range given by min and max.
// Both are inclusive.
void SnapShotReader::DefineLong(const char *argname,const char *,LONG min,LONG max,LONG &var)
{
  struct Option *option;
#if CHECK_LEVEL > 0
  if (CurrentTopic == NULL) {
    Throw(ObjectDoesntExist,"SnapShotReader::DefineLong",
	  "no current topic active to install option into");
  }
#endif
  if (Collecting) {
    // Option build-up. This option must not yet exist.
    option = CurrentTopic->FindOption(argname);
    if (option) {
      Throw(ObjectExists,"SnapShotReader::DefineLong",
	    "duplicate option definition detected");
    }
    CurrentTopic->Options.AddHead(new struct NumericOption(argname,min,max,var));
  } else {
    // Option definition: Read it from the collected issue.
    option = CurrentTopic->FindOption(argname);
    if (option == NULL) {
      Throw(ObjectExists,"SnapShotReader::DefineLong",
	    "unknown option requested in build-up phase");
    }
    if (option->Type != Option::Numeric) {
      Throw(PhaseError,"SnapShotReader::DefineNumeric",
	    "collected option is not numeric");
    }
    var = ((struct NumericOption *)option)->Value;
  }
}
///

/// SnapShotReader::DefineSelection
// Define a radio switch type argument given by an array of options and
// values.
void SnapShotReader::DefineSelection(const char *argname,const char *,const struct SelectionVector selections[],LONG &var)
{
  struct Option *option;
#if CHECK_LEVEL > 0
  if (CurrentTopic == NULL) {
    Throw(ObjectDoesntExist,"SnapShotReader::DefineSelection",
	  "no current topic active to install option into");
  }
#endif
  if (Collecting) {
    // Option build-up. This option must not yet exist.
    option = CurrentTopic->FindOption(argname);
    if (option) {
      Throw(ObjectExists,"SnapShotReader::DefineSelection",
	    "duplicate option definition detected");
    }
    CurrentTopic->Options.AddHead(new struct SelectionOption(argname,selections,var));
  } else {
    // Option definition: Read it from the collected issue.
    option = CurrentTopic->FindOption(argname);
    if (option == NULL) {
      Throw(ObjectExists,"SnapShotReader::DefineSelection",
	    "unknown option requested in build-up phase");
    }
    if (option->Type != Option::Selection) {
      Throw(PhaseError,"SnapShotReader::DefineSelection",
	    "collected option is not a selection");
    }
    // Re-allocate the variable and copy the new value in.
    var = ((struct SelectionOption *)option)->Value;
  }
}
///

/// SnapShotReader::DefineChunk
// This is the only additional method here, required to load/save entire blocks
void SnapShotReader::DefineChunk(const char *argname,const char *,UBYTE *mem,size_t size)
{  
  struct Option *option;
#if CHECK_LEVEL > 0
  if (CurrentTopic == NULL) {
    Throw(ObjectDoesntExist,"SnapShotReader::DefineChunk",
	  "no current topic active to install option into");
  }
#endif
  if (Collecting) {
    // Option build-up. This option must not yet exist.
    option = CurrentTopic->FindOption(argname);
    if (option) {
      Throw(ObjectExists,"SnapShotReader::DefineChunk",
	    "duplicate option definition detected");
    }
    CurrentTopic->Options.AddHead(new struct BlockOption(argname,mem,size));
  } else {
    // Option definition: Read it from the collected issue.
    option = CurrentTopic->FindOption(argname);
    if (option == NULL) {
      Throw(ObjectExists,"SnapShotReader::DefineChunk",
	    "unknown option requested in build-up phase");
    }
    if (option->Type != Option::Block) {
      Throw(PhaseError,"SnapShotReader::DefineChunk",
	    "collected option is not a chunk");
    }
    if (((struct BlockOption *)option)->Size != size) {
      Throw(PhaseError,"SnapShotReader::DefineChunk",
	    "chunk sizes do not match");
    }
    // Now copy the new definitions back into the user space.
    memcpy(mem,((struct BlockOption *)option)->Value,size);
  }
}
///

