/***********************************************************************************
 **
 ** Atari++ emulator (c) 2002 THOR-Software, Thomas Richter
 **
 ** $Id: snapshotwriter.cpp,v 1.10 2015/05/21 18:52:43 thor Exp $
 **
 ** This class implements the snapshot interface for writing a snapshot
 ** out to a file.
 **********************************************************************************/

/// Includes
#include "types.h"
#include "exceptions.hpp"
#include "snapshotwriter.hpp"
#include "new.hpp"
#include "stdio.hpp"
#include <stdarg.h>
///

/// SnapShotWriter::SnapShotWriter
SnapShotWriter::SnapShotWriter(void)
  : File(NULL), FileName(NULL), CurrentTopic(NULL)
{
}
///

/// SnapShotWriter::~SnapShotWriter
SnapShotWriter::~SnapShotWriter(void)
{
  if (File) {
    fclose(File);
    // If we have to dispose the file here, something must have went wrong.
    // Hence, dispose the file here.
    remove(FileName);
  }
  delete[] CurrentTopic;
}
///

/// SnapShotWriter::OpenFile
// Open the output file of the snapshot writer
void SnapShotWriter::OpenFile(const char *filename)
{
#if CHECK_LEVEL > 0
  if (File || FileName)
    Throw(ObjectExists,"SnapShotWriter::OpenFile",
	  "the snapshot output file is already open");
#endif
  File = fopen(filename,"w"); // This is a text file.
  if (File) {
    FileName = filename;
  } else {
    // Generate an IO error
    ThrowIo("SnapShotWriter::OpenFile","unable to open the snapshot output file");
  }
  fprintf(File,"#\n"
	  "#Atari++ generated snapshot file. Syntax of this file is as follows:\n"
	  "#Each snapshot item starts with a + sign, followed by the object creating\n"
	  "#the snapshot, a double colon, and the setting defined by the data\n"
	  "#and an = sign separating the object from its setup.\n"
	  "#Comment lines start with a hash-mark, and empty lines are ignored.\n"
	  "#\n");
}
///

/// SnapShotWriter::CloseFile
// Close the snapshot file and signal that everything went fine
void SnapShotWriter::CloseFile(void)
{
  if (File) {
    fclose(File);
    File = NULL;
  }
  // Forget the file name. This has the effect that we can re-open this object
  // and we do not delete the file if we delete this class.
  FileName = NULL;
}
///

/// SnapShotWriter::PrintError
// Generate a parsing error of the snapshot file with the
// indicated message.
void SnapShotWriter::PrintError(const char *msg,...)
{
  va_list args;
  char buffer[256];
  //
  // Print the error into the buffer
  va_start(args,msg);
  vsnprintf(buffer,255,msg,args);
  Throw(BadSnapShot,"SnapShotWriter::PrintError",buffer);
  va_end(args);
}
///

/// SnapShotWriter::DefineTitle
// Define the module this and the following arguments
// are good for.
void SnapShotWriter::DefineTitle(const char *title)
{
#if CHECK_LEVEL > 0
  if (File == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineTitle",
	  "snapshot output file has not been opened yet");
#endif
  //
  delete[] CurrentTopic;
  CurrentTopic = NULL;
  // Build a new currenttopic string.
  CurrentTopic = new char[strlen(title) + 1];
  strcpy(CurrentTopic,title);
  fprintf(File,"#\n#\n"
	  "################################################################\n"
	  "# %s specific settings follow:\n"
	  "################################################################\n",
	  title);

}
///

/// SnapShotWriter::DefineBool
// Define a boolean argument, resp. parse it.
void SnapShotWriter::DefineBool(const char *argname,const char *help,bool &var)
{
#if CHECK_LEVEL > 0
  if (File == NULL || CurrentTopic == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineBool",
	  "snapshot file or title missing");
#endif
  fprintf(File,"#%s (boolean)\n"
	  "+%s::%s = %s\n",
	  help,CurrentTopic,argname,var?("on"):("off"));
}
///

/// SnapShotWriter::DefineString
// Define a string argument, resp. parse it. This function
// releases the old string and allocate a new one.
void SnapShotWriter::DefineString(const char *argname,const char *help,char *&var)
{
#if CHECK_LEVEL > 0
  if (File == NULL || CurrentTopic == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineString",
	  "snapshot file or title missing");
#endif
  fprintf(File,"#%s (string)\n"
	  "+%s::%s = %s\n",
	  help,CurrentTopic,argname,var);
}
///

/// SnapShotWriter::DefineFile
// A specialized version of the above, reads file names.
void SnapShotWriter::DefineFile(const char *argname,const char *help,char *&var,
				bool,bool,bool)
{
#if CHECK_LEVEL > 0
  if (File == NULL || CurrentTopic == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineString",
	  "snapshot file or title missing");
#endif
  fprintf(File,"#%s (pathname)\n"
	  "+%s::%s = %s\n",
	  help,CurrentTopic,argname,var);
}
///

/// SnapShotWriter::DefineLong
// Define a LONG argument and its range given by min and max.
// Both are inclusive.
void SnapShotWriter::DefineLong(const char *argname,const char *help,
				LONG min,LONG max,LONG &var)
{
#if CHECK_LEVEL > 0
  if (File == NULL || CurrentTopic == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineLong",
	  "snapshot file or title missing");
#endif
  fprintf(File,"#%s (numeric between " LD " and " LD ")\n"
	  "+%s::%s = " LD "\n",
	  help,min,max,CurrentTopic,argname,var);
}
///

/// SnapShotWriter::DefineSelection
// Define a radio switch type argument given by an array of options and
// values.
void SnapShotWriter::DefineSelection(const char *argname,const char *help,
				     const struct SelectionVector selections[],
				     LONG &var)
{
  const struct SelectionVector *sv;
  bool writebar;

#if CHECK_LEVEL > 0
  if (File == NULL || CurrentTopic == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineSelection",
	  "snapshot file or title missing");
#endif
  fprintf(File,"#%s (one of ",help);
  // Now write the possible selections out
  writebar = false;
  sv       = selections;
  while(sv->Name) {
    fprintf(File,"%c%s",writebar?('|'):('\"'),sv->Name);
    writebar = true;
    sv++;
  }
  //
  // Now write the current selection
  sv       = selections;
  while(sv->Name) {
    if (sv->Value == var) {
      fprintf(File,"\")\n"
	      "+%s::%s = %s\n",
	      CurrentTopic,argname,sv->Name);
    }
    sv++;
  }
}
///

/// SnapShotWriter::DefineChunk
// This is the only additional method here, required to load/save entire blocks
void SnapShotWriter::DefineChunk(const char *argname,const char *help,UBYTE *mem,size_t size)
{
  int column = 0;
  
#if CHECK_LEVEL > 0
  if (File == NULL || CurrentTopic == NULL)
    Throw(ObjectDoesntExist,"SnapShotWriter::DefineSelection",
	  "snapshot file or title missing");
#endif
  fprintf(File,"#%s (raw memory contents in hex)\n"
	  "+%s::%s = \n",
	  help,CurrentTopic,argname);
  do {   
    if (column >= 40) {
      fprintf(File,"\n");
      column = 0;
    }
    fprintf(File,"%02x",*mem);
    mem++,column++;
  }while(--size);
  fprintf(File,"\n");
}
///

