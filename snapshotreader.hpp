/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: snapshotreader.hpp,v 1.10 2015/05/21 18:52:43 thor Exp $
 **
 ** This class implements the snapshot interface for reading a snapshot
 ** back from a file.
 **********************************************************************************/

#ifndef SNAPSHOTREADER_HPP
#define SNAPSHOTREADER_HPP

/// Includes
#include "types.hpp"
#include "string.hpp"
#include "argparser.hpp"
#include "snapshot.hpp"
#include "list.hpp"
#include "stdio.hpp"
///

/// Class SnapShotReader
// This class extends the argument parser by a method that loads and saves
// entire blocks of continuous data.
class SnapShotReader : public SnapShot {
  //
  // The stream we read data from. Gets closed on deletion. This is ours!
  FILE *File;
  //  
  // This structure defines a single option within the options of a single
  // topic. We derive from this structure to identify the individual types.
  struct Option : public Node<struct Option> {
    // The name of this option
    char *Name;
    //
    // The type of this option
    enum OptionType {
      Boolean,             // a simple yes/no decision
      Numeric,             // a numerical range,
      String,              // a string type,
      File,                // a file specification/location
      Selection,           // radio-button like "one out of several" option
      Block                // a block of data
    }   Type;
    //
    Option(const char *name,OptionType type);
    virtual ~Option(void);
  };
  //
  // Several derivates of options, selected by type.
  struct BooleanOption : public Option {
    bool Value;           // whether this option is on or off
    //
    BooleanOption(const char *name,bool value);
    ~BooleanOption(void);
    //
    // Parse off the value from the argument.
    void Parse(const char *value);
  };
  struct NumericOption : public Option {
    LONG Value;          // the current setting of this option
    LONG Min;            // minimal allowed value,
    LONG Max;            // maximal allowed value
    //
    NumericOption(const char *name,LONG min,LONG max,LONG value);
    ~NumericOption(void);    
    // Parse off the value from the argument.
    void Parse(const char *value);
  };
  struct StringOption : public Option {
    char *Value;        // the current setting
    //
    StringOption(const char *name,const char *value);
    ~StringOption(void);
    // Parse off the value from the argument.
    void Parse(const char *value);
  };
  struct FileOption : public Option {
    char *Value;        // the current setting
    //
    FileOption(const char *name,const char *value);
    ~FileOption(void);
    // Parse off the value from the argument.
    void Parse(const char *value);
  };
  struct SelectionOption : public Option {
    LONG Value;        // the current selection
    struct SelectionVector *Selections; // Possibilities concering the selections
    //
    SelectionOption(const char *name,const struct SelectionVector *selections,LONG value);
    ~SelectionOption(void);
    // Parse off the value from the argument.
    void Parse(const char *value);
  };
  struct BlockOption : public Option {
    UBYTE *Value;      // the current setting
    size_t Size;       // size of data to collect
    //
    BlockOption(const char *name,const UBYTE *value,size_t size);
    ~BlockOption(void);
    //
    // Parse off the value from the file: This uses several lines in the file.
    void Parse(FILE *file);
  };
  //
  // The following structure keeps all the topics we collected so far.
  struct Topic : public Node<struct Topic> {
    // The name of this topic.
    char        *Name;
    //
    // The list of options within this topic.
    List<Option> Options;
    //
    // Construct a topic from a name
    Topic(const char *name);
    ~Topic(void);
    //
    // Find a named option within here by name.
    struct Option *FindOption(const char *name);
  };
  //
  // A list of topics: These lists all the topics we've found so far.
  List<Topic> Topics;
  //
  // The topic that is currently processed.
  struct Topic *CurrentTopic;
  //
  // This boolean is set in case we are just collecting the data types
  // from the "saveables" instead of really installing data.
  bool Collecting;
  // 
  // Find a named topic by name
  struct Topic *FindTopic(const char *name);
  //
public:
  SnapShotReader(void);
  ~SnapShotReader(void);
  //
  // Open the snapshot file for reading, and initialize it. Prepare the
  // snapshot for pre-parsing, i.e. collection of which data is required.
  void OpenFile(const char *pathname);
  // Close the file and signal success.
  void CloseFile(void);
  //
  // Parse the data in the file: Run this after having collected all the
  // data items we need to assign the file contents to the data items.
  void Parse(void);
  //
  // Generate a parsing error of the snapshot file with the
  // indicated message.
  virtual void PrintError(const char *msg,...) PRINTF_STYLE;
  //
  // Argument registering methods borrowed from the argparser.
  //
  // Define the module this and the following arguments
  // are good for.
  virtual void DefineTitle(const char *title);
  //
  // Define a boolean argument, resp. parse it.
  virtual void DefineBool(const char *argname,const char *help,bool &var);
  //
  // Define a string argument, resp. parse it. This function
  // releases the old string and allocate a new one.
  virtual void DefineString(const char *argname,const char *help,char *&var);
  //
  // A specialized version of the above, reads file names.
  virtual void DefineFile(const char *argname,const char *help,char *&var,
			  bool forsave,bool filesonly,bool dirsonly);
  //
  // Define a LONG argument and its range given by min and max.
  // Both are inclusive.
  virtual void DefineLong(const char *argname,const char *help,
			  LONG min,LONG max,LONG &var);
  //
  // Define a radio switch type argument given by an array of options and
  // values.
  virtual void DefineSelection(const char *argname,const char *help,
			       const struct SelectionVector selections[],
			       LONG &var);
  //
  // This is the only additional method here, required to load/save entire blocks
  virtual void DefineChunk(const char *argname,const char *help,UBYTE *mem,size_t size);
  //
};
///

///
#endif
