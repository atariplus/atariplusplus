/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: snapshotwriter.hpp,v 1.7 2015/05/21 18:52:43 thor Exp $
 **
 ** This class implements the snapshot interface for writing a snapshot
 ** out to a file.
 **********************************************************************************/

#ifndef SNAPSHOTWRITER_HPP
#define SNAPSHOTWRITER_HPP

/// Includes
#include "types.hpp"
#include "string.hpp"
#include "argparser.hpp"
#include "snapshot.hpp"
#include "stdio.hpp"
///

/// Class SnapShotWriter
// This class extends the argument parser by a method that loads and saves
// entire blocks of continuous data.
class SnapShotWriter : public SnapShot {
  //
  // The file we're writing to. This is ours, and we close it
  // on exit.
  FILE       *File;
  //
  // The file name we save to. We keep this here to be able to remove a partially
  // written file.
  const char *FileName;
  //
  // The currently saved topic. This is modified/created by the DefineTitle method.
  char *CurrentTopic;
  //
public:
  SnapShotWriter(void);
  ~SnapShotWriter(void);
  //   
  // Open the output file of this snapshot: This is where the data
  // goes into.
  void OpenFile(const char *pathname);
  // Complete the snapshot'ing by closing the output file and signal that
  // everything went fine.
  void CloseFile(void);
  // Start parsing data from the snapshot file: This call indicates that the
  // collection of data items to be defined is over and we begin to read the
  // data back from the file into the collection.
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
